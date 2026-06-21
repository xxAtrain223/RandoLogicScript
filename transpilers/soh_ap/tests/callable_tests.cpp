// Tests for SoH callable lowering: how Condition arguments, invoke expressions, and the
// rule-context receiver (`bundle`) render for the SoH world. The generic mechanism is
// covered by the ap transpiler's callable_tests.cpp; this file pins the SoH-specific
// `bundle`-threaded shapes that match the oot_soh RuleBuilder convention
// (a Condition is `Callable[[bundle], Rule]`, invoked as `cond(bundle)`).
// OptionFilter rendering and host-call rewrites live in their own test files.
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

// Invoking a Condition parameter threads the bundle receiver: a Condition is a one-arg
// rule callback `Callable[[bundle], Rule]`, so it must be called as `cond(bundle)`.
TEST(SohApCallables, InvokeThreadsBundle) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(cond: Condition):\n"
		"    cond()\n",
		"test")),
		"cond(bundle)");
}

// A non-Condition argument bound to a Condition parameter is wrapped in a bundle-taking
// thunk; the call itself also threads the bundle receiver as the first argument.
TEST(SohApCallables, ConditionArgumentWrapsThunkWithBundle) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define both(left: Condition, right: Condition):\n"
		"    left() and right()\n"
		"\n"
		"define test():\n"
		"    both(has(RG_HOOKSHOT), has(RG_BOOMERANG))\n",
		"test")),
		"both(bundle, (lambda bundle: has_item(bundle, Items.RG_HOOKSHOT)), "
		"(lambda bundle: has_item(bundle, Items.RG_BOOMERANG)))");
}

// A bool literal bound to a Condition parameter wraps the rule literal True_() in the thunk,
// not the bare Python True used for ordinary value parameters.
TEST(SohApCallables, BoolLiteralConditionArgumentWrapsRuleLiteral) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define gate(cond: Condition):\n"
		"    cond()\n"
		"\n"
		"define test():\n"
		"    gate(true)\n",
		"test")),
		"gate(bundle, (lambda bundle: True_()))");
}

// Forwarding a Condition value passes it through unchanged (no double-wrap), then invoking
// the result threads the bundle.
TEST(SohApCallables, ConditionValuePassesThroughThenInvokes) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define make_cond(cond: Condition):\n"
		"    cond\n"
		"\n"
		"define test(cond: Condition):\n"
		"    make_cond(cond)()\n",
		"test")),
		"make_cond(bundle, cond)(bundle)");
}
