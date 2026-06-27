// Tests for generated function-definition signatures (§6.2/§6.3 of
// docs/AP-Function-Generation.md): the `bundle` receiver is the first parameter, and each
// parameter/return type annotation is the SoH Python type produced by pythonTypeName --
// which is pinned to renderEnumValue's enum class, so a value `Items.RG_FOO` always
// annotates as `Items`. SoH-specific because pythonTypeName/renderEnumValue are SoH hooks.
#include "helpers.h"

using namespace rls::transpilers::soh_ap_tests;

namespace {
// Exposes the protected function-generation building block so a test can capture the
// emitted functions.gen.py without wiring it into Transpile() (Phase 5).
class TestSohApTranspiler : public rls::transpilers::soh_ap::SohApTranspiler {
public:
	using rls::transpilers::soh_ap::SohApTranspiler::SohApTranspiler;
	using rls::transpilers::soh_ap::SohApTranspiler::GenerateFunctionDefinitionsSource;
};
} // namespace

// Generate functions.gen.py from inline RLS source and return its contents.
static std::string generateFunctions(const std::string& source) {
	auto project = resolveFromSource(source);
	TestSohApTranspiler transpiler(project);
	MemoryWriter writer;
	transpiler.GenerateFunctionDefinitionsSource(writer);
	return writer.content("functions.gen.py");
}

// The bundle receiver leads the parameter list, ahead of the RLS parameters.
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
// values with: Item -> Items, Check -> Locations, Trick -> Tricks, Enemy -> Enemies.
TEST(SohApFunctionSignatures, EnumParamTypesMatchRenderEnumValueClasses) {
	std::string out = generateFunctions(
		"extern define affordable(check: Check) -> Bool\n"
		"define probe(item: Item, check: Check, t: Trick, e: Enemy):\n"
		"    has(item)\n");
	EXPECT_NE(out.find("def probe(bundle, item: Items, check: Locations, t: Tricks, e: Enemies) -> bool:"),
		std::string::npos) << out;
}

// WaterLevel annotates as Events (its values live in the Events enum), confirming the old
// `RandoWaterLeve` typo is gone now that the type name is derived from renderEnumValue.
TEST(SohApFunctionSignatures, WaterLevelParamAnnotatesAsEvents) {
	EXPECT_NE(generateFunctions(
		"define at_level(wl: WaterLevel):\n"
		"    true\n").find("def at_level(bundle, wl: Events) -> bool:"),
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

// A default for a value (Bool) parameter uses Python's True/False, not the True_()/False_()
// rule literals -- a default binds like a call argument (see GenerateCallArgument), and the
// frozen build-time value of a bool parameter is a plain bool.
TEST(SohApFunctionSignatures, BoolParamDefaultUsesPythonLiteral) {
	std::string out = generateFunctions(
		"define guard(flag: Bool = false):\n"
		"    has(RG_HOOKSHOT)\n");
	EXPECT_NE(out.find("def guard(bundle, flag: bool = False) -> bool:"), std::string::npos) << out;
	EXPECT_EQ(out.find("False_()"), std::string::npos) << out;
}

// An enum-valued default renders through renderEnumValue, so it carries the enum class prefix.
TEST(SohApFunctionSignatures, EnumParamDefaultIsPrefixed) {
	EXPECT_NE(generateFunctions(
		"define at(distance: Distance = ED_CLOSE):\n"
		"    has(RG_HOOKSHOT)\n").find(
		"def at(bundle, distance: EnemyDistance = EnemyDistance.ED_CLOSE) -> bool:"),
		std::string::npos);
}

// Defines the host world supplies natively (has_bottle is a hand-written LogicHelpers rule;
// wallet_capacity is folded into can_afford_slot) are skipped entirely -- not emitted, and
// wallet_capacity's otherwise-unrepresentable body raises no diagnostic because it is never
// lowered.
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
	std::string out = writer.content("functions.gen.py");

	EXPECT_EQ(out.find("def has_bottle("), std::string::npos) << out;
	EXPECT_EQ(out.find("def wallet_capacity("), std::string::npos) << out;
	EXPECT_NE(out.find("def can_climb(bundle) -> bool:"), std::string::npos) << out;
	EXPECT_TRUE(transpiler.Diagnostics().empty());
}
