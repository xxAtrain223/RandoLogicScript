// Tests for "diagnose, don't miscompile" (§6.4 of docs/AP-Function-Generation.md): RLS
// that type-checks but cannot be expressed in the RuleBuilder target must raise a precise
// diagnostic rather than emit Python that throws at world-load. Covers negating a rule,
// a rule/runtime-valued ternary condition, and combining a runtime non-rule value with a
// rule. Generic AP behavior, tested against the minimal default-hook transpiler.
#include "helpers.h"

using namespace rls::transpilers::ap_tests;

namespace {
struct GenResult {
	std::string output;
	std::vector<rls::ast::Diagnostic> diagnostics;
};
} // namespace

static GenResult generate(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	auto it = project.DefineDecls.find(defineName);
	if (it == project.DefineDecls.end()) {
		ADD_FAILURE() << "define not found: " << defineName;
		return {};
	}
	TestApTranspiler transpiler(project);
	std::string output = transpiler.GenerateExpression(it->second->body);
	return { std::move(output), transpiler.Diagnostics() };
}

// Assert exactly one error diagnostic whose message contains `needle`.
static void expectOneError(const GenResult& result, const std::string& needle) {
	ASSERT_EQ(result.diagnostics.size(), 1u);
	EXPECT_EQ(result.diagnostics[0].level, rls::ast::DiagnosticLevel::Error);
	EXPECT_NE(result.diagnostics[0].message.find(needle), std::string::npos)
		<< "message was: " << result.diagnostics[0].message;
}

// == not ======================================================================

// Negating a rule is unrepresentable (no rule negation in the RuleBuilder), so it is
// diagnosed rather than silently dropping the `not`.
TEST(ApDiagnostics, NegatingRuleIsDiagnosed) {
	expectOneError(generate(
		"define test():\n"
		"    not has(RG_HOOKSHOT)\n",
		"test"),
		"negate");
}

// `not setting(...)` is the one representable negation: it becomes an OptionFilter with a
// false value and raises no diagnostic.
TEST(ApDiagnostics, NegatingSettingIsNotDiagnosed) {
	GenResult result = generate(
		"define test():\n"
		"    not setting(RSK_FOO)\n",
		"test");
	EXPECT_TRUE(result.diagnostics.empty());
	EXPECT_EQ(result.output, "True_(options=[OptionFilter(RSK_FOO, False)])");
}

// Negating a build-time value is an ordinary Python `not` -- emitted, not dropped, and
// not diagnosed. (Regression guard for the previously silently-dropped `not`.)
TEST(ApDiagnostics, NegatingBuildTimeValueEmitsPythonNot) {
	GenResult result = generate(
		"define test(flag: Bool):\n"
		"    not flag\n",
		"test");
	EXPECT_TRUE(result.diagnostics.empty());
	EXPECT_EQ(result.output, "not flag");
}

// == ternary condition ========================================================

// A rule-conditioned ternary with *rule* branches is no longer diagnosed: it lowers to the
// `(C & a) | b` rule idiom (see ternary_tests.cpp). Only the value-branch case below remains
// unrepresentable.

// A value-producing ternary whose condition is a rule (wallet_capacity style) is likewise
// diagnosed -- it cannot produce a state-dependent integer.
TEST(ApDiagnostics, RuleConditionedValueTernaryIsDiagnosed) {
	expectOneError(generate(
		"define test():\n"
		"    has(RG_TYCOON_WALLET) ? 999 : 0\n",
		"test"),
		"ternary condition");
}

// A build-time ternary condition is fine: the value is frozen when the rule is built, so
// it selects between the rule branches without a diagnostic.
TEST(ApDiagnostics, BuildTimeConditionedTernaryIsNotDiagnosed) {
	GenResult result = generate(
		"define test(pick: Bool):\n"
		"    pick ? has(RG_HOOKSHOT) : has(RG_BOW)\n",
		"test");
	EXPECT_TRUE(result.diagnostics.empty());
}

// == runtime value in and/or ==================================================

// A comparison over a runtime quantity (bottle_count) is neither a rule nor a build-time
// value; combining it with a rule via `and` is diagnosed.
TEST(ApDiagnostics, RuntimeValueCombinedWithRuleIsDiagnosed) {
	expectOneError(generate(
		"extern define bottle_count() -> Int\n"
		"define test():\n"
		"    bottle_count() >= 1 and has(RG_HOOKSHOT)\n",
		"test"),
		"runtime value");
}

// A match whose arms produce a runtime value (not a rule, not build-time) is unrepresentable.
TEST(ApDiagnostics, RuntimeValuedMatchIsDiagnosed) {
	expectOneError(generate(
		"extern define bottle_count() -> Int\n"
		"define test(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: bottle_count() >= 1\n"
		"    }\n",
		"test"),
		"runtime value");
}
