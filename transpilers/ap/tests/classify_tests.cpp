// Tests for R/V classification (ExpressionIsRule): whether an expression lowers to a
// runtime Rule (R) or a build-time Python value (V). This is generic AP analysis owned
// by the base ApTranspiler, so it is exercised against the minimal default-hook
// transpiler. The codegen that consumes this classification lives elsewhere; these
// tests pin the classification itself.
#include "helpers.h"

using namespace rls::transpilers::ap_tests;

// Classify the body of a named define from inline RLS source. The define body is left
// in place (not moved out) so callees referenced by name remain classifiable.
static bool defineIsRule(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	auto it = project.DefineDecls.find(defineName);
	if (it == project.DefineDecls.end()) {
		ADD_FAILURE() << "define not found: " << defineName;
		return false;
	}
	return TestApTranspiler(project).ExpressionIsRule(it->second->body);
}

// == Runtime rules (R) ========================================================

// A bare host Bool call is the canonical runtime rule.
TEST(ApClassify, HostBoolCallIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test():\n"
		"    has(RG_HOOKSHOT)\n",
		"test"));
}

// true/false lower to the True_()/False_() rule literals, so they are rules.
TEST(ApClassify, BoolLiteralIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test():\n"
		"    true\n",
		"test"));
}

// A disjunction of host rules is a rule.
TEST(ApClassify, OrOfHostRulesIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test():\n"
		"    can_use(RG_HOOKSHOT) or has(RG_BOOMERANG)\n",
		"test"));
}

// A setting comparison lowers to an OptionFilter-wrapped rule.
TEST(ApClassify, SettingComparisonIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test():\n"
		"    setting(RSK_FOO) is RO_BAR\n",
		"test"));
}

// `not setting(...)` is still an OptionFilter rule (the negation lives in the filter).
TEST(ApClassify, NotSettingIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test():\n"
		"    not setting(RSK_FOO)\n",
		"test"));
}

// A user define whose body is a host rule is itself a rule, transitively.
TEST(ApClassify, UserDefineForwardingHostRuleIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define inner():\n"
		"    can_use(RG_BOMB_BAG) or can_use(RG_BOMBCHU_5)\n"
		"\n"
		"define test():\n"
		"    inner()\n",
		"test"));
}

// A match whose arm bodies are rules is a rule.
TEST(ApClassify, MatchWithRuleBodiesIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test(e: Enemy):\n"
		"    match e {\n"
		"        RE_GOLD_SKULLTULA: can_use(RG_BOOMERANG)\n"
		"    }\n",
		"test"));
}

// == Build-time values (V) ====================================================

// An integer literal is a build-time value.
TEST(ApClassify, IntLiteralIsValue) {
	EXPECT_FALSE(defineIsRule(
		"define test():\n"
		"    5\n",
		"test"));
}

// A comparison over a runtime host quantity (bottle_count) is not a rule -- it is a
// runtime non-rule value that must be lowered to a host rule, never a Python comparison.
TEST(ApClassify, RuntimeIntComparisonIsNotRule) {
	EXPECT_FALSE(defineIsRule(
		"extern define bottle_count() -> Int\n"
		"define test():\n"
		"    bottle_count() >= 1\n",
		"test"));
}

// A match whose arm bodies are integers is a build-time value.
TEST(ApClassify, MatchWithValueBodiesIsValue) {
	EXPECT_FALSE(defineIsRule(
		"define test(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: 0\n"
		"        ED_FAR: 8\n"
		"    }\n",
		"test"));
}

// An Int-returning host call is a build-time value.
TEST(ApClassify, IntHostCallIsValue) {
	EXPECT_FALSE(defineIsRule(
		"extern define bottle_count() -> Int\n"
		"define test():\n"
		"    bottle_count()\n",
		"test"));
}

// A bare Bool parameter carries a build-time value (deferred rules are passed as
// Conditions, not bare Bools).
TEST(ApClassify, BoolParameterIsValue) {
	EXPECT_FALSE(defineIsRule(
		"define test(flag: Bool):\n"
		"    flag\n",
		"test"));
}

// == Mixed (folds to a rule) ==================================================

// `V and R` is a rule: the build-time value (a bool parameter, frozen when the rule is
// built) short-circuits to select the rule.
TEST(ApClassify, ValueAndRuleIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test(wall_or_floor: Bool):\n"
		"    wall_or_floor and has(RG_HOOKSHOT)\n",
		"test"));
}

// A ternary with rule branches is a rule even when its condition is build-time.
TEST(ApClassify, TernaryWithRuleBranchesIsRule) {
	EXPECT_TRUE(defineIsRule(
		"define test(pick: Bool):\n"
		"    pick ? has(RG_HOOKSHOT) : has(RG_BOOMERANG)\n",
		"test"));
}

// Combining a runtime non-rule value with a rule is not itself a representable rule:
// `bottle_count() >= 1` cannot be a Python `if` condition (it would freeze to the empty
// collection state), so the whole expression is not classified as a rule.
TEST(ApClassify, RuntimeValueAndRuleIsNotRule) {
	EXPECT_FALSE(defineIsRule(
		"extern define bottle_count() -> Int\n"
		"define test():\n"
		"    bottle_count() >= 1 and has(RG_HOOKSHOT)\n",
		"test"));
}
