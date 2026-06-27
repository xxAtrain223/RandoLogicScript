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
//   Shared blocks (renderSharedBlock):
//     spirit_shared(...)
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

std::optional<std::string> SohApTranspiler::enumClassName(rls::ast::Type type) const {
	switch (type) {
		case rls::ast::Type::Item: return "Items";
		case rls::ast::Type::Enemy: return "Enemies";
		case rls::ast::Type::Distance: return "EnemyDistance";
		case rls::ast::Type::Trick: return "Tricks";
		// Water-level values live in the Events enum in the reference oot_soh world.
		case rls::ast::Type::Logic:
		case rls::ast::Type::WaterLevel: return "Events";
		case rls::ast::Type::Setting: return "RandomizerSettingKey";
		case rls::ast::Type::Region: return "Regions";
		case rls::ast::Type::Check: return "Locations";
		case rls::ast::Type::Trial: return "TrialKey";
		default: return std::nullopt;
	}
}

std::string SohApTranspiler::renderEnumValue(rls::ast::Type type, const std::string& name) const {
	// Generic on/off setting literals collapse to Python booleans rather than enum members.
	if (type == rls::ast::Type::Setting) {
		if (name == "RO_GENERIC_YES") {
			return "True";
		} else if (name == "RO_GENERIC_NO") {
			return "False";
		}
	}
	if (auto cls = enumClassName(type)) {
		return *cls + "." + name;
	}
	return name;
}

std::optional<std::string> SohApTranspiler::renderHostCall(const rls::ast::CallExpr& node) const {
	const auto* resolvedPtr = project.getResolvedCallArgs(&node);
	if (!resolvedPtr || resolvedPtr->empty()) {
		return std::nullopt;
	}
	const auto& resolved = *resolvedPtr;

	// Uniform `<helper>(bundle, <arg>)` rewrites (has/flag/trick).
	for (const auto& rewrite : kHostCallRewrites) {
		if (node.callee.text == rewrite.rlsCallee) {
			return std::string(rewrite.pyHelper) + "(bundle, " + GenerateExpression(resolved[0]->node) + ")";
		}
	}

	if (node.callee.text == "check_price") {
		// Special case: check_price(...) should output can_afford_slot(...).
		// If the argument is an RC_UNKNOWN_CHECK identifier, use the current location from context.
		if (auto* id = std::get_if<rls::ast::Identifier>(&resolved[0]->node);
			id && id->name.text == "RC_UNKNOWN_CHECK" && currentLocationName.has_value()) {
			return "can_afford_slot(Locations." + currentLocationName.value() + ")";
		}
		return "can_afford_slot(" + GenerateExpression(resolved[0]->node) + ")";
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

// Going to have to fiddle with this.
// I think as long as we define the SharedSpirit functions and the SharedSpiritData map this could be done
std::string SohApTranspiler::renderSharedBlock(const rls::ast::SharedBlock& node) const {
	std::ostringstream oss;

	const auto& firstBranch = node.branches[0];
	oss << "spirit_shared(" << firstBranch.region->text << ", "
		<< "(lambda: " << GenerateExpression(firstBranch.condition) << "), "
		<< (node.anyAge ? "True" : "False");

	for (size_t i = 1; i < node.branches.size(); i++) {
		oss << ", " << node.branches[i].region->text << ", "
			<< "(lambda:" << GenerateExpression(node.branches[i].condition) << ")";
	}

	oss << ")";

	return oss.str();
}

} // namespace rls::transpilers::soh_ap
