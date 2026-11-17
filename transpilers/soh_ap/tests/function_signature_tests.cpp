// Tests for generated function-definition signatures (§6.2/§6.3 of
// docs/AP-Function-Generation.md): the `bundle` receiver is the first parameter, and each
// parameter/return type annotation is the SoH Python type produced by pythonTypeName --
// which is pinned to renderEnumValue's enum class, so a value `Items.RG_FOO` always
// annotates as `Items`. SoH-specific because pythonTypeName/renderEnumValue are SoH hooks.
#include "helpers.h"

using namespace rls::transpilers::soh_ap_tests;

namespace {
// Exposes the protected function-generation building block so a test can capture the
// emitted functions_gen.py without wiring it into Transpile() (Phase 5).
class TestSohApTranspiler : public rls::transpilers::soh_ap::SohApTranspiler {
public:
	using rls::transpilers::soh_ap::SohApTranspiler::SohApTranspiler;
	using rls::transpilers::soh_ap::SohApTranspiler::GenerateFunctionDefinitionsSource;
};
} // namespace

// Generate functions_gen.py from inline RLS source and return its contents.
static std::string generateFunctions(const std::string& source) {
	auto project = resolveFromSource(source);
	TestSohApTranspiler transpiler(project);
	MemoryWriter writer;
	transpiler.GenerateFunctionDefinitionsSource(writer);
	return writer.content("functions_gen.py");
}

// The bundle receiver leads the parameter list, ahead of the RLS parameters.
TEST(SohApFunctionSignatures, AnnotatedReturnAndInferredParameter) {
	const auto output = generateFunctions("define identity(item) -> Item: item\n");
	EXPECT_NE(output.find("def identity(bundle, item: Items) -> Items:"), std::string::npos) << output;
}

TEST(SohApFunctionSignatures, BundleIsFirstParameter) {
	EXPECT_NE(generateFunctions(
		"define needs(item: Item):\n"
		"    has(item)\n").find("def needs(bundle, item: Items) -> bool:"),
		std::string::npos);
}

// A parameterless define still takes the bundle receiver and nothing else.
TEST(SohApFunctionSignatures, ParameterlessDefineTakesOnlyBundle) {
	EXPECT_NE(generateFunctions(
		"define unconditional():\n"
		"    true\n").find("def unconditional(bundle) -> bool:"),
		std::string::npos);
}

// Each enum-typed parameter is annotated with the same class renderEnumValue prefixes its
// values with: Item -> Items, Location -> Locations, Trick -> Tricks, Enemy -> Enemies.
TEST(SohApFunctionSignatures, EnumParamTypesMatchRenderEnumValueClasses) {
	std::string out = generateFunctions(
		"extern define affordable(check: Location) -> Bool\n"
		"define probe(item: Item, check: Location, t: Trick, e: Enemy):\n"
		"    has(item)\n");
	EXPECT_NE(out.find("def probe(bundle, item: Items, check: Locations, t: Tricks, e: Enemies) -> bool:"),
		std::string::npos) << out;
}

// WaterLevel is a normal RLS enum, so writeEnums generates a WaterLevel class and the
// annotation names that class -- rather than the Events class its values used to be folded
// into. Confirms the type name still tracks renderEnumValue (the old `RandoWaterLeve` typo
// is gone) and that generated enums annotate under their own name.
TEST(SohApFunctionSignatures, DeclaredEnumParamAnnotatesAsItsOwnClass) {
	EXPECT_NE(generateFunctions(
		"define at_level(wl: WaterLevel):\n"
		"    true\n").find("def at_level(bundle, wl: WaterLevel) -> bool:"),
		std::string::npos);
}

// A Condition parameter is a thunk invoked as cond(bundle): it takes the bundle and returns
// a Rule, so it annotates as Callable[[tuple[Regions, "SohWorld"]], Rule].
TEST(SohApFunctionSignatures, ConditionParamAnnotatesAsCallable) {
	EXPECT_NE(generateFunctions(
		"define gate(cond: Condition):\n"
		"    cond()\n").find(
		"def gate(bundle, cond: Callable[[tuple[Regions, \"SohWorld\"]], Rule]) -> bool:"),
		std::string::npos);
}

TEST(SohApFunctionSignatures, BoolDefaultIsSuppliedAtCallSite) {
	std::string out = generateFunctions(
		"define guard(flag: Bool = false):\n"
		"    has(RG_HOOKSHOT)\n"
		"define caller(): guard()\n");
	EXPECT_NE(out.find("def guard(bundle, flag: bool) -> bool:"), std::string::npos) << out;
	EXPECT_NE(out.find("return guard(bundle, False)"), std::string::npos) << out;
	EXPECT_EQ(out.find("False_()"), std::string::npos) << out;
}

TEST(SohApFunctionSignatures, EnumDefaultIsSuppliedAtCallSite) {
	const auto out = generateFunctions(
		"define at(distance: Distance = ED_CLOSE):\n"
		"    has(RG_HOOKSHOT)\n"
		"define caller(): at()\n");
	EXPECT_NE(out.find("def at(bundle, distance: EnemyDistance) -> bool:"), std::string::npos) << out;
	EXPECT_NE(out.find("return at(bundle, EnemyDistance.ED_CLOSE)"), std::string::npos) << out;
}

TEST(SohApFunctionSignatures, RecursiveValueHelpersUseValueBridgingAndNegation) {
	auto project = resolveFromSource(
		"define countdown(n: Int) -> Int: n <= 0 ? 0 : countdown(n - 1)\n"
		"define zero(n: Int) -> Bool: n <= 0 ? n == 0 : zero(n - 1)\n"
		"define check(): countdown(2) == 0 and true\n"
		"define negated(): not zero(2)\n");
	TestSohApTranspiler transpiler(project);
	MemoryWriter writer;
	transpiler.GenerateFunctionDefinitionsSource(writer);
	const auto out = writer.content("functions_gen.py");
	EXPECT_TRUE(transpiler.Diagnostics().empty());
	EXPECT_NE(out.find("return True_() if countdown(bundle, 2) == 0 else False_()"), std::string::npos) << out;
	EXPECT_NE(out.find("return not zero(bundle, 2)"), std::string::npos) << out;
}

TEST(SohApFunctionSignatures, RecursiveDefaultsAreExpandedOnlyAtCallSites) {
	auto project = resolveFromSource(
		"define first(n: Int = first(0)) -> Int: n <= 1 ? 1 : first()\n"
		"define second(n: Int = third(0)) -> Int: n\n"
		"define third(n: Int = second(0)) -> Int: n\n"
		"define caller(): first() + second() + third()\n");
	TestSohApTranspiler transpiler(project);
	MemoryWriter writer;
	transpiler.GenerateFunctionDefinitionsSource(writer);
	const auto out = writer.content("functions_gen.py");
	EXPECT_TRUE(transpiler.Diagnostics().empty());
	for (const auto* name : {"first", "second", "third"}) {
		EXPECT_NE(out.find(std::string("def ") + name + "(bundle, n: int) -> int:"), std::string::npos) << out;
	}
	EXPECT_NE(out.find("return 1 if n <= 1 else first(bundle, first(bundle, 0))"), std::string::npos) << out;
	EXPECT_NE(out.find("return first(bundle, first(bundle, 0)) + second(bundle, third(bundle, 0)) + third(bundle, second(bundle, 0))"),
		std::string::npos) << out;
}

// wallet_capacity is supplied natively by the host (it is folded into can_afford_slot), so it is
// skipped entirely -- not emitted, and its otherwise-unrepresentable body raises no diagnostic
// because it is never lowered. has_bottle is NOT host-provided: `bottle_count() >= 1` is covered
// by the kThresholdRewrites row, so it generates like any other define.
TEST(SohApFunctionSignatures, HostProvidedDefinesAreSkipped) {
	auto project = resolveFromSource(
		"extern define bottle_count() -> Int\n"
		"define has_bottle():\n"
		"    bottle_count() >= 1\n"
		"\n"
		"define wallet_capacity():\n"
		"    has(RG_TYCOON_WALLET) ? 999 : 0\n"
		"\n"
		"define can_climb():\n"
		"    has(RG_CLIMB)\n");
	TestSohApTranspiler transpiler(project);
	MemoryWriter writer;
	transpiler.GenerateFunctionDefinitionsSource(writer);
	std::string out = writer.content("functions_gen.py");

	EXPECT_NE(out.find("def has_bottle(bundle) -> bool:\n    return has_bottle_count(bundle, 1)"),
		std::string::npos) << out;
	EXPECT_EQ(out.find("def wallet_capacity("), std::string::npos) << out;
	EXPECT_NE(out.find("def can_climb(bundle) -> bool:"), std::string::npos) << out;
	EXPECT_TRUE(transpiler.Diagnostics().empty());
}
