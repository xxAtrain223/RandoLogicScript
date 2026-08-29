#include "soh_ap.h"

#include <sstream>
#include <string_view>

namespace rls::transpilers::soh_ap {

namespace {

// ============================================================================
// SoH AP World host vocabulary -- the single source of truth for the contract
// between the generated Python and the hand-maintained oot_soh world. Every
// symbol/pattern here is something RLS does NOT derive itself: the world must
// provide it. When that contract changes, this is the one block to edit; the
// render hooks below read from these tables instead of hard-coding names.
//
//   Host calls (renderHostCall):
//     has / flag   -> has_item(bundle, <arg>)        [kHostCallRewrites]
//     trick        -> can_do_trick(bundle, <arg>)    [kHostCallRewrites]
//     check_price  -> can_afford_slot(<arg> | current location)   (bespoke)
//   Binary special cases (renderBinarySpecialCase, [kBinaryRewrites]):
//     check_price <= wallet_capacity   -> check_price (the cap is dropped, so
//                                         the whole expr renders can_afford_slot)
//     collected_triforce_pieces >= required_triforce_pieces -> CanWinTriforceHunt()
//   Defines provided natively (isHostProvidedDefine, [kHostProvidedDefines]):
//     has_bottle, wallet_capacity  -> skipped by function generation
//
// Note on check_price -- it deliberately appears in TWO stages, which is the one
// non-obvious thing here:
//   1. As a kBinaryRewrites row, `check_price(...) <= wallet_capacity(...)` is
//      projected down to just its left operand (empty replacement). The wallet
//      cap is dropped because the host's can_afford_slot already accounts for it.
//   2. That surviving `check_price(...)` is then a plain host call, rewritten by
//      renderHostCall to can_afford_slot(...). check_price is NOT a kHostCallRewrites
//      row: unlike has/flag/trick it takes no bundle and needs RC_UNKNOWN_CHECK
//      argument handling, so it stays a bespoke branch there.
// So a bare `check_price(...)` becomes can_afford_slot via stage 2 alone; the
// `<= wallet_capacity` form needs stage 1 first to strip the cap.
// ============================================================================

// The Python enum class each RLS enum's values belong to in the oot_soh world, keyed by
// the RLS enum name (as declared in the stdlib's `extern enum Item { RG_* }` etc.). An
// enum absent from this table renders its values bare and annotates as unsupported_type:
// Scene, Dungeon and Area have no dedicated world class, as their values only ever reach
// Python through rewrites that consume them (kSmallKeyScenes) or region data.
struct EnumClassMapping {
	std::string_view rlsEnum;
	std::string_view pyClass;
};
constexpr EnumClassMapping kEnumClasses[] = {
	{"Item",     "Items"},
	{"Enemy",    "Enemies"},
	{"Distance", "EnemyDistance"},
	{"Trick",    "Tricks"},
	{"Setting",    "RandomizerSettingKey"},
	// Region/Event/Location cover both the extern enums the stdlib declares and the same-named
	// types sema gives values declared in RLS itself (IdentifierKind::DeclaredValue). Events are
	// materialized as the Events class, built from the regions' event sections. (WaterLevel has
	// no row: it is a normal RLS enum, so writeEnums generates a WaterLevel class and
	// enumClassName falls back to that name.)
	{"Region",     "Regions"},
	{"Event",      "Events"},
	{"Location",   "Locations"},
	{"Trial",      "TrialKey"},
};

// has/flag/trick all rewrite to `<helper>(bundle, <first arg>)`. check_price is
// not here: it needs bespoke RC_UNKNOWN_CHECK handling and takes no bundle.
struct HostCallRewrite {
	std::string_view rlsCallee;
	std::string_view pyHelper;
};
constexpr HostCallRewrite kHostCallRewrites[] = {
	{"has", "has_item"},
	{"flag", "has_item"},
	{"trick", "can_do_trick"},
};

// A binary comparison `<leftCallee>(...) <op> <rightCallee>(...)` that the world
// collapses to a single rule. The replacement is either:
//   - a fixed string  -> emit it verbatim (the whole comparison is one host rule), or
//   - empty ("")       -> project to the left operand: emit GenerateExpression(left)
//                         and discard the right. Used when the right side is a host
//                         cap/threshold the surviving left call already folds in.
struct BinaryRewrite {
	rls::ast::BinaryOp op;
	std::string_view leftCallee;
	std::string_view rightCallee;
	std::string_view replacement;
};
constexpr BinaryRewrite kBinaryRewrites[] = {
	// Projected to its left operand (check_price), which renderHostCall then turns into
	// can_afford_slot -- see the check_price note in the header above. The wallet cap is
	// dropped here rather than rendered.
	{rls::ast::BinaryOp::LtEq, "check_price", "wallet_capacity", ""},
	// Replaced wholesale: the comparison is exactly the host's triforce-hunt win check.
	{rls::ast::BinaryOp::GtEq, "collected_triforce_pieces", "required_triforce_pieces", "CanWinTriforceHunt()"},
};

// A threshold comparison `<callee>() >= N` (or `> N` / `!= N` / `== N`) against a state-dependent
// count that the world exposes as an `_at_least`-style host rule taking the threshold as an
// argument. Unlike kBinaryRewrites the right operand is a value, not a call; it is threaded into
// the helper. These re-evaluate against collection state, so the comparison lowers to a Rule
// instead of a build-time-frozen Int (which ClassifyExpression rejects as a runtime value).
//
// The threshold need not be a literal -- any BuildTime expression works, since it is by
// definition fixed when the lambda that builds the rule runs. That covers a define's
// parameters, so `effective_health() >= quantity / 2 + 1` inside a define taking `quantity`
// lowers as `effective_health_at_least(bundle, quantity // 2 + 1)`.
//
// `extraArg` is inserted before the amount, for a helper that also takes a fixed argument -- e.g.
// GS tokens reuse the generic `has_item(bundle, <item>, count)`. `allowEq` permits `== N` (lowered
// as `>= N`); it is sound only for a count compared at its cap (StoneCount() == 3 means all three
// stones, so `== 3` is `>= 3`), NOT for an exact match like `hearts() == 3`, so it is off by default.
struct ThresholdRewrite {
	std::string_view rlsCallee;
	std::string_view pyHelper;
	std::string_view extraArg;
	bool allowEq;
};
constexpr ThresholdRewrite kThresholdRewrites[] = {
	{"fire_timer", "fire_timer_at_least", "", false},
	{"water_timer", "water_timer_at_least", "", false},
	{"hearts", "hearts_at_least", "", false},
	{"effective_health", "effective_health_at_least", "", false},
	// ocarina_buttons() >= N -> has_enough_ocarina_buttons(bundle, N). The world helper takes the
	// exact required-button count, matching SoH's `OcarinaButtons() >= N` (e.g. ScarecrowsSong needs 2).
	{"ocarina_buttons", "has_enough_ocarina_buttons", "", false},
	// stone_count() == 3 -> has_enough_stones(bundle, 3). Spiritual stones cap at 3, so the SoH
	// `StoneCount() == 3` ("have all stones") is exactly `>= 3`; allowEq makes `==` the cap check.
	{"stone_count", "has_enough_stones", "", true},
	// get_gs_count() >= N -> has_item(bundle, Items.RG_GOLD_SKULLTULA_TOKEN, N): the token count
	// reuses the generic has_item count check, matching SoH's `GetGSCount() >= N` GS rewards.
	{"get_gs_count", "has_item", "Items.RG_GOLD_SKULLTULA_TOKEN", false},
};

// The RLS `small_keys(scene, count)` (faithful to SoH's SmallKeys(scene, ...)) maps to the
// world's `small_keys(bundle, key, requiredAmount)`, which keys by the dungeon's small-key
// *item* rather than the scene. This table pairs each scene with that key item. Names diverge
// in places (Thieves' Hideout uses the Gerudo Fortress key), so the mapping is explicit.
struct SmallKeyScene {
	std::string_view scene;
	std::string_view keyItem;
};
constexpr SmallKeyScene kSmallKeyScenes[] = {
	{"SCENE_FOREST_TEMPLE", "RG_FOREST_TEMPLE_SMALL_KEY"},
	{"SCENE_FIRE_TEMPLE", "RG_FIRE_TEMPLE_SMALL_KEY"},
	{"SCENE_WATER_TEMPLE", "RG_WATER_TEMPLE_SMALL_KEY"},
	{"SCENE_BOTTOM_OF_THE_WELL", "RG_BOTTOM_OF_THE_WELL_SMALL_KEY"},
	{"SCENE_SHADOW_TEMPLE", "RG_SHADOW_TEMPLE_SMALL_KEY"},
	{"SCENE_THIEVES_HIDEOUT", "RG_GERUDO_FORTRESS_SMALL_KEY"},
	{"SCENE_GERUDO_TRAINING_GROUND", "RG_GERUDO_TRAINING_GROUND_SMALL_KEY"},
	{"SCENE_SPIRIT_TEMPLE", "RG_SPIRIT_TEMPLE_SMALL_KEY"},
	{"SCENE_INSIDE_GANONS_CASTLE", "RG_GANONS_CASTLE_SMALL_KEY"},
	{"SCENE_TREASURE_BOX_SHOP", "RG_TREASURE_GAME_SMALL_KEY"},
};

// RLS defines the world supplies by hand, so function generation skips them.
// has_bottle is a hand-written rule in the reference Rules.py / LogicHelpers;
// wallet_capacity is a state-dependent Int that only ever appears inside
// `check_price(...) <= wallet_capacity()` (collapsed away by kBinaryRewrites),
// so generating its body would emit an unrepresentable runtime value.
constexpr std::string_view kHostProvidedDefines[] = {
	"has_bottle",
	"wallet_capacity",
};

} // namespace

std::optional<std::string> SohApTranspiler::enumClassName(std::string_view enumName) const {
	for (const auto& mapping : kEnumClasses) {
		if (mapping.rlsEnum == enumName) {
			return std::string(mapping.pyClass);
		}
	}
	// An enum declared in RLS is generated into enums.gen.py under its own name (see
	// writeEnums), so it is referenced by that name -- no kEnumClasses row needed. Only
	// extern enums need one, to reach the class the hand-written world keeps them in.
	if (const auto* info = project.getEnumInfo(enumName);
		info != nullptr && info->kind == rls::ast::EnumKind::Normal) {
		return std::string(enumName);
	}
	return std::nullopt;
}

std::string SohApTranspiler::renderEnumValue(std::string_view enumName, const std::string& value) const {
	// Generic on/off setting literals collapse to Python booleans rather than enum members.
	if (enumName == "Setting") {
		if (value == "RO_GENERIC_YES") {
			return "True";
		} else if (value == "RO_GENERIC_NO") {
			return "False";
		}
	}
	if (auto cls = enumClassName(enumName)) {
		return *cls + "." + value;
	}
	return value;
}

std::optional<std::string> SohApTranspiler::renderHostCall(const rls::ast::CallExpr& node,
	size_t overrideIdx, const rls::ast::Expr* overrideExpr) const {
	const auto* resolvedPtr = project.getResolvedCallArgs(&node);
	if (!resolvedPtr || resolvedPtr->empty()) {
		return std::nullopt;
	}
	const auto& resolved = *resolvedPtr;

	// The argument at position `k`, honoring the ternary-distribution override (see renderCall):
	// when distributing `f(.., C ? A : B, ..)`, each branch re-renders this call with the ternary
	// argument replaced by A or B, so the rewrite emits the branch value in its place.
	auto argAt = [&](size_t k) -> const rls::ast::Expr* {
		return k == overrideIdx ? overrideExpr : resolved[k];
	};

	// Uniform `<helper>(bundle, <arg>)` rewrites (has/flag/trick).
	for (const auto& rewrite : kHostCallRewrites) {
		if (node.callee.text == rewrite.rlsCallee) {
			return std::string(rewrite.pyHelper) + "(bundle, " + GenerateExpression(argAt(0)->node) + ")";
		}
	}

	if (node.callee.text == "check_price") {
		// Special case: check_price(...) should output can_afford_slot(...).
		// If the argument is an RC_UNKNOWN_CHECK identifier, use the current location from context.
		if (auto* id = std::get_if<rls::ast::Identifier>(&argAt(0)->node);
			id && id->name.text == "RC_UNKNOWN_CHECK" && currentLocationName.has_value()) {
			return "can_afford_slot(Locations." + currentLocationName.value() + ")";
		}
		return "can_afford_slot(" + GenerateExpression(argAt(0)->node) + ")";
	}

	if (node.callee.text == "small_keys") {
		// Rewrite `small_keys(SCENE_X, count)` to `small_keys(bundle, key, count)`: map the
		// scene to its small-key item (see kSmallKeyScenes), bundle first per convention. When a
		// rule-conditioned count ternary is distributed, argAt(1) is the selected branch count.
		if (auto* sceneId = std::get_if<rls::ast::Identifier>(&argAt(0)->node)) {
			for (const auto& mapping : kSmallKeyScenes) {
				if (sceneId->name.text == mapping.scene) {
					return "small_keys(" + ruleContextParam() + ", Items." +
						std::string(mapping.keyItem) + ", " + GenerateExpression(argAt(1)->node) + ")";
				}
			}
		}
		// Unknown scene: fall through to the default call form rather than guess a key item.
		return std::nullopt;
	}

	return std::nullopt;
}

std::optional<std::string> SohApTranspiler::renderBinarySpecialCase(const rls::ast::BinaryExpr& node) const {
	// Collapse a `<leftCallee>(...) <op> <rightCallee>(...)` comparison to the world
	// rule registered for it (see kBinaryRewrites in the host vocabulary above).
	for (const auto& rewrite : kBinaryRewrites) {
		if (node.op != rewrite.op) {
			continue;
		}
		auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
		auto* rightCall = std::get_if<rls::ast::CallExpr>(&node.right->node);
		if (leftCall && rightCall &&
			leftCall->callee.text == rewrite.leftCallee &&
			rightCall->callee.text == rewrite.rightCallee) {
			return rewrite.replacement.empty()
				? GenerateExpression(node.left)
				: std::string(rewrite.replacement);
		}
	}

	// Threshold comparison `<callee>() >= N` / `> N` / `!= N` / `== N` -> the world's
	// `_at_least(bundle, N)` host rule. `> N` and `!= N` normalize to `>= N + 1`; the `!= N`
	// form is valid only because these counts have a floor at the target (e.g. effective
	// health is always >= 1, so `!= 1` means `>= 2`). `== N` (allowEq entries only) lowers as
	// `>= N`, sound when the count is compared at its cap. See kThresholdRewrites.
	if (node.op == rls::ast::BinaryOp::GtEq || node.op == rls::ast::BinaryOp::Gt ||
		node.op == rls::ast::BinaryOp::NotEq || node.op == rls::ast::BinaryOp::Eq) {
		auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
		auto* rightLit = std::get_if<rls::ast::IntLiteral>(&node.right->node);
		const bool rightIsBuildTime = ClassifyExpression(node.right) == ValueClass::BuildTime;
		if (leftCall && (rightLit || rightIsBuildTime)) {
			for (const auto& rewrite : kThresholdRewrites) {
				if (leftCall->callee.text != rewrite.rlsCallee) {
					continue;
				}
				// `==` is only a threshold (cap) check for entries that opt in; otherwise leave it
				// as a raw comparison, which classifies as a runtime value and is diagnosed.
				if (node.op == rls::ast::BinaryOp::Eq && !rewrite.allowEq) {
					break;
				}
				// `> N` and `!= N` normalize to `>= N + 1`. A literal folds; anything else gets
				// the `+ 1` in the emitted Python, parenthesized so it binds the whole threshold.
				const bool plusOne = node.op == rls::ast::BinaryOp::Gt || node.op == rls::ast::BinaryOp::NotEq;
				std::string amount;
				if (rightLit != nullptr) {
					amount = std::to_string(rightLit->value + (plusOne ? 1 : 0));
				} else {
					amount = GenerateExpression(node.right);
					if (plusOne) {
						amount = "(" + amount + ") + 1";
					}
				}
				std::string args = ruleContextParam() + ", ";
				if (!rewrite.extraArg.empty()) {
					args += std::string(rewrite.extraArg) + ", ";
				}
				args += amount;
				return std::string(rewrite.pyHelper) + "(" + args + ")";
			}
		}
	}

	return std::nullopt;
}

bool SohApTranspiler::isHostProvidedDefine(const std::string& name) const {
	// See kHostProvidedDefines in the host vocabulary above for why each is skipped.
	for (std::string_view provided : kHostProvidedDefines) {
		if (name == provided) {
			return true;
		}
	}
	return false;
}

} // namespace rls::transpilers::soh_ap
