#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ap_transpiler.h"

namespace {

using rls::transpilers::ap::FreePythonNames;
using Names = std::set<std::string>;

TEST(FreePythonNamesTests, ReportsCalledNamesAndEnumClasses) {
	EXPECT_EQ(FreePythonNames("x = can_use(bundle, Items.RG_HOOKSHOT)"), (Names{"x", "can_use", "bundle", "Items"}));
}

TEST(FreePythonNamesTests, AttributesAreNotNames) {
	// Only the class is imported, never its members, and a call's result can be accessed too.
	EXPECT_EQ(FreePythonNames("Items.RG_X.value"), (Names{"Items"}));
	EXPECT_EQ(FreePythonNames("OptionFilter(K, True).check(opts)"), (Names{"OptionFilter", "K", "opts"}));
}

TEST(FreePythonNamesTests, KeywordArgumentNamesAreNotNames) {
	EXPECT_EQ(FreePythonNames("True_(options=[OptionFilter(RSK_A, V)])"), (Names{"True_", "OptionFilter", "RSK_A", "V"}));
	EXPECT_EQ(FreePythonNames("f(a, b=c)"), (Names{"f", "a", "c"}));
}

TEST(FreePythonNamesTests, ComparisonIsNotAKeywordArgument) {
	// `==` is one token, so the left side of a comparison after a comma or paren is still a name.
	EXPECT_EQ(FreePythonNames("f(a == b)"), (Names{"f", "a", "b"}));
}

TEST(FreePythonNamesTests, DefBindsItsNameAndParametersButNotAnnotations) {
	const std::string src = "def helper(bundle, distance: EnemyDistance, flag: bool = False) -> bool:\n"
							"    return other(bundle, distance, flag)\n";
	EXPECT_EQ(FreePythonNames(src), (Names{"EnemyDistance", "other"}));
}

TEST(FreePythonNamesTests, DefaultValuesAreReferences) {
	EXPECT_EQ(FreePythonNames("def f(a, b = DEFAULT_B):\n    return a\n"), (Names{"DEFAULT_B"}));
}

TEST(FreePythonNamesTests, AnnotationWithNestedBracketsAndCommas) {
	// The comma inside the brackets must not start a new parameter.
	const std::string src = "def f(rule: Callable[[tuple[Regions, \"SohWorld\"]], Rule], b) -> Rule:\n    return b\n";
	EXPECT_EQ(FreePythonNames(src), (Names{"Callable", "Regions", "Rule"}));
}

TEST(FreePythonNamesTests, LambdaParametersAreBoundAndDefaultsAreReferences) {
	// `lambda distance=distance:` binds the parameter and reads the enclosing one.
	EXPECT_EQ(FreePythonNames("(lambda a, b=c: a + b + d)"), (Names{"c", "d"}));
	EXPECT_EQ(FreePythonNames("(lambda: g(h))"), (Names{"g", "h"}));
	EXPECT_EQ(FreePythonNames("lambda bundle: has_item(bundle, X)"), (Names{"has_item", "X"}));
}

TEST(FreePythonNamesTests, NestedLambdas) {
	EXPECT_EQ(FreePythonNames("lambda a: (lambda b: a + b + c)"), (Names{"c"}));
}

TEST(FreePythonNamesTests, StringsAndCommentsAreIgnored) {
	EXPECT_EQ(FreePythonNames("f(\"not_a_name\", 'also_not')  # nor_this\n"), (Names{"f"}));
	EXPECT_EQ(FreePythonNames("f(\"quote \\\" still inside\", real)"), (Names{"f", "real"}));
}

TEST(FreePythonNamesTests, KeywordsBuiltinsAndConstantsAreNotNames) {
	EXPECT_EQ(FreePythonNames("a if b and not c or d else None"), (Names{"a", "b", "c", "d"}));
	EXPECT_EQ(FreePythonNames("x: bool = True\ny: int = False\nz = tuple[a]"), (Names{"x", "y", "z", "a"}));
}

TEST(FreePythonNamesTests, NumbersAreNotNames) {
	EXPECT_EQ(FreePythonNames("f(16, 0x10, 1.5)"), (Names{"f"}));
}

TEST(FreePythonNamesTests, AlsoBoundNamesAreNotReported) {
	// A def line written separately from the body (the regions file's `world`).
	EXPECT_EQ(FreePythonNames("connect_regions(Regions.X, world, [])", {"world"}), (Names{"connect_regions", "Regions"}));
}

TEST(FreePythonNamesTests, MultiLineGeneratedShape) {
	const std::string src =
		"    add_locations(Regions.RR_A, world, [\n"
		"        (Locations.RC_B, lambda bundle: is_child(bundle) & has_item(bundle, Items.RG_C)),\n"
		"    ])\n";
	EXPECT_EQ(FreePythonNames(src, {"world"}), (Names{"add_locations", "Regions", "Locations", "is_child", "has_item", "Items"}));
}

} // namespace
