// Tests for `match` lowering. A match dispatches to one of two runtime helpers chosen by
// the value class of its arm bodies: `rls_match_value` (build-time values -- returns the
// selected one, with a type-appropriate default) or `rls_match_rule` (rules -- |-combines
// matched arms, accumulating down `or`-fallthrough chains, since `bool(rule)` cannot drive
// a Python `or`). Each arm renders to a flat `condition, body, fallthrough` triple.
// Generic AP behavior, tested against the minimal default-hook transpiler; SoH-specific
// rendering inside the lambdas (bundle, enum prefixes) is covered in host_rewrite_tests.
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

// == value matches ============================================================

// Integer arm bodies make a value match defaulting to 0.
TEST(ApMatch, IntValueMatchUsesValueHelperWithZeroDefault) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: 0\n"
		"        ED_FAR: 8\n"
		"    }\n",
		"test")),
		"rls_match_value(0, (lambda d=d: d == ED_CLOSE), (lambda: 0), False, "
		"(lambda d=d: d == ED_FAR), (lambda: 8), False)");
}

// A build-time bool arm body (a Bool parameter) defaults to False instead of 0.
TEST(ApMatch, BoolValueMatchDefaultsToFalse) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(d: Distance, flag: Bool):\n"
		"    match d {\n"
		"        ED_CLOSE: flag\n"
		"    }\n",
		"test")),
		"rls_match_value(False, (lambda d=d: d == ED_CLOSE), (lambda: flag), False)");
}

// == rule matches =============================================================

// Rule arm bodies make a rule match; without fallthrough each arm stands alone.
TEST(ApMatch, RuleMatchUsesRuleHelper) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(e: Enemy):\n"
		"    match e {\n"
		"        RE_GOLD_SKULLTULA: has(RG_HOOKSHOT)\n"
		"        RE_KEESE: has(RG_BOOMERANG)\n"
		"    }\n",
		"test")),
		"rls_match_rule((lambda e=e: e == RE_GOLD_SKULLTULA), (lambda: has(RG_HOOKSHOT)), False, "
		"(lambda e=e: e == RE_KEESE), (lambda: has(RG_BOOMERANG)), False)");
}

// A trailing `or` marks an arm as falling through: the runtime helper carries the True
// flag so it |-combines the matched arm with the arms below it.
TEST(ApMatch, FallthroughArmSetsFlag) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: has(RG_HOOKSHOT) or\n"
		"        ED_FAR: has(RG_BOOMERANG)\n"
		"    }\n",
		"test")),
		"rls_match_rule((lambda d=d: d == ED_CLOSE), (lambda: has(RG_HOOKSHOT)), True, "
		"(lambda d=d: d == ED_FAR), (lambda: has(RG_BOOMERANG)), False)");
}

// A default arm (`_`) always matches, so its condition is the constant-True lambda.
TEST(ApMatch, DefaultArmRendersAlwaysTrueCondition) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(e: Enemy):\n"
		"    match e {\n"
		"        RE_GOLD_SKULLTULA: has(RG_HOOKSHOT)\n"
		"        _: has(RG_BOOMERANG)\n"
		"    }\n",
		"test")),
		"rls_match_rule((lambda e=e: e == RE_GOLD_SKULLTULA), (lambda: has(RG_HOOKSHOT)), False, "
		"(lambda: True), (lambda: has(RG_BOOMERANG)), False)");
}

// Multiple patterns on one arm are or-combined inside the condition lambda.
TEST(ApMatch, MultiPatternArmOrsConditions) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test(e: Enemy):\n"
		"    match e {\n"
		"        RE_KEESE or\n"
		"        RE_FIRE_KEESE: has(RG_BOOMERANG)\n"
		"    }\n",
		"test")),
		"rls_match_rule((lambda e=e: e == RE_KEESE or e == RE_FIRE_KEESE), "
		"(lambda: has(RG_BOOMERANG)), False)");
}
