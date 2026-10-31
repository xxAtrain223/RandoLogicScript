#include <gtest/gtest.h>

#include <algorithm>
#include <locale>
#include <type_traits>

#include "parser.h"
#include "sema.h"
#include "tracker_emission.h"
#include "tracker_graph.h"
#include "tracker_lowering.h"
#include "tracker_test_writer.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

size_t occurrences(const std::string& text, std::string_view needle) {
	size_t count = 0;
	for (size_t offset = 0; (offset = text.find(needle, offset)) != std::string::npos;
		offset += needle.size()) ++count;
	return count;
}

class SohTrackerEmission : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_emission";
	ast::Project project;
	Graph graph;

	void parse(std::string_view text, const std::string& path = "source.rls") {
		project.files.push_back(parser::ParseString(std::string(text), (sourceRoot / path).string()));
		for (const auto& error : project.files.back().diagnostics) {
			ASSERT_NE(error.level, ast::DiagnosticLevel::Error) << error.message;
		}
	}

	void lower() {
		for (const auto& error : sema::analyze(project)) {
			ASSERT_NE(error.level, ast::DiagnosticLevel::Error) << error.message;
		}
		auto result = LowerRoots(project, sourceRoot);
		if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result)) {
			for (const auto& error : *errors) ADD_FAILURE() << error.message;
			return;
		}
		graph = std::get<Graph>(std::move(result));
	}

	test::CapturedOutput render(std::span<const HostEnumBinding> customEnums = {}) const {
		test::MemoryWriter writer;
		const auto diagnostics = EmitArtifacts(project, graph, writer, sourceRoot, customEnums);
		for (const auto& error : diagnostics) ADD_FAILURE() << error.message;
		if (!diagnostics.empty()) return {};
		return writer.capture();
	}

	void expectAccountedFor(const test::CapturedOutput& artifacts) const {
		const auto numbered = NumberDistinctGraph(graph, sourceRoot);
		ASSERT_TRUE(std::holds_alternative<Numbering>(numbered));
		const auto& numbering = std::get<Numbering>(numbered);
		EXPECT_EQ(occurrences(artifacts.source, "\tcase "), numbering.count.value);
		EXPECT_EQ(occurrences(artifacts.source, "{LogicRootKey{"), graph.roots.size());
		for (size_t i = 0; i < numbering.nodesById.size(); ++i) {
			EXPECT_EQ(occurrences(artifacts.source, "\tcase " + std::to_string(i) + ":"), 1);
		}
		for (const auto& root : graph.roots) {
			const auto identity = "\"" + root.owner->key.text + "\", \"" + root.entry->name.text +
				"\", 0}, " + std::to_string(numbering.idsBySlot[root.expression.slot]->value) + "}";
			EXPECT_NE(artifacts.source.find(identity), std::string::npos) << identity;
		}
	}
};

TEST_F(SohTrackerEmission, EmptyGraphHasExplicitMapAndThrowingFactoryWithoutDeducedArrays) {
	const auto artifacts = render();
	ASSERT_FALSE(HasFailure());
	EXPECT_NE(artifacts.header.find("#include \"logic_expression.h\""), std::string::npos);
	EXPECT_NE(artifacts.header.find("uint64_t GetExpressionCount();"), std::string::npos);
	EXPECT_EQ(artifacts.header.find("struct LogicRoot"), std::string::npos);
	EXPECT_NE(artifacts.source.find("return 0;"), std::string::npos);
	EXPECT_NE(artifacts.source.find("static const LogicRootMap roots{\n\t};"), std::string::npos);
	EXPECT_NE(artifacts.source.find("default: throw std::out_of_range("), std::string::npos);
	EXPECT_EQ(artifacts.source.find("[] ="), std::string::npos);
	EXPECT_NE(artifacts.source.find("#include \"functions.gen.h\""), std::string::npos);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, MinimalFactoriesAndRegistryNeverResolveOrEvaluateDescendants) {
	parse("region RR_TEST { locations { RC_A: true } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto artifacts = render();
	EXPECT_NE(artifacts.source.find(
		"case 0: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t "
		"{ return true; }, \"true\", false);"), std::string::npos);
	EXPECT_NE(artifacts.source.find("return 1;"), std::string::npos);
	EXPECT_EQ(occurrences(artifacts.source, "LogicExpression* GetExpression(index_t index)"), 1);
	EXPECT_EQ(occurrences(artifacts.source, "uint64_t GetExpressionCount()"), 1);
	EXPECT_EQ(occurrences(artifacts.source, "const LogicRootMap& GetLogicRoots()"), 1);
	EXPECT_NE(artifacts.source.find("static LogicExpressionCache cache(GetExpressionCount(), "
		"CreateExpression);"), std::string::npos);
	const auto factory = artifacts.source.substr(artifacts.source.find("static std::unique_ptr"));
	const auto factoryOnly = factory.substr(0, factory.find("\nuint64_t GetExpressionCount()"));
	EXPECT_EQ(factoryOnly.find("GetExpression("), std::string::npos);
	EXPECT_EQ(factoryOnly.find("Evaluate("), std::string::npos);
	const auto roots = artifacts.source.substr(artifacts.source.find("const LogicRootMap& GetLogicRoots()"));
	EXPECT_EQ(roots.find("GetExpression("), std::string::npos);
	EXPECT_EQ(roots.find("source.rls"), std::string::npos);
	EXPECT_EQ(artifacts.source.find("ResolveChildren"), std::string::npos);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, EmitsEveryPayloadWithTypedStructuralConstructorsAndCompleteRoots) {
	parse(
		"enum Choice { FIRST, SECOND }\n"
		"extern define host(value: Int, condition: Condition = false) -> Int\n"
		"extern define live() -> Bool\n"
		"define select(value: Choice): match value { FIRST or SECOND: true\n"
		"    _: false }\n"
		"define invoke(condition: Condition): condition()\n"
		"region RR_TEST { events { EV_TEST: live() } locations {\n"
		"    RC_A: not false and (select(FIRST) or live())\n"
		"    RC_B: (true ? host(1 + 2 * 3 / (4 - 1)) : 0) > 2\n"
		"    RC_C: invoke(true)\n"
		"} exits { RR_OTHER: true } }\n"
		"region RR_OTHER { locations { RC_A: false } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto artifacts = render();
	for (const auto* type : {"ValueExpression", "AndExpression", "OrExpression", "NotExpression",
		"ArithmeticExpression", "ComparisonExpression", "TernaryExpression", "FunctionCallExpression",
		"ExternFunctionCallExpression", "ParameterExpression", "ConditionExpression", "InvokeExpression",
		"MatchExpression<bool>", "MatchArm", "MatchArmPatterns"}) {
		EXPECT_NE(artifacts.source.find("std::make_unique<" + std::string(type) + ">"),
			std::string::npos) << type;
	}
	EXPECT_NE(artifacts.source.find("scope.get(\"value\")"), std::string::npos);
	EXPECT_NE(artifacts.source.find("std::make_unique<MatchArmPatterns>(std::vector<index_t>{})"),
		std::string::npos);
	EXPECT_NE(artifacts.source.find("#include \"functions.gen.h\""), std::string::npos);
	EXPECT_NE(artifacts.source.find("LogicRootKind::Event, \"RR_TEST\", \"EV_TEST\""), std::string::npos);
	EXPECT_NE(artifacts.source.find("LogicRootKind::Exit, \"RR_TEST\", \"RR_OTHER\""), std::string::npos);
	EXPECT_NE(artifacts.source.find("LogicRootKind::Location, \"RR_OTHER\", \"RC_A\""), std::string::npos);
	EXPECT_EQ(graph.roots.size(), 6);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, UsesCheckedSymbolicHostValuesAndActualHostReturnTypes) {
	parse(
		"extern enum Item { RG_* }\n"
		"extern enum Setting { RSK_* }\n"
		"extern enum Custom { CUSTOM_* }\n"
		"extern define setting(key: Setting) -> Int\n"
		"extern define custom(key: Custom, value: Condition) -> Custom\n"
		"region RR_TEST { events { EV_TEST: true } locations {\n"
		"    RC_A: setting(RSK_TEST) > 0 and custom(CUSTOM_ONE, true) == CUSTOM_ONE\n"
		"    RC_B: RG_BOW == RG_BOW and here == here and EV_TEST == EV_TEST and RC_A == RC_A\n"
		"} }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const std::vector<HostEnumBinding> mappings{{"Custom", "HostCustom"}};
	const auto artifacts = render(mappings);
	for (const auto* symbol : {"RSK_TEST", "HostCustom::CUSTOM_ONE", "RandomizerGet::RG_BOW",
		"RandomizerRegion::RR_TEST", "LogicVal::EV_TEST", "RandomizerCheck::RC_A"}) {
		EXPECT_NE(artifacts.source.find("return CheckedHostInt(" + std::string(symbol) + ");"),
			std::string::npos) << symbol;
	}
	EXPECT_EQ(artifacts.source.find("RandomizerSettingKey::RSK_TEST"), std::string::npos);
	EXPECT_NE(artifacts.source.find("return CheckedHostInt(setting("), std::string::npos);
	EXPECT_NE(artifacts.source.find("return CheckedHostInt(custom("), std::string::npos);
	EXPECT_NE(artifacts.source.find("return int32_t{0};"), std::string::npos);
	EXPECT_EQ(artifacts.source.find("LL}"), std::string::npos);
	EXPECT_NE(artifacts.source.find("MakeConditionCallback(arguments[1])"), std::string::npos);
	EXPECT_NE(artifacts.source.find("std::underlying_type_t<T>"), std::string::npos);
	EXPECT_NE(artifacts.source.find("std::conditional_t<std::is_signed_v<T>, intmax_t, uintmax_t>"),
		std::string::npos);
	const auto check = artifacts.source.find("if (!std::in_range<int32_t>(static_cast<Wide>(value)))");
	const auto narrowing = artifacts.source.find("return static_cast<int32_t>(value)");
	ASSERT_NE(check, std::string::npos);
	EXPECT_LT(check, narrowing);
	EXPECT_NE(artifacts.source.find("throw std::out_of_range(\"Tracker host value"), std::string::npos);
	EXPECT_NE(artifacts.source.find("std::is_same_v<T, bool>"), std::string::npos);
	EXPECT_NE(artifacts.source.find("#include \"functions.gen.h\""), std::string::npos);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, EmitsAllSevenConversionAndMatchTargetsFromSyntheticDescriptors) {
	// ABI shape fixture, not a source-language or runtime conversion test.
	parse("region RR_TEST { locations { RC_A: true\nRC_B: true\nRC_C: true\nRC_D: true\n"
		"RC_E: true\nRC_F: true\nRC_G: true } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto provenance = graph.nodes[graph.roots[0].expression.slot].provenance;
	const auto add = [&](Payload payload, std::optional<ValueType> type) {
		const NodeHandle handle{graph.nodes.size()};
		graph.nodes.push_back({std::move(payload), type, {provenance.span, {}}});
		return handle;
	};
	const auto one = add(Value{int32_t{1}}, ValueType{ast::Type::Int, nullptr, IntegralTarget::Int32});
	const auto patterns = add(MatchPatterns{{}}, std::nullopt);
	size_t rootIndex = 0;
	for (const auto target : {IntegralTarget::Bool, IntegralTarget::Int8, IntegralTarget::Int16,
		IntegralTarget::Int32, IntegralTarget::UInt8, IntegralTarget::UInt16, IntegralTarget::UInt32}) {
		const ValueType type{target == IntegralTarget::Bool ? ast::Type::Bool : ast::Type::Int,
			nullptr, target};
		const auto converted = add(Conversion{target, one}, type);
		const auto arm = add(MatchArm{patterns, converted, true}, std::nullopt);
		const auto match = add(Match{target, one, {arm}}, type);
		graph.nodes[graph.roots[rootIndex++].expression.slot].payload =
			Conversion{IntegralTarget::Bool, match};
	}
	const auto artifacts = render();
	for (const auto* target : {"bool", "int8_t", "int16_t", "int32_t", "uint8_t", "uint16_t", "uint32_t"}) {
		EXPECT_NE(artifacts.source.find("std::make_unique<ConversionExpression<" + std::string(target) + ">>"),
			std::string::npos);
		EXPECT_NE(artifacts.source.find("std::make_unique<MatchExpression<" + std::string(target) + ">>"),
			std::string::npos);
	}
	EXPECT_NE(artifacts.source.find(", true);"), std::string::npos);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, EscapesRuntimeNamesAndUtf8WithoutTemporaryStringBacking) {
	parse("define same(value: Bool): value\nregion RR_TEST { locations { RC_A: same(true) } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	auto& declaration = std::get<ast::DefineDecl>(project.files[0].declarations[0]);
	const std::string name = "quote\"\\\n\t\0019" "\303\251";
	declaration.params[0].name.text = name;
	std::get<ast::Identifier>(declaration.body->node).name.text = name;
	const auto artifacts = render();
	const std::string escaped = "\"quote\\\"\\\\\\012\\011\\0019\\303\\251\"";
	EXPECT_NE(artifacts.source.find("scope.get(" + escaped + ")"), std::string::npos);
	EXPECT_NE(artifacts.source.find("ParameterExpression>(0, " + escaped), std::string::npos);
	EXPECT_EQ(artifacts.source.find("std::string("), std::string::npos);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, RejectsInvalidUnreachableDescriptorsAndMissingBindingsWithoutArtifacts) {
	parse(
		"extern enum Custom { CUSTOM_* }\nextern define host(value: Custom) -> Bool\n"
		"region RR_TEST { locations { RC_A: host(CUSTOM_ONE) } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	test::MemoryWriter writer;
	const auto missing = EmitArtifacts(project, graph, writer, sourceRoot);
	EXPECT_FALSE(missing.empty());
	EXPECT_EQ(writer.opens, 0);
	const std::vector<HostEnumBinding> mappings{{"Custom", "HostCustom"}};
	graph.nodes.push_back({Not{NodeHandle{999}}, ValueType{ast::Type::Bool, nullptr, IntegralTarget::Bool},
		{graph.roots[0].provenance.span, {}}});
	const auto invalid = EmitArtifacts(project, graph, writer, sourceRoot, mappings);
	ASSERT_FALSE(invalid.empty());
	EXPECT_EQ(writer.opens, 0);
	EXPECT_NE(invalid[0].message.find("invalid node handle"),
		std::string::npos);
}

TEST_F(SohTrackerEmission, ArtifactsAreIdenticalAfterFileAndDescriptorEnumerationChanges) {
	parse("extern define z_host() -> Bool\nextern define a_host() -> Bool\n"
		"define helper(value: Bool): value\n"
		"region RR_TEST { locations { RC_A: helper(z_host()) and a_host() and helper(z_host())\n"
		"RC_COPY: helper(z_host()) and a_host() and helper(z_host()) } }\n", "a.rls");
	parse("extend region RR_TEST { locations { RC_B: false } }\n", "z.rls");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto original = render();
	std::reverse(project.files.begin(), project.files.end());
	// Moving a File preserves its declaration storage; sema pointers remain valid.
	std::reverse(graph.roots.begin(), graph.roots.end());
	std::reverse(graph.adapters.begin(), graph.adapters.end());
	const auto remap = [&](NodeHandle& handle) { handle.slot = graph.nodes.size() - 1 - handle.slot; };
	for (auto& node : graph.nodes) {
		std::visit([&](auto& payload) {
			using T = std::decay_t<decltype(payload)>;
			if constexpr (std::is_same_v<T, Logical> || std::is_same_v<T, Arithmetic>) {
				for (auto& child : payload.children) remap(child);
			} else if constexpr (std::is_same_v<T, HelperCall> || std::is_same_v<T, ExternCall>) {
				for (auto& child : payload.parameters) remap(child);
				if constexpr (std::is_same_v<T, ExternCall>)
					payload.adapter.slot = graph.adapters.size() - 1 - payload.adapter.slot;
			} else if constexpr (std::is_same_v<T, Parameter>) remap(payload.value);
		}, node.payload);
	}
	for (auto& root : graph.roots) remap(root.expression);
	for (auto& helper : graph.helpers) remap(*helper.body);
	std::reverse(graph.nodes.begin(), graph.nodes.end());
	EXPECT_EQ(render(), original);
	expectAccountedFor(original);
}

TEST_F(SohTrackerEmission, ArtifactsAreIdenticalAcrossRelocatedSourceRoots) {
	const auto build = [&](const std::filesystem::path& root) {
		ast::Project local;
		local.files.push_back(parser::ParseString(
			"region RR_TEST { locations { RC_A: 1 + 2 > 0\nRC_B: 1 + 2 > 0 } }\n",
			(root / "source.rls").string()));
		const auto diagnostics = sema::analyze(local);
		EXPECT_TRUE(diagnostics.empty());
		const auto lowered = LowerRoots(local, root);
		EXPECT_TRUE(std::holds_alternative<Graph>(lowered));
		test::MemoryWriter writer;
		const auto emitted = EmitArtifacts(local, std::get<Graph>(lowered), writer, root);
		EXPECT_TRUE(emitted.empty());
		return writer.capture();
	};
	const auto first = build(sourceRoot);
	const auto second = build(sourceRoot / "relocated");
	EXPECT_EQ(first, second);
	EXPECT_EQ(first.header.find(sourceRoot.string()), std::string::npos);
	EXPECT_EQ(first.source.find(sourceRoot.string()), std::string::npos);
}

TEST_F(SohTrackerEmission, GenerationUsesStableLocaleOnCallerStreams) {
	struct GroupedNumbers : std::numpunct<char> {
		char do_thousands_sep() const override { return '_'; }
		std::string do_grouping() const override { return "\1"; }
	};
	parse("region RR_TEST { locations { RC_A: 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 > 0 } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto expected = render();
	test::MemoryWriter writer;
	const std::locale grouped(std::locale::classic(), new GroupedNumbers);
	writer.files["logic_tracker.gen.cpp"].imbue(grouped);
	ASSERT_TRUE(EmitArtifacts(project, graph, writer, sourceRoot).empty());
	EXPECT_EQ(writer.capture(), expected);
}

TEST_F(SohTrackerEmission, RequiresHostMappingsForStandaloneExternalConstantsWithoutAdapters) {
	parse("extern enum Custom { CUSTOM_* }\n"
		"region RR_TEST { locations { RC_A: CUSTOM_ONE == CUSTOM_ONE } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(graph.adapters.empty());
	test::MemoryWriter writer;
	const auto missing = EmitArtifacts(project, graph, writer, sourceRoot);
	ASSERT_FALSE(missing.empty());
	EXPECT_EQ(writer.opens, 0);
	EXPECT_NE(missing[0].message.find(
		"Tracker enum 'Custom' is missing a supported host type mapping"), std::string::npos);
	const std::vector<HostEnumBinding> mappings{{"Custom", "HostCustom"}};
	const auto artifacts = render(mappings);
	EXPECT_NE(artifacts.source.find("return CheckedHostInt(HostCustom::CUSTOM_ONE);"), std::string::npos);
	EXPECT_EQ(artifacts.source.find("return CheckedHostInt(Custom::CUSTOM_ONE)"), std::string::npos);
}

TEST_F(SohTrackerEmission, EmitsAllSixFlattenedChainsWithOrderedIdsAndNoDescendantResolution) {
	parse(
		"extern define a() -> Int\nextern define b() -> Int\nextern define c() -> Int\n"
		"extern define p() -> Bool\nextern define q() -> Bool\nextern define r() -> Bool\n"
		"region RR_TEST { locations {\n"
		"RC_ADD: a() + b() + c() > 0\n"
		"RC_SUB: a() - b() - c() > 0\n"
		"RC_MUL: a() * b() * c() > 0\n"
		"RC_DIV: a() / b() / c() > 0\n"
		"RC_AND: p() and q() and r()\n"
		"RC_OR: p() or q() or r()\n"
		"RC_GROUP: a() / (b() / c()) > 0\n"
		"} }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto artifacts = render();
	const auto numbered = NumberDistinctGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(numbered));
	const auto& numbering = std::get<Numbering>(numbered);
	size_t flattened = 0;
	size_t grouped = 0;
	for (const auto& descriptor : graph.nodes) {
		std::string constructor;
		const std::vector<NodeHandle>* children = nullptr;
		if (const auto* chain = std::get_if<Arithmetic>(&descriptor.payload)) {
			const char* op = chain->operation == ArithmeticOperator::Add ? "+" :
				chain->operation == ArithmeticOperator::Subtract ? "-" :
				chain->operation == ArithmeticOperator::Multiply ? "*" : "/";
			constructor = "std::make_unique<ArithmeticExpression>(\"" + std::string(op) + "\", ";
			children = &chain->children;
		} else if (const auto* chain = std::get_if<Logical>(&descriptor.payload)) {
			constructor = chain->operation == LogicalOperator::And ?
				"std::make_unique<AndExpression>(" : "std::make_unique<OrExpression>(";
			children = &chain->children;
		}
		if (!children) continue;
		flattened += children->size() == 3;
		grouped += children->size() == 2;
		constructor += "std::vector<index_t>{";
		for (size_t i = 0; i < children->size(); ++i) {
			if (i) constructor += ", ";
			constructor += std::to_string(numbering.idsBySlot[(*children)[i].slot]->value);
		}
		constructor += "})";
		EXPECT_NE(artifacts.source.find(constructor), std::string::npos) << constructor;
	}
	EXPECT_EQ(flattened, 6);
	EXPECT_EQ(grouped, 2);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, SharesCallsAndRootsButRetainsArgumentAndRepeatedChildPositions) {
	parse(
		"extern define live() -> Bool\n"
		"define twice(first: Bool, second: Bool): first and second\n"
		"region RR_TEST { locations {\n"
		"RC_A: twice(live(), live())\n"
		"RC_B: twice(live(), live())\n"
		"RC_C: live() and live()\n"
		"} }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto artifacts = render();
	EXPECT_NE(artifacts.source.find("return 8;"), std::string::npos);
	EXPECT_EQ(occurrences(artifacts.source, "std::make_unique<ExternFunctionCallExpression>"), 1);
	EXPECT_EQ(occurrences(artifacts.source, "std::make_unique<FunctionCallExpression>"), 1);
	EXPECT_EQ(occurrences(artifacts.source, "std::make_unique<ParameterExpression>"), 2);
	const auto numbered = NumberDistinctGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(numbered));
	const auto& numbering = std::get<Numbering>(numbered);
	EXPECT_EQ(numbering.idsBySlot[graph.roots[0].expression.slot],
		numbering.idsBySlot[graph.roots[1].expression.slot]);
	const auto& call = std::get<HelperCall>(graph.nodes[graph.roots[0].expression.slot].payload);
	ASSERT_EQ(call.parameters.size(), 2);
	EXPECT_NE(numbering.idsBySlot[call.parameters[0].slot], numbering.idsBySlot[call.parameters[1].slot]);
	const auto& first = std::get<Parameter>(graph.nodes[call.parameters[0].slot].payload);
	const auto& second = std::get<Parameter>(graph.nodes[call.parameters[1].slot].payload);
	EXPECT_EQ(numbering.idsBySlot[first.value.slot], numbering.idsBySlot[second.value.slot]);
	const auto& children = std::get<Logical>(graph.nodes[graph.roots[2].expression.slot].payload).children;
	ASSERT_EQ(children.size(), 2);
	const auto id = std::to_string(numbering.idsBySlot[children[0].slot]->value);
	EXPECT_NE(artifacts.source.find("std::make_unique<AndExpression>(std::vector<index_t>{" +
		id + ", " + id + "})"), std::string::npos);
	EXPECT_EQ(graph.roots.size(), 3);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, SharesConditionStructureWithoutStoringCapturedValuesOrResults) {
	parse("extern define live() -> Bool\n"
		"define invoke(condition: Condition): condition()\n"
		"region RR_TEST { locations { RC_A: invoke(live())\nRC_B: invoke(live()) } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto artifacts = render();
	EXPECT_EQ(occurrences(artifacts.source, "std::make_unique<ConditionExpression>"), 1);
	EXPECT_EQ(occurrences(artifacts.source, "std::make_unique<InvokeExpression>"), 1);
	EXPECT_EQ(artifacts.source.find("ConditionValue"), std::string::npos);
	EXPECT_EQ(artifacts.source.find("EvalResult{"), std::string::npos);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, SharesBindingStructureAcrossHelpersWithoutMergingTheCalls) {
	parse("define first(value: Bool): value\n"
		"define second(value: Bool): value\n"
		"region RR_TEST { locations { RC_A: first(true)\nRC_B: second(true) } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	const auto artifacts = render();
	EXPECT_NE(artifacts.source.find("return 5;"), std::string::npos);
	EXPECT_EQ(occurrences(artifacts.source, "std::make_unique<FunctionCallExpression>"), 2);
	EXPECT_EQ(occurrences(artifacts.source, "std::make_unique<ParameterExpression>"), 1);
	EXPECT_EQ(occurrences(artifacts.source, "return scope.get(\"value\");"), 1);
	expectAccountedFor(artifacts);
}

TEST_F(SohTrackerEmission, ValidatesEachOccurrenceBeforeSharingAndOpeningOutputs) {
	parse("region RR_TEST { locations { RC_A: true\nRC_B: true\nRC_C: false } }\n");
	lower();
	ASSERT_FALSE(HasFatalFailure());
	graph.nodes[graph.roots[1].expression.slot].payload = Value{false};
	test::MemoryWriter writer;
	const auto diagnostics = EmitArtifacts(project, graph, writer, sourceRoot);
	ASSERT_FALSE(diagnostics.empty());
	EXPECT_NE(diagnostics[0].message.find("Bool literal differs from its source value"), std::string::npos);
	EXPECT_EQ(writer.opens, 0);
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
