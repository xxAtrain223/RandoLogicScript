// Tests for callable lowering: Condition-argument thunking, callable pass-through,
// function references (FunctionRef), and invoke expressions. This is generic AP behavior
// owned by the base ApTranspiler, so it is tested against a minimal default-hook transpiler.
// Because that transpiler has no rule context parameter, thunks render as `(lambda : ...)`;
// a derived world with a ruleContextParam() (e.g. SoH's `bundle`) renders `(lambda bundle: ...)`.
// OptionFilter rendering is covered by option_filter_tests.cpp.
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

// A non-Condition expression bound to a Condition parameter is wrapped in a lazy thunk
// so the RuleBuilder evaluates it on demand.
TEST(ApCallables, ConditionArgumentIsWrappedInThunk) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define both(left: Condition, right: Condition):\n"
		"    left() and right()\n"
		"\n"
		"define test():\n"
		"    both(has(RG_HOOKSHOT) or can_use(RG_BOOMERANG), has(RG_FAIRY_BOW))\n",
		"test")),
		"both((lambda : has(RG_HOOKSHOT) | can_use(RG_BOOMERANG)), (lambda : has(RG_FAIRY_BOW)))");
}

// A bool literal bound to a Condition parameter is wrapped as a thunk whose body is the
// rule literal True_(), not the bare Python `True` used for ordinary value parameters.
TEST(ApCallables, BoolLiteralConditionArgumentWrapsRuleLiteral) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define gate(cond: Condition):\n"
		"    cond()\n"
		"\n"
		"define test():\n"
		"    gate(true)\n",
		"test")),
		"gate((lambda : True_()))");
}

// An argument that is already a Condition value (forwarding a Condition parameter) is passed
// through unchanged rather than being double-wrapped in another thunk.
TEST(ApCallables, ConditionValueArgumentPassesThrough) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define any_age(condition: Condition) -> Bool\n"
		"define gate(cond: Condition):\n"
		"    any_age(cond)\n",
		"gate")),
		"any_age(cond)");
}

// A bare reference to a function used as a callable value emits just the function name.
TEST(ApCallables, FunctionReferenceEmitsBareName) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define always_true():\n"
		"    true\n"
		"\n"
		"define return_cond():\n"
		"    always_true\n",
		"return_cond")),
		"always_true");
}

// Invoking a callable-valued result appends `()` to the callee expression.
TEST(ApCallables, InvokeAppendsCallParentheses) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define make_cond(cond: Condition):\n"
		"    cond\n"
		"\n"
		"define test(cond: Condition):\n"
		"    make_cond(cond)()\n",
		"test")),
		"make_cond(cond)()");
}
