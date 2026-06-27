// Tests for R/V bridging in `and`/`or` lowering (§4.1 of docs/AP-Function-Generation.md):
// two rules combine with the RuleBuilder `&`/`|` operators, two build-time values combine
// with Python `and`/`or`, and a mixed pair folds the build-time value into a conditional
// (`R if V else False_()` / `True_() if V else R`) -- the only sound way to splice a
// build-time fact into a runtime rule. Generic AP behavior, tested against the minimal
// default-hook transpiler (no `bundle` receiver, host calls render as plain calls).
#include "helpers.h"

using namespace rls::transpilers::ap_tests;

namespace {
struct ResolvedExpression {
	rls::ast::Project project;
	rls::ast::ExprPtr expr;
};
} // namespace

static std::string GenerateExpression(const ResolvedExpression& resolved) {
	return TestApTranspiler(resolved.project).GenerateExpression(resolved.expr);
}

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

// == Rule & Rule -> RuleBuilder operators =====================================

TEST(ApBridging, RuleAndRuleUsesBitwiseAnd) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) and has(RG_BOOMERANG)\n",
		"test")),
		"has(RG_HOOKSHOT) & has(RG_BOOMERANG)");
}

TEST(ApBridging, RuleOrRuleUsesBitwiseOr) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) or has(RG_BOOMERANG)\n",
		"test")),
		"has(RG_HOOKSHOT) | has(RG_BOOMERANG)");
}

// == Value & Value -> Python operators ========================================
// Build-time witnesses are bool parameters: they are bound to literals/config at the
// call that builds the rule, so they are genuinely frozen at build time -- unlike a
// runtime quantity such as bottle_count().

TEST(ApBridging, ValueAndValueUsesPythonAnd) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(a: Bool, b: Bool):\n"
		"    a and b\n",
		"test")),
		"a and b");
}

TEST(ApBridging, ValueOrValueUsesPythonOr) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(a: Bool, b: Bool):\n"
		"    a or b\n",
		"test")),
		"a or b");
}

// == Mixed -> build-time short-circuit conditional ============================

// `V and R` folds the build-time value into the conditional that selects the rule. This
// mirrors the real `wall_or_floor and can_use(...)` in the enemies logic.
TEST(ApBridging, ValueAndRuleFoldsToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(wall_or_floor: Bool):\n"
		"    wall_or_floor and can_use(RG_BOMBCHU_5)\n",
		"test")),
		"can_use(RG_BOMBCHU_5) if wall_or_floor else False_()");
}

// `R and V` lowers identically -- the build-time operand becomes the condition regardless
// of which side it was written on.
TEST(ApBridging, RuleAndValueFoldsToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(wall_or_floor: Bool):\n"
		"    can_use(RG_BOMBCHU_5) and wall_or_floor\n",
		"test")),
		"can_use(RG_BOMBCHU_5) if wall_or_floor else False_()");
}

// `V or R`: a true build-time value short-circuits to an always-open rule. This mirrors
// the real `above_link or (...)` in can_get_drop.
TEST(ApBridging, ValueOrRuleFoldsToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(above_link: Bool):\n"
		"    above_link or can_use(RG_BOOMERANG)\n",
		"test")),
		"True_() if above_link else can_use(RG_BOOMERANG)");
}

// `R or V` lowers identically to `V or R`.
TEST(ApBridging, RuleOrValueFoldsToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(above_link: Bool):\n"
		"    can_use(RG_BOOMERANG) or above_link\n",
		"test")),
		"True_() if above_link else can_use(RG_BOOMERANG)");
}

// == Composition: a folded conditional nested under a rule operator ===========

// A mixed subexpression is itself a rule (R), so combining it with another rule uses `&`;
// the conditional is a loose Python ternary, so it must be parenthesized under `&`.
TEST(ApBridging, FoldedConditionalIsParenthesizedUnderBitwiseAnd) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(wall_or_floor: Bool):\n"
		"    wall_or_floor and can_use(RG_BOMBCHU_5) and has(RG_BOOMERANG)\n",
		"test")),
		"(can_use(RG_BOMBCHU_5) if wall_or_floor else False_()) & has(RG_BOOMERANG)");
}
