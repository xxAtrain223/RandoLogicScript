// Tests for OptionFilter generation: how `setting()` expressions become RuleBuilder
// OptionFilters wrapped in standalone rules. This is generic AP behavior owned by the
// base ApTranspiler, so it is tested against a minimal default-hook transpiler with no
// game-specific rendering. Game-specific renderings (enum prefixes, host-call rewrites)
// are covered by each derived transpiler's own tests.
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

// A bare OptionFilter is not a RuleBuilder Rule, so a `setting(KEY) is VALUE` comparison
// is wrapped in its own True_(options=[...]) rule.
TEST(ApOptionFilters, SettingComparisonIsWrappedAsRule) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO) is RO_BAR\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, RO_BAR)])");
}

// `is not` carries the "ne" operator through into the OptionFilter.
TEST(ApOptionFilters, NotEqualSettingComparisonUsesNeOperator) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO) is not RO_BAR\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, RO_BAR, \"ne\")])");
}

// A bare setting() call is a truthiness check, emitted as OptionFilter(KEY, True).
TEST(ApOptionFilters, BareSettingCallChecksTrue) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO)\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, True)])");
}

// `not setting()` is the negated truthiness check, OptionFilter(KEY, False).
TEST(ApOptionFilters, NegatedSettingCallChecksFalse) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    not setting(RSK_FOO)\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, False)])");
}

// Two setting comparisons OR'd together cannot combine as bare OptionFilters in RuleBuilder.
// Each must be wrapped in its own rule and joined with `|` so it stays a valid Or of rules.
TEST(ApOptionFilters, AdjacentOrOfSettingsBecomesSeparateWrappedRules) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO) is RO_BAR or setting(RSK_FOO) is RO_BAZ\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, RO_BAR)]) | "
		"True_(options=[OptionFilter(RSK_FOO, RO_BAZ)])");
}

// The same applies to AND: each wrapped rule joins with `&`.
TEST(ApOptionFilters, AdjacentAndOfSettingsBecomesSeparateWrappedRules) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_FOO) is RO_BAR and setting(RSK_QUX) is RO_BAZ\n",
		"test")),
		"True_(options=[OptionFilter(RSK_FOO, RO_BAR)]) & "
		"True_(options=[OptionFilter(RSK_QUX, RO_BAZ)])");
}

// A setting comparison combined with a real rule needs no extra parentheses around the
// wrapped OptionFilter rule: it is emitted as an atomic call, not a Python comparison.
TEST(ApOptionFilters, SettingComparisonMixedWithRuleHasNoExtraParens) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) or setting(RSK_FOO) is RO_BAR\n",
		"test")),
		"has(RG_HOOKSHOT) | "
		"True_(options=[OptionFilter(RSK_FOO, RO_BAR)])");
}

// Complex rule to test parenthesis
TEST(ApOptionFilters, ComplexSettingParens) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT) or setting(RSK_FOO) is RO_BAR and (setting(RSK_BAR) is RO_FOO or can_kill(RE_GOLD_SKULLTULA)) and (flag(LOGIC_BAZ) or setting(RSK_BAZ) is RO_GENERIC_YES)\n",
		"test")),
		"has(RG_HOOKSHOT) | "
		"True_(options=[OptionFilter(RSK_FOO, RO_BAR)]) & "
		"(True_(options=[OptionFilter(RSK_BAR, RO_FOO)]) | can_kill(RE_GOLD_SKULLTULA)) & "
		"(flag(LOGIC_BAZ) | True_(options=[OptionFilter(RSK_BAZ, RO_GENERIC_YES)]))");
}
