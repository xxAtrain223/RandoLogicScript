// Tests for SoH host-call rewrites and special-cases: how `trick`, `check_price`,
// wallet-capacity and triforce-hunt comparisons, and age-conditional ternaries are lowered
// to the oot_soh runtime helpers. OptionFilter / setting rendering is covered by
// option_filter_tests.cpp.
#include "helpers.h"

using namespace rls::transpilers::soh_ap_tests;

namespace {
struct ResolvedExpression {
	rls::ast::Project project;
	rls::ast::ExprPtr expr;
};
} // namespace

static std::string GenerateExpression(const ResolvedExpression& resolved) {
	return rls::transpilers::soh_ap::SohApTranspiler(resolved.project).GenerateExpression(resolved.expr);
}

// Resolve a define from inline RLS source and hand back its body expression.
static ResolvedExpression sourceToExpression(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	auto defineDecl = project.DefineDecls.find(defineName);
	if (defineDecl == project.DefineDecls.end()) {
		return { std::move(project), nullptr };
	}

	return {
		std::move(project),
		std::move(const_cast<rls::ast::DefineDecl*>(defineDecl->second)->body)
	};
}

// `trick(...)` is rewritten to the SoH can_do_trick host call, threading the bundle
// receiver and prefixing the value with the Tricks enum class.
TEST(SohApHostRewrites, TrickCallRewritesToCanDoTrick) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    trick(RT_FOO)\n",
		"test")),
		"can_do_trick(bundle, Tricks.RT_FOO)");
}

// `check_price(check) <= wallet_capacity()` collapses to just the affordability check.
TEST(SohApHostRewrites, WalletCapacityComparisonCollapsesToAffordCheck) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define check_price(check: Check) -> Int\n"
		"extern define wallet_capacity() -> Int\n"
		"define test():\n"
		"    check_price(RC_FOO) <= wallet_capacity()\n",
		"test")),
		"can_afford_slot(Locations.RC_FOO)");
}

// `collected_triforce_pieces() >= required_triforce_pieces()` collapses to CanWinTriforceHunt().
TEST(SohApHostRewrites, TriforceHuntComparisonCollapses) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define collected_triforce_pieces() -> Int\n"
		"extern define required_triforce_pieces() -> Int\n"
		"define test():\n"
		"    collected_triforce_pieces() >= required_triforce_pieces()\n",
		"test")),
		"CanWinTriforceHunt()");
}

// A collapsed special-case call is atomic, so it composes with surrounding rules without
// extra parentheses (GetPythonPrecedence treats the rewrite as a tightly-bound call).
TEST(SohApHostRewrites, CollapsedSpecialCaseNeedsNoParensUnderAnd) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define check_price(check: Check) -> Int\n"
		"extern define wallet_capacity() -> Int\n"
		"define test():\n"
		"    has(RG_HOOKSHOT) and check_price(RC_FOO) <= wallet_capacity()\n",
		"test")),
		"has_item(bundle, Items.RG_HOOKSHOT) & can_afford_slot(Locations.RC_FOO)");
}

// A rule-conditioned ternary `is_child() ? a : b` cannot be a Python `if`, so it lowers to
// the rule idiom `(is_child() & a) | b` -- the then-branch gated by the age rule, the
// else-branch unconditional. No complement (is_adult) is synthesized: the source never wrote
// one, and the else-branch items stay age-independent. This verifies the generic lowering
// (ternary_tests.cpp) threads the bundle receiver and enum prefixes through the SoH hooks.
TEST(SohApHostRewrites, AgeConditionalTernaryLowersToRuleIdiom) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define is_child() -> Bool\n"
		"define test():\n"
		"    is_child() ? has(RG_HOOKSHOT) : has(RG_BOOMERANG)\n",
		"test")),
		"(is_child(bundle) & has_item(bundle, Items.RG_HOOKSHOT)) | "
		"has_item(bundle, Items.RG_BOOMERANG)");
}

// A loose else branch (an or-rule) is parenthesized as the right operand of `|`, and is left
// ungated -- those items are reachable regardless of the condition.
TEST(SohApHostRewrites, AgeConditionalLeavesElseOrBranchUngated) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define is_child() -> Bool\n"
		"define test():\n"
		"    is_child() ? has(RG_HOOKSHOT) : (has(RG_BOOMERANG) or has(RG_FAIRY_BOW))\n",
		"test")),
		"(is_child(bundle) & has_item(bundle, Items.RG_HOOKSHOT)) | "
		"(has_item(bundle, Items.RG_BOOMERANG) | has_item(bundle, Items.RG_FAIRY_BOW))");
}

// A fallthrough rule match renders the SoH host-call rewrites and enum prefixes inside the
// arm lambdas, and threads the bundle receiver -- the helper |-combines matched arms.
TEST(SohApHostRewrites, RuleMatchThreadsBundleAndEnumPrefixes) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: can_use(RG_MEGATON_HAMMER) or\n"
		"        ED_HOOKSHOT: can_use(RG_HOOKSHOT)\n"
		"    }\n",
		"test")),
		"rls_match_rule((lambda d=d: d == EnemyDistance.ED_CLOSE), "
		"(lambda: can_use(bundle, Items.RG_MEGATON_HAMMER)), True, "
		"(lambda d=d: d == EnemyDistance.ED_HOOKSHOT), "
		"(lambda: can_use(bundle, Items.RG_HOOKSHOT)), False)");
}

// check_price(RC_UNKNOWN_CHECK) uses the location set on the transpiler during region
// generation to emit can_afford_slot(Locations.<current location>).
TEST(SohApHostRewrites, CheckPriceUnknownUsesCurrentLocation) {
	auto resolved = sourceToExpression(
		"extern define check_price(check: Check) -> Int\n"
		"extern define wallet_capacity() -> Int\n"
		"define test():\n"
		"    check_price(RC_UNKNOWN_CHECK) <= wallet_capacity()\n",
		"test");
	rls::transpilers::soh_ap::SohApTranspiler transpiler(resolved.project);
	transpiler.SetCurrentLocation("RC_KF_SHOP_ITEM_1");
	EXPECT_EQ(transpiler.GenerateExpression(resolved.expr),
		"can_afford_slot(Locations.RC_KF_SHOP_ITEM_1)");
}
