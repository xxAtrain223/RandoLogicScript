#include <gtest/gtest.h>

#include <algorithm>

#include "parser.h"
#include "sema.h"
#include "tracker_lowering.h"
#include "tracker_validation.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

class SohTrackerValidation : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_validation";
	const ast::Span span{"validation.rls", {7, 4}, {7, 12}};
	const ValueType boolType{ast::Type::Bool, nullptr, IntegralTarget::Bool};
	const ValueType intType{ast::Type::Int, nullptr, IntegralTarget::Int32};
	const ValueType conditionType{ast::Type::Condition, nullptr, ConditionTarget{}};
	ast::Project project;
	Graph graph;

	void lower(const std::string& source) {
		project.files.push_back(parser::ParseString(source, (sourceRoot / "validation.rls").string()));
		for (const auto& diagnostic : project.files.back().diagnostics) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error)
				<< diagnostic.span.start.line << ":" << diagnostic.span.start.column << " " << diagnostic.message;
		}
		for (const auto& diagnostic : sema::analyze(project)) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
		}
		auto lowered = LowerRoots(project, sourceRoot);
		if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&lowered)) {
			for (const auto& error : *errors) ADD_FAILURE() << error.message;
			return;
		}
		graph = std::get<Graph>(std::move(lowered));
	}

	NodeHandle add(Payload payload, std::optional<ValueType> type) {
		const NodeHandle handle{graph.nodes.size()};
		graph.nodes.push_back({std::move(payload), type, {span, {}}});
		return handle;
	}

	void expectSuccess() const {
		const auto validation = ValidateGraph(project, graph, sourceRoot);
		for (const auto& error : validation) ADD_FAILURE() << error.message;
	}

	void expectError(std::string_view text, const ast::Span* expectedSpan = nullptr) const {
		const auto errors = ValidateGraph(project, graph, sourceRoot);
		const auto found = std::find_if(errors.begin(), errors.end(),
			[&](const ast::Diagnostic& diagnostic) {
				return diagnostic.level == ast::DiagnosticLevel::Error &&
					diagnostic.message.find(text) != std::string::npos;
			});
		ASSERT_NE(found, errors.end()) << text;
		if (expectedSpan) {
			EXPECT_EQ(found->span.file, expectedSpan->file);
			EXPECT_EQ(found->span.start.line, expectedSpan->start.line);
			EXPECT_EQ(found->span.start.column, expectedSpan->start.column);
		}
	}
};

TEST_F(SohTrackerValidation, EmptyGraphReturnsNoDiagnostics) {
	const std::vector<ast::Diagnostic> diagnostics = ValidateGraph(project, graph, sourceRoot);
	EXPECT_TRUE(diagnostics.empty());
}

TEST_F(SohTrackerValidation, ValidatesAllOrdinaryPayloadsBeforeSeparateNumberingAndRendering) {
	lower(
		"enum Choice { FIRST, SECOND }\n"
		"extern define host(value: Int, condition: Condition = false) -> Int\n"
		"extern define live() -> Bool\n"
		"define select(value: Choice):\n"
		"    match value { FIRST or SECOND: true\n"
		"        _: false }\n"
		"region RR_TEST { locations {\n"
		"    RC_A: not false and (select(FIRST) or live())\n"
		"    RC_B: (true ? host(1 + 2 * 3 / (4 - 1)) : 0) > 2\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	const std::vector<ast::Diagnostic> diagnostics = ValidateGraph(project, graph, sourceRoot);
	ASSERT_TRUE(diagnostics.empty());
	const auto numbering = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(numbering));
	EXPECT_EQ(std::get<Numbering>(numbering).count.value, graph.nodes.size());
	const auto rendering = RenderHostAdapters(graph);
	ASSERT_TRUE(std::holds_alternative<std::vector<RenderedAdapter>>(rendering));
	const auto& adapters = std::get<std::vector<RenderedAdapter>>(rendering);
	ASSERT_EQ(adapters.size(), 2);
	for (const auto& adapter : adapters) {
		EXPECT_EQ(adapter.symbol, graph.adapters[adapter.handle.slot].declaration->name.text + "_adapter");
		EXPECT_FALSE(adapter.definition.empty());
	}
}

TEST_F(SohTrackerValidation, ChecksInvalidReferencesEvenInUnreachableDescriptors) {
	add(Not{NodeHandle{99}}, boolType);
	expectError("invalid node handle", &span);
	graph.nodes[0].payload = HelperCall{HelperHandle{99}, {}};
	expectError("invalid helper handle", &span);
	graph.nodes[0].payload = ExternCall{AdapterHandle{99}, {}};
	expectError("invalid host adapter handle", &span);
}

TEST_F(SohTrackerValidation, RejectsIncompleteDefinitionsBeforeDereferencingThem) {
	graph.helpers.push_back({nullptr, std::nullopt});
	expectError("helper is missing its declaration");
	graph.helpers.clear();
	graph.adapters.push_back({nullptr, boolType, {}});
	expectError("host adapter is missing its declaration");
	graph.adapters.clear();
	lower(
		"define helper(): true\n"
		"region RR_TEST { locations { RC_TEST: helper() } }\n");
	ASSERT_FALSE(HasFatalFailure());
	graph.helpers[0].body.reset();
	expectError("helper definition is incomplete");
}

TEST_F(SohTrackerValidation, ChecksNaryArityAndRepresentationsWithoutPruningBranches) {
	const auto boolean = add(Value{true}, boolType);
	const auto integer = add(Value{int32_t{2}}, intType);
	const auto chain = add(Logical{LogicalOperator::And, {boolean, integer}}, boolType);
	expectError("chain operand", &span);
	graph.nodes[chain.slot].payload = Logical{LogicalOperator::And, {boolean}};
	expectError("at least two children", &span);
	graph.nodes[chain.slot] = {Arithmetic{ArithmeticOperator::Add, {integer, boolean}}, intType, {span, {}}};
	expectError("chain operand", &span);
	graph.nodes[chain.slot].payload = Arithmetic{ArithmeticOperator::Add, {}};
	expectError("at least two children", &span);
}

TEST_F(SohTrackerValidation, RejectsUnsupportedAndMissingValueTypesOnEveryStoredNode) {
	const auto value = add(Value{true}, std::nullopt);
	expectError("missing or unsupported payload value type", &span);
	for (const auto source : {ast::Type::String, ast::Type::List, ast::Type::Callable,
		ast::Type::Void, ast::Type::Error, ast::Type::Enum}) {
		SCOPED_TRACE(static_cast<int>(source));
		graph.nodes[value.slot].valueType = ValueType{source, nullptr, IntegralTarget::Bool};
		expectError("missing or unsupported payload value type", &span);
	}
	graph.nodes[value.slot].valueType = intType;
	expectError("Bool literal", &span);
	graph.nodes[value.slot].payload = Value{int32_t{1}};
	graph.nodes[value.slot].valueType = boolType;
	expectError("Int literal", &span);
}

TEST_F(SohTrackerValidation, PreservesIntegralAndConditionBoundaries) {
	const auto integer = add(Value{int32_t{1}}, intType);
	const auto boolean = add(Value{true}, boolType);
	const auto condition = add(Condition{boolean}, conditionType);
	const auto operation = add(Conversion{IntegralTarget::Bool, integer}, boolType);
	expectSuccess();
	graph.nodes[operation.slot].payload = Conversion{IntegralTarget::Bool, condition};
	expectError("operand must be an integral", &span);
	graph.nodes[operation.slot].payload = Invoke{integer};
	expectError("invocation callable", &span);
	graph.nodes[operation.slot].payload = Not{integer};
	expectError("not operand", &span);
	graph.nodes[operation.slot].payload = Comparison{ComparisonOperator::Equal, integer, condition};
	expectError("operand must be an integral", &span);
	graph.nodes[condition.slot].payload = Condition{integer};
	expectError("condition body", &span);
}

TEST_F(SohTrackerValidation, AcceptsAllSevenExplicitIntegralTargets) {
	const auto integer = add(Value{int32_t{1}}, intType);
	for (const auto target : {IntegralTarget::Bool, IntegralTarget::Int8, IntegralTarget::Int16,
		IntegralTarget::Int32, IntegralTarget::UInt8, IntegralTarget::UInt16, IntegralTarget::UInt32}) {
		add(Conversion{target, integer}, ValueType{
			target == IntegralTarget::Bool ? ast::Type::Bool : ast::Type::Int, nullptr, target});
	}
	expectSuccess();
}

TEST_F(SohTrackerValidation, ChecksTernaryAndMatchChildContracts) {
	const auto integer = add(Value{int32_t{1}}, intType);
	const auto boolean = add(Value{true}, boolType);
	const auto ternary = add(Ternary{boolean, boolean, integer}, boolType);
	expectError("ternary false branch", &span);
	graph.nodes[ternary.slot].payload = Ternary{integer, boolean, boolean};
	expectError("ternary condition", &span);
	graph.nodes[ternary.slot].payload = Ternary{boolean, boolean, boolean};
	const auto patterns = add(MatchPatterns{{integer}}, std::nullopt);
	const auto arm = add(MatchArm{patterns, boolean, false}, std::nullopt);
	const auto match = add(Match{IntegralTarget::Bool, integer, {arm}}, boolType);
	expectSuccess();
	graph.nodes[match.slot].payload = Match{IntegralTarget::Bool, integer, {boolean}};
	expectError("match child must be a match arm", &span);
	graph.nodes[match.slot].payload = Match{IntegralTarget::Int32, integer, {arm}};
	graph.nodes[match.slot].valueType = intType;
	expectError("match arm body", &span);
	graph.nodes[arm.slot].payload = MatchArm{boolean, integer, false};
	expectError("requires a pattern-list node", &span);
	graph.nodes[patterns.slot].valueType = boolType;
	expectError("payload value type", &span);
}

TEST_F(SohTrackerValidation, RejectsInvalidOperatorsAndIntegralTargets) {
	const auto boolean = add(Value{true}, boolType);
	const auto invalid = add(Logical{static_cast<LogicalOperator>(99), {boolean, boolean}}, boolType);
	expectError("unsupported operator", &span);
	graph.nodes[invalid.slot].payload = Arithmetic{static_cast<ArithmeticOperator>(99), {boolean, boolean}};
	expectError("unsupported operator", &span);
	graph.nodes[invalid.slot].payload = Comparison{static_cast<ComparisonOperator>(99), boolean, boolean};
	expectError("unsupported operator", &span);
	graph.nodes[invalid.slot].payload = Conversion{static_cast<IntegralTarget>(99), boolean};
	expectError("unsupported integral target", &span);
	graph.nodes[invalid.slot].payload = Match{static_cast<IntegralTarget>(99), boolean, {}};
	expectError("unsupported integral target", &span);
}

TEST_F(SohTrackerValidation, ChecksCallWrapperCountOrderAndDeclarationIdentity) {
	lower(
		"define choose(first: Bool, second: Bool = false): first or second\n"
		"region RR_TEST { locations { RC_TEST: choose(true) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	auto& call = std::get<HelperCall>(graph.nodes[graph.roots[0].expression.slot].payload);
	const auto original = call.parameters;
	std::swap(call.parameters[0], call.parameters[1]);
	expectError("choose' requires ordered declaration-specific");
	call.parameters = original;
	auto& wrapper = std::get<Parameter>(graph.nodes[call.parameters[0].slot].payload);
	wrapper.position = 1;
	expectError("choose' requires ordered declaration-specific");
	wrapper.position = 0;
	wrapper.declaration = nullptr;
	expectError("parameter is missing its declaration");
	wrapper.declaration = &project.DefineDecls.at("choose")->params[0];
	call.parameters.pop_back();
	expectError("choose' has an incomplete parameter list");
}

TEST_F(SohTrackerValidation, ChecksAdapterParameterTypesAndCallResult) {
	lower(
		"extern define host(value: Bool) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: host(true) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	graph.adapters[0].parameters[0] = intType;
	expectError("adapter 'host' parameter");
	graph.adapters[0].parameters[0] = boolType;
	graph.adapters[0].result = intType;
	expectError("extern call 'host' result");
	graph.adapters[0].result = boolType;
	graph.adapters.push_back(graph.adapters[0]);
	expectError("duplicate adapter handles");
}

TEST_F(SohTrackerValidation, DiagnosesMissingHostMappingWithoutRendering) {
	lower(
		"extern enum Custom { CUSTOM_* }\n"
		"extern define host(value: Custom) -> Bool\n"
		"extern define live() -> Bool\n"
		"region RR_TEST { locations { RC_TEST: live() and host(CUSTOM_ONE) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectError("host': missing supported host type mapping");
	const std::vector<HostEnumBinding> mappings{{"Custom", "HostCustom"}};
	EXPECT_TRUE(ValidateGraph(project, graph, sourceRoot, mappings).empty());
}

TEST_F(SohTrackerValidation, ValidatesUnreferencedAdapterResultRepresentations) {
	const ast::ExternDefineDecl declaration{ast::Name{"host"}, {}};
	project.ExternDefineDecls.emplace("host", &declaration);
	graph.adapters.push_back({&declaration,
		ValueType{ast::Type::String, nullptr, IntegralTarget::Int32}, {}});
	expectError("unsupported result type/representation");
}

TEST_F(SohTrackerValidation, ChecksMissingAnalyzedMetadataRatherThanRebindingCalls) {
	lower(
		"extern define host(value: Bool = false) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: false and host() } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto& root = *graph.roots[0].entry->condition;
	const auto& call = std::get<ast::CallExpr>(std::get<ast::BinaryExpr>(root.node).right->node);
	project.setResolvedCallArgs(&call, {nullptr});
	expectError("host' has a missing analyzed argument");
	project.setResolvedCallArgs(&call, {});
	expectError("host' differs from its analyzed argument metadata");
	project.setResolvedCallArgs(&call, {project.ExternDefineDecls.at("host")->params[0].defaultValue.get()});
	project.setType(&root, ast::Type::Error);
	expectError("analyzed Error type");
}

TEST_F(SohTrackerValidation, ChecksRootResultDuplicateIdentityAndOwner) {
	lower("region RR_TEST { locations { RC_TEST: true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	graph.nodes[graph.roots[0].expression.slot].valueType = intType;
	expectError("RC_TEST' requires a Bool result");
	graph.nodes[graph.roots[0].expression.slot].valueType = boolType;
	graph.roots.push_back(graph.roots[0]);
	expectError("duplicate root identity");
	graph.roots.pop_back();
	const auto root = graph.roots[0];
	graph.roots.clear();
	expectError("missing root 'RC_TEST'");
	graph.roots.push_back(root);
	graph.roots[0].kind = ast::SectionKind::Events;
	expectError("RC_TEST' differs from its source record");
	graph.roots[0] = root;
	project.RegionDecls.clear();
	expectError("RC_TEST' is missing its analyzed owner");
}

TEST_F(SohTrackerValidation, ChecksBorrowedValueIdentities) {
	const auto value = add(Value{ParameterRead{nullptr}}, boolType);
	expectError("parameter read is missing its declaration", &span);
	graph.nodes[value.slot].payload = Value{EnumConstant{nullptr, nullptr}};
	expectError("enum constant is missing", &span);
	graph.nodes[value.slot].payload = Value{KeyConstant{nullptr}};
	expectError("key constant is missing", &span);
	graph.nodes[value.slot].payload = Value{HereValue{nullptr}};
	expectError("here value is missing", &span);
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
