// Tests for rule-conditioned ternary lowering. A ternary `C ? a : b` whose condition is a
// rule (so it cannot be a Python `if` -- bool(rule) raises) and whose branches are rules
// lowers to `(C & a) | b`: the then-branch stays gated by the condition, the else-branch is
// unconditional. This needs no rule negation and never synthesizes a complement for the
// condition. Generic AP behavior, tested against the minimal default-hook transpiler (no
// receiver, no host rewrites). Build-time-conditioned and value-branch ternaries are covered
// in diagnostic_tests.cpp.
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

// `C ? a : b` lowers to `(C & a) | b`: the then-branch is gated by the rule condition, the
// else-branch is unconditional -- no complement of the condition is synthesized.
TEST(ApTernary, RuleConditionedTernaryGatesThenLeavesElseUnconditional) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) ? has(RG_BOW) : has(RG_SLINGSHOT)\n",
		"test")),
		"(has(RG_HOOKSHOT) & has(RG_BOW)) | has(RG_SLINGSHOT)");
}

// A loose else branch (an or-rule) is parenthesized as the right operand of `|`.
TEST(ApTernary, ElseOrBranchIsParenthesized) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) ? has(RG_BOW) : (has(RG_SLINGSHOT) or has(RG_BOOMERANG))\n",
		"test")),
		"(has(RG_HOOKSHOT) & has(RG_BOW)) | (has(RG_SLINGSHOT) | has(RG_BOOMERANG))");
}

// A nested else ternary chains: `C1 ? a : C2 ? b : c` -> `(C1 & a) | ((C2 & b) | c)`.
TEST(ApTernary, NestedElseTernaryChains) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) ? has(RG_BOW) :\n"
		"    has(RG_SLINGSHOT) ? has(RG_BOOMERANG) : has(RG_HAMMER)\n",
		"test")),
		"(has(RG_HOOKSHOT) & has(RG_BOW)) | ((has(RG_SLINGSHOT) & has(RG_BOOMERANG)) | has(RG_HAMMER))");
}
