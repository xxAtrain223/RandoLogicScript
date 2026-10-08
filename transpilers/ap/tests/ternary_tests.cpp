// Tests for rule-conditioned ternary lowering. A ternary `C ? a : b` whose condition is a
// rule (so it cannot be a Python `if` -- bool(rule) raises) lowers to the host conditional
// rule `rls_conditional(C, a, b)`, which evaluates C at solve time and returns the branch it
// selects. This needs no rule negation, never synthesizes a complement for the condition, and
// -- unlike the `(C & a) | b` idiom it replaced -- keeps the else-branch gated. Generic AP
// behavior, tested against the minimal default-hook transpiler (no receiver, no host
// rewrites). Build-time-conditioned and value-branch ternaries are covered in
// diagnostic_tests.cpp.
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

// `C ? a : b` lowers to `rls_conditional(C, a, b)`: each branch is reachable only under the
// truth value the source wrote for it, and no complement of the condition is synthesized.
TEST(ApTernary, RuleConditionedTernaryLowersToConditionalRule) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) ? has(RG_BOW) : has(RG_SLINGSHOT)\n",
		"test")),
		"rls_conditional(has(RG_HOOKSHOT), has(RG_BOW), has(RG_SLINGSHOT))");
}

// A compound else branch is one argument of the conditional rule -- no parentheses needed, and
// it stays gated on the condition being false rather than becoming unconditional.
TEST(ApTernary, ElseOrBranchStaysGated) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) ? has(RG_BOW) : (has(RG_SLINGSHOT) or has(RG_BOOMERANG))\n",
		"test")),
		"rls_conditional(has(RG_HOOKSHOT), has(RG_BOW), has(RG_SLINGSHOT) | has(RG_BOOMERANG))");
}

// A nested else ternary nests the conditional rules, mirroring the source chain exactly.
TEST(ApTernary, NestedElseTernaryChains) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) ? has(RG_BOW) :\n"
		"    has(RG_SLINGSHOT) ? has(RG_BOOMERANG) : has(RG_HAMMER)\n",
		"test")),
		"rls_conditional(has(RG_HOOKSHOT), has(RG_BOW), "
		"rls_conditional(has(RG_SLINGSHOT), has(RG_BOOMERANG), has(RG_HAMMER)))");
}

// When a rule-conditioned ternary's branches are VALUES fed into a call, they cannot be
// `&`-combined with the condition, so the call is distributed over the branches into a conditional
// rule that picks a branch at solve time: `f(C ? A : B)` -> `rls_conditional(C, f(A), f(B))`. This
// is the faithful lowering of the C++ ternary and needs no rule negation. (SoH-specific rendering
// -- bundle receiver, enum prefixes, host rewrites -- is covered in soh_ap's host_rewrite_tests.)
TEST(ApTernary, RuleConditionedValueBranchArgDistributesToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    can_use(has(RG_CLIMB) ? RG_HOOKSHOT : RG_LONGSHOT)\n",
		"test")),
		"rls_conditional(has(RG_CLIMB), can_use(RG_HOOKSHOT), can_use(RG_LONGSHOT))");
}

// A rule passed for a define's Bool parameter is lifted the same way. The body reads a bare Bool
// as build-time, so passing the rule through would put it in a Python `if` (bool(rule) raises);
// each branch binds a literal instead. Ship's can_ground_jump(trick(RT_GROUND_JUMP_HARD)).
TEST(ApTernary, RuleArgForBoolParamDistributesToConditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define jump(flower: Bool = false):\n"
		"    has(RG_BOMB_BAG) or (flower and has(RG_GORONS_BRACELET))\n"
		"define test():\n"
		"    jump(has(RG_CLIMB))\n",
		"test")),
		"rls_conditional(has(RG_CLIMB), jump(True), jump(False))");
}

// Distribution is scoped to RULE conditions. A build-time condition (a Bool parameter) leaves the
// ternary an ordinary Python `if` selecting the value in place -- no conditional rule, no
// duplicated call.
TEST(ApTernary, BuildTimeConditionedValueBranchArgStaysPythonIf) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(pick: Bool):\n"
		"    can_use(pick ? RG_HOOKSHOT : RG_LONGSHOT)\n",
		"test")),
		"can_use(RG_HOOKSHOT if pick else RG_LONGSHOT)");
}
