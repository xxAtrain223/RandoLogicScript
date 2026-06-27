// Tests for negation of pure option-filter rules. The RuleBuilder has no general rule
// negation, but a rule built entirely from setting(...) comparisons resolves at build time
// against world.options, so `not` over it is sound: push `not` down via De Morgan and flip
// each setting leaf (eq <-> "ne"). Each test states the resulting truth condition, so it is
// clear under what circumstances the negated rule is true or false. Generic AP behavior,
// tested against the minimal default-hook transpiler (no receiver, bare enum values).
// `not` over anything with a collection rule (has/can_use/...) stays a diagnostic.
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

// == representable negations ==================================================

// not (setting in {A,B,C}) -> De Morgan -> AND of three "ne" filters.
// True  <=> RSK_FOO is none of {RO_A, RO_B, RO_C}.
TEST(ApNegation, NotMembershipBecomesAndOfNeFilters) {
	GenResult r = generate(
		"define test():\n"
		"    not (setting(RSK_FOO) is RO_A or setting(RSK_FOO) is RO_B or setting(RSK_FOO) is RO_C)\n",
		"test");
	EXPECT_TRUE(r.diagnostics.empty());
	EXPECT_EQ(r.output,
		"True_(options=[OptionFilter(RSK_FOO, RO_A, \"ne\")]) & "
		"True_(options=[OptionFilter(RSK_FOO, RO_B, \"ne\")]) & "
		"True_(options=[OptionFilter(RSK_FOO, RO_C, \"ne\")])");
}

// not (setting is V) flips the single leaf to "ne". True <=> RSK_FOO != RO_A.
TEST(ApNegation, NotSingleEqualityBecomesNe) {
	GenResult r = generate(
		"define test():\n"
		"    not (setting(RSK_FOO) is RO_A)\n",
		"test");
	EXPECT_TRUE(r.diagnostics.empty());
	EXPECT_EQ(r.output, "True_(options=[OptionFilter(RSK_FOO, RO_A, \"ne\")])");
}

// not (a and b) -> not a | not b, even across different setting keys (the case the single
// list-"in" form cannot express). True <=> RSK_FOO != RO_A or RSK_BAR != RO_B.
TEST(ApNegation, NotConjunctionBecomesDisjunctionOfNe) {
	GenResult r = generate(
		"define test():\n"
		"    not (setting(RSK_FOO) is RO_A and setting(RSK_BAR) is RO_B)\n",
		"test");
	EXPECT_TRUE(r.diagnostics.empty());
	EXPECT_EQ(r.output,
		"True_(options=[OptionFilter(RSK_FOO, RO_A, \"ne\")]) | "
		"True_(options=[OptionFilter(RSK_BAR, RO_B, \"ne\")])");
}

// Double negation cancels back to the positive rule. True <=> RSK_FOO == RO_A.
TEST(ApNegation, DoubleNegationCancels) {
	GenResult r = generate(
		"define test():\n"
		"    not (not (setting(RSK_FOO) is RO_A))\n",
		"test");
	EXPECT_TRUE(r.diagnostics.empty());
	EXPECT_EQ(r.output, "True_(options=[OptionFilter(RSK_FOO, RO_A)])");
}

// Negating the rule literals: not true is never satisfiable, not false is always satisfiable.
TEST(ApNegation, NotBoolLiteralsFlipRuleLiterals) {
	EXPECT_EQ(generate("define test():\n    not true\n", "test").output, "False_()");
	EXPECT_EQ(generate("define test():\n    not false\n", "test").output, "True_()");
}

// == the diagnostic boundary ==================================================

// A `not` over anything containing a collection rule (here has(...)) is NOT a pure
// option-filter rule, so it cannot be negated and is diagnosed.
TEST(ApNegation, NotOverCollectionRuleIsDiagnosed) {
	GenResult r = generate(
		"define test():\n"
		"    not (setting(RSK_FOO) is RO_A or has(RG_HOOKSHOT))\n",
		"test");
	ASSERT_EQ(r.diagnostics.size(), 1u);
	EXPECT_EQ(r.diagnostics[0].level, rls::ast::DiagnosticLevel::Error);
	EXPECT_NE(r.diagnostics[0].message.find("negate a rule"), std::string::npos)
		<< "message was: " << r.diagnostics[0].message;
}
