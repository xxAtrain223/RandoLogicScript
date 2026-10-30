#include <gtest/gtest.h>

#include <algorithm>

#include "parser.h"
#include "sema.h"
#include "tracker_adapters.h"
#include "tracker_lowering.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

class SohTrackerAdapters : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_adapters";
	ast::Project project;
	Graph graph;

	void lower(const std::string& source) {
		project.files.push_back(parser::ParseString(source, (sourceRoot / "adapters.rls").string()));
		for (const auto& diagnostic : project.files.back().diagnostics) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error)
				<< diagnostic.span.start.line << ":" << diagnostic.span.start.column << " " << diagnostic.message;
		}
		for (const auto& diagnostic : sema::analyze(project)) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
		}
		auto result = LowerRoots(project, sourceRoot);
		if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result)) {
			for (const auto& error : *errors) ADD_FAILURE() << error.message;
			return;
		}
		graph = std::get<Graph>(std::move(result));
	}

	const RenderedAdapter& named(const std::vector<RenderedAdapter>& adapters,
		std::string_view name) const
	{
		const auto found = std::find_if(adapters.begin(), adapters.end(),
			[&](const RenderedAdapter& adapter) {
				return graph.adapters[adapter.handle.slot].declaration->name.text == name;
			});
		EXPECT_NE(found, adapters.end());
		return adapters.at(static_cast<size_t>(found - adapters.begin()));
	}

	void expectError(const AdapterRenderingResult& result, std::string_view text,
		const ast::ExternDefineDecl* declaration = nullptr) const
	{
		const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result);
		ASSERT_NE(errors, nullptr);
		const auto found = std::find_if(errors->begin(), errors->end(),
			[&](const ast::Diagnostic& error) {
				return error.level == ast::DiagnosticLevel::Error &&
					error.message.find(text) != std::string::npos;
			});
		ASSERT_NE(found, errors->end()) << text;
		if (declaration) {
			EXPECT_EQ(found->span.start.line, declaration->span.start.line);
			EXPECT_NE(found->message.find(declaration->name.text), std::string::npos);
		}
	}
};

TEST_F(SohTrackerAdapters, RendersBaselineEnumsWidthsAndOriginalCallbackArguments) {
	lower(
		"extern enum Item { RG_* }\n"
		"extern enum Region { RR_* }\n"
		"extern enum Location { RC_* }\n"
		"extern enum Setting { RSK_* }\n"
		"extern define has(item: Item) -> Bool\n"
		"extern define setting(key: Setting) -> Int\n"
		"extern define check_price(check: Location = RC_UNKNOWN_CHECK) -> Int\n"
		"extern define spirit_shared(first_region: Region, first_condition: Condition,\n"
		"    any_age: Bool = false, second_region: Region = RR_NONE,\n"
		"    second_condition: Condition = false, third_region: Region = RR_NONE,\n"
		"    third_condition: Condition = false) -> Bool\n"
		"region RR_TEST { locations {\n"
		"    RC_A: spirit_shared(here, has(RG_BOW))\n"
		"    RC_B: setting(RSK_TEST) > check_price()\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto result = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(result));
	const auto& adapters = std::get<std::vector<RenderedAdapter>>(result);
	ASSERT_EQ(adapters.size(), 4);
	EXPECT_EQ(named(adapters, "spirit_shared").symbol, "spirit_shared_adapter");
	const auto& spirit = named(adapters, "spirit_shared").definition;
	for (const size_t i : {size_t{1}, size_t{4}, size_t{6}}) {
		EXPECT_NE(spirit.find("MakeConditionCallback(arguments[" + std::to_string(i) + "])"),
			std::string::npos);
	}
	EXPECT_NE(spirit.find("ToBool(arguments[2].Result)"), std::string::npos);
	EXPECT_NE(spirit.find("static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[0].Result))"),
		std::string::npos);
	EXPECT_EQ(spirit.find("auto arguments"), std::string::npos);
	const auto& setting = named(adapters, "setting").definition;
	EXPECT_NE(setting.find("return CheckedHostInt(::setting("), std::string::npos);
	EXPECT_EQ(setting.find("uint8_t"), std::string::npos);
	EXPECT_EQ(setting.find("RandomizerSettingKey::"), std::string::npos);
	const auto& has = named(adapters, "has").definition;
	EXPECT_NE(has.find("static_cast<RandomizerGet>(ToInt<int32_t>(arguments[0].Result))"),
		std::string::npos);
	EXPECT_NE(named(adapters, "check_price").definition.find(
		"static_cast<RandomizerCheck>(ToInt<int32_t>(arguments[0].Result))"), std::string::npos);
	const auto& comparison = std::get<Comparison>(graph.nodes[graph.roots[1].expression.slot].payload);
	const auto& price = std::get<ExternCall>(graph.nodes[comparison.right.slot].payload);
	const auto& parameter = std::get<Parameter>(graph.nodes[price.parameters[0].slot].payload);
	EXPECT_EQ(std::get<EnumConstant>(std::get<Value>(
		graph.nodes[parameter.value.slot].payload).value).member->text, "RC_UNKNOWN_CHECK");
}

TEST_F(SohTrackerAdapters, CoversEveryEmbeddedBaselineBindingAndDeduplicatesRepeatedCalls) {
	std::string source =
		"extern enum Item { RG_* }\nextern enum Region { RR_* }\n"
		"extern enum Event { EV_* }\nextern enum Trick { RT_* }\n"
		"extern enum Scene { SC_* }\nextern enum Trial { TR_* }\n";
	const std::vector<std::string> booleanQueries = {
		"is_child", "is_adult", "at_day", "at_night", "is_vanilla", "is_mq",
		"bombchus_enabled", "can_build_rainbow_bridge", "can_trigger_lacs"};
	const std::vector<std::string> integerQueries = {
		"bottle_count", "ocarina_buttons", "effective_health", "hearts", "dungeon_count",
		"stone_count", "medallion_count", "fire_timer", "water_timer", "get_gs_count",
		"get_big_poe_count", "required_triforce_pieces", "collected_triforce_pieces"};
	for (const auto& name : booleanQueries) source += "extern define " + name + "() -> Bool\n";
	for (const auto& name : integerQueries) source += "extern define " + name + "() -> Int\n";
	source +=
		"extern define flag(key: Event) -> Bool\n"
		"extern define trick(key: Trick) -> Bool\n"
		"extern define any_age(condition: Condition) -> Bool\n"
		"extern define spirit_certain_access(reg: Region) -> Bool\n"
		"extern define can_use(item: Item) -> Bool\n"
		"extern define can_plant_bean(reg: Region, bean: Item) -> Bool\n"
		"extern define trial_skipped(trial: Trial) -> Bool\n"
		"extern define small_keys(scene: Scene, count: Int) -> Bool\n"
		"region RR_TEST { locations { RC_A:\n";
	for (const auto& name : booleanQueries) source += name + "() and ";
	for (const auto& name : integerQueries) source += name + "() > 0 and ";
	source +=
		"flag(EV_TEST) and trick(RT_TEST) and any_age(true) and spirit_certain_access(here) and\n"
		"can_use(RG_BOW) and can_use(RG_BOW) and can_plant_bean(here, RG_BEAN) and\n"
		"trial_skipped(TR_TEST) and small_keys(SC_TEST, 2) } }\n";
	lower(source);
	ASSERT_FALSE(HasFatalFailure());
	const auto result = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(result));
	const auto& adapters = std::get<std::vector<RenderedAdapter>>(result);
	EXPECT_EQ(adapters.size(), booleanQueries.size() + integerQueries.size() + 8);
	EXPECT_NE(named(adapters, "small_keys").definition.find("ToInt<int32_t>(arguments[1].Result)"),
		std::string::npos);
	EXPECT_NE(named(adapters, "flag").definition.find("static_cast<LogicVal>"), std::string::npos);
	EXPECT_NE(named(adapters, "trick").definition.find("static_cast<RandomizerTrick>"), std::string::npos);
	EXPECT_NE(named(adapters, "trial_skipped").definition.find("static_cast<TrialKey>"), std::string::npos);
	for (const auto& name : integerQueries) {
		EXPECT_NE(named(adapters, name).definition.find("return CheckedHostInt(::" + name + "())"),
			std::string::npos);
	}
}

TEST_F(SohTrackerAdapters, UsesAnalyzedCustomExternsWithoutRegistrationOrPrefixInference) {
	lower(
		"extern define is_custom(value: Int) -> Bool\n"
		"extern define unused(value: List) -> Bool\n"
		"region RR_TEST { locations { RC_A: is_custom(1) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto result = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(result));
	const auto& adapters = std::get<std::vector<RenderedAdapter>>(result);
	ASSERT_EQ(adapters.size(), 1);
	EXPECT_EQ(adapters[0].symbol, "is_custom_adapter");
	EXPECT_NE(adapters[0].definition.find(
		"return static_cast<bool>(::is_custom(ToInt<int32_t>(arguments[0].Result)))"), std::string::npos);
}

TEST_F(SohTrackerAdapters, SupportsCustomEnumsLocalEnumsAndEnumReturnsWithoutInventingValues) {
	lower(
		"extern enum Custom { CUSTOM_* }\n"
		"enum Local { One, Two }\n"
		"extern define convert(value: Local) -> Custom\n"
		"region RR_TEST { locations { RC_A: convert(One) == CUSTOM_TEST } }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectError(RenderHostAdapters(graph), "missing supported host type mapping",
		project.ExternDefineDecls.at("convert"));
	const std::vector<HostEnumBinding> enums = {{"Custom", "HostCustom"}};
	const auto result = RenderHostAdapters(graph, enums);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(result));
	const auto& output = std::get<std::vector<RenderedAdapter>>(result)[0].definition;
	EXPECT_NE(output.find("static_cast<Local>(ToInt<int32_t>(arguments[0].Result))"), std::string::npos);
	EXPECT_NE(output.find("return CheckedHostInt(::convert("), std::string::npos);
}

TEST_F(SohTrackerAdapters, GeneratesSynchronousCallbacksFromExternConditionTypes) {
	lower(
		"extern define custom(condition: Condition) -> Bool\n"
		"region RR_TEST { locations { RC_A: custom(true) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto inferred = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(inferred));
	const auto& inferredOutput = std::get<std::vector<RenderedAdapter>>(inferred)[0].definition;
	EXPECT_NE(inferredOutput.find("::custom(MakeConditionCallback(arguments[0]))"), std::string::npos);
}

TEST_F(SohTrackerAdapters, RequiresConditionObjectReturnsRatherThanCallbackReturns) {
	lower(
		"extern define custom() -> Condition\n"
		"region RR_TEST { locations { RC_A: custom()() } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto inferred = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(inferred));
	EXPECT_NE(std::get<std::vector<RenderedAdapter>>(inferred)[0].definition.find(
		"return condition_t{::custom()};"), std::string::npos);
}

TEST_F(SohTrackerAdapters, NormalizesCustomIntegerReturnsFromExternDeclarations) {
	lower(
		"extern define custom(value: Int) -> Int\n"
		"region RR_TEST { locations { RC_A: custom(256) > 0 } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto result = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(result));
	EXPECT_NE(std::get<std::vector<RenderedAdapter>>(result)[0].definition.find(
		"return CheckedHostInt(::custom(ToInt<int32_t>(arguments[0].Result)))"),
		std::string::npos);
}

TEST_F(SohTrackerAdapters, MapsDeclaredKeyTypesWithoutEnumDeclarations) {
	lower(
		"extern define custom(reg: Region, ev: Event, loc: Location) -> Region\n"
		"region RR_TEST { events { EV_TEST: true } locations {\n"
		"    RC_A: custom(here, EV_TEST, RC_A) == here\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto result = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(result));
	const auto& output = std::get<std::vector<RenderedAdapter>>(result)[0].definition;
	EXPECT_NE(output.find("static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[0].Result))"),
		std::string::npos);
	EXPECT_NE(output.find("static_cast<LogicVal>(ToInt<int32_t>(arguments[1].Result))"),
		std::string::npos);
	EXPECT_NE(output.find("static_cast<RandomizerCheck>(ToInt<int32_t>(arguments[2].Result))"),
		std::string::npos);
}

TEST_F(SohTrackerAdapters, UsesDeclarationTypesEvenForFamiliarFunctionNames) {
	lower(
		"extern define is_child() -> Bool\n"
		"extern define setting(key: Bool) -> Bool\n"
		"region RR_TEST { locations { RC_A: is_child() and setting(true) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto inferred = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(inferred));
	EXPECT_NE(named(std::get<std::vector<RenderedAdapter>>(inferred), "setting").definition.find(
		"return static_cast<bool>(::setting(ToBool(arguments[0].Result)))"), std::string::npos);
}

TEST_F(SohTrackerAdapters, RejectsInvalidCountsMissingDeclarationsAndRepresentations) {
	lower(
		"extern define is_child() -> Bool\n"
		"region RR_TEST { locations { RC_A: is_child() } }\n");
	ASSERT_FALSE(HasFatalFailure());
	graph.adapters[0].result.representation = IntegralTarget::Int32;
	expectError(RenderHostAdapters(graph), "incompatible return representation");
	graph.adapters[0].result.representation = IntegralTarget::Bool;
	graph.adapters[0].parameters.push_back(graph.adapters[0].result);
	expectError(RenderHostAdapters(graph), "parameter count does not match");
	graph.adapters[0].declaration = nullptr;
	expectError(RenderHostAdapters(graph), "missing its extern declaration");
}

TEST_F(SohTrackerAdapters, ValidatesContractsIndependentlyOfRendering) {
	lower(
		"extern enum Custom { CUSTOM_* }\n"
		"extern define host(value: Custom) -> Bool\n"
		"region RR_TEST { locations { RC_A: host(CUSTOM_ONE) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const std::vector<ast::Diagnostic> missing = ValidateHostAdapters(graph);
	ASSERT_EQ(missing.size(), 1);
	EXPECT_NE(missing[0].message.find("missing supported host type mapping"), std::string::npos);
	const std::vector<HostEnumBinding> mappings{{"Custom", "HostCustom"}};
	EXPECT_TRUE(ValidateHostAdapters(graph, mappings).empty());
	const auto rendering = RenderHostAdapters(graph, mappings);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(rendering));
	ASSERT_EQ(std::get<std::vector<RenderedAdapter>>(rendering).size(), 1);
}

TEST_F(SohTrackerAdapters, RejectsAmbiguousConfigurationAndHandlesEmptyGraphs) {
	const auto empty = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(empty));
	EXPECT_TRUE(std::get<std::vector<RenderedAdapter>>(empty).empty());
	const std::vector<HostEnumBinding> enums = {{"Setting", "Other"}};
	expectError(RenderHostAdapters(graph, enums), "overrides a baseline mapping");
	const std::vector<HostEnumBinding> missing = {{"Custom", ""}};
	expectError(RenderHostAdapters(graph, missing), "mapping is empty");
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
