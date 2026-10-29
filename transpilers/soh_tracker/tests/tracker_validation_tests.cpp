#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

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

TEST_F(SohTrackerValidation, RequiresChainSourceOperatorProvenance) {
	const auto boolean = add(Value{true}, boolType);
	const auto chain = add(Logical{LogicalOperator::And, {boolean, boolean}}, boolType);
	expectError("chain is missing source operator provenance", &span);

	graph.nodes[chain.slot].provenance.expressions.push_back(nullptr);
	expectError("chain is missing source operator provenance", &span);
}

TEST_F(SohTrackerValidation, RejectsReorderedAndRepeatedOperandsEvenWhenTypesMatch) {
	lower("region RR_TEST { locations { RC_TEST: true and false and true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto root = graph.roots[0].expression;
	auto& chain = std::get<Logical>(graph.nodes[root.slot].payload);
	const auto original = chain.children;
	std::swap(chain.children[0], chain.children[1]);
	expectError("chain operand order/provenance", &graph.nodes[root.slot].provenance.span);
	chain.children = original;
	chain.children[1] = chain.children[0];
	expectError("chain operand order/provenance");
}

TEST_F(SohTrackerValidation, RejectsChangedChainOperatorAndMissingFlattenedOccurrences) {
	lower("region RR_TEST { locations { RC_TEST: true or (false or true) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	auto& node = graph.nodes[graph.roots[0].expression.slot];
	auto& chain = std::get<Logical>(node.payload);
	chain.operation = LogicalOperator::And;
	expectError("chain operator differs from its source");
	chain.operation = LogicalOperator::Or;
	ASSERT_EQ(node.provenance.expressions.size(), 2);
	node.provenance.expressions.pop_back();
	expectError("chain flattened operator provenance");
}

TEST_F(SohTrackerValidation, RejectsFlatteningRightNestedArithmeticForEveryOperator) {
	for (const auto* operation : {"+", "-", "*", "/"}) {
		SCOPED_TRACE(operation);
		project = {};
		graph = {};
		lower(std::string("region RR_TEST { locations { RC_TEST: 24 ") +
			operation + " (3 " + operation + " 2) } }\n");
		ASSERT_FALSE(HasFatalFailure());
		expectSuccess();
		const auto root = std::get<Conversion>(graph.nodes[graph.roots[0].expression.slot].payload).operand;
		auto& parent = std::get<Arithmetic>(graph.nodes[root.slot].payload);
		const auto& nested = std::get<Arithmetic>(graph.nodes[parent.children[1].slot].payload);
		const auto children = nested.children;
		parent.children.pop_back();
		parent.children.insert(parent.children.end(), children.begin(), children.end());
		expectError("chain operand count violates source grouping");
	}
}

TEST_F(SohTrackerValidation, RejectsCrossingMixedOperatorAndLogicalBoundaries) {
	lower("region RR_TEST { locations { RC_TEST: true and (false or true) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	auto& parent = std::get<Logical>(graph.nodes[graph.roots[0].expression.slot].payload);
	const auto& nested = std::get<Logical>(graph.nodes[parent.children[1].slot].payload);
	parent.children[1] = nested.children[0];
	expectError("chain operand order/provenance");

	project = {};
	graph = {};
	lower("region RR_TEST { locations { RC_TEST: (24 / 3) * 2 } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto root = std::get<Conversion>(graph.nodes[graph.roots[0].expression.slot].payload).operand;
	auto& arithmetic = std::get<Arithmetic>(graph.nodes[root.slot].payload);
	const auto division = arithmetic.children[0];
	arithmetic.children[0] = std::get<Arithmetic>(graph.nodes[division.slot].payload).children[0];
	expectError("chain operand order/provenance");
}

TEST_F(SohTrackerValidation, RejectsFlatteningAcrossHelperAndExternCalls) {
	for (const bool external : {false, true}) {
		SCOPED_TRACE(external);
		project = {};
		graph = {};
		lower(std::string(external ? "extern define query(value: Int) -> Int\n" :
			"define query(value: Int): value + 1\n") +
			"region RR_TEST { locations { RC_TEST: query(2 + 3) + 4 } }\n");
		ASSERT_FALSE(HasFatalFailure());
		expectSuccess();
		const auto root = std::get<Conversion>(graph.nodes[graph.roots[0].expression.slot].payload).operand;
		auto& chain = std::get<Arithmetic>(graph.nodes[root.slot].payload);
		const auto& call = graph.nodes[chain.children[0].slot];
		const auto& parameters = external ? std::get<ExternCall>(call.payload).parameters :
			std::get<HelperCall>(call.payload).parameters;
		chain.children[0] = std::get<Parameter>(graph.nodes[parameters[0].slot].payload).value;
		expectError("chain operand order/provenance");
	}
}

TEST_F(SohTrackerValidation, RejectsRemovedOrBypassedIntermediateNormalization) {
	lower(
		"extern define enabled() -> Bool\n"
		"region RR_TEST { locations { RC_TEST: (enabled() ? 99 : 1) and false } }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectSuccess();
	const auto root = graph.roots[0].expression;
	auto& chain = std::get<Logical>(graph.nodes[root.slot].payload);
	const auto normalized = chain.children[0];
	auto& conversion = std::get<Conversion>(graph.nodes[normalized.slot].payload);
	const auto source = conversion.operand;
	chain.children[0] = source;
	expectError("chain operand is missing its source normalization conversion");
	chain.children[0] = normalized;
	conversion.operand = std::get<Ternary>(graph.nodes[source.slot].payload).trueBranch;
	expectError("chain conversion bypasses its source operand boundary");
}

TEST_F(SohTrackerValidation, ValidatesDynamicChainsAndPotentiallyThrowingOperandsStructurally) {
	lower(
		"extern define value() -> Int\n"
		"region RR_TEST { locations {\n"
		"    RC_A: value() / 0 / value()\n"
		"    RC_B: value() - value() - value()\n"
		"    RC_C: value() + (value() + value())\n"
		"    RC_D: false and (value() / 0 > 1)\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectSuccess();
}

TEST_F(SohTrackerValidation, PreservesExactInt32LiteralExtremes) {
	lower("region RR_TEST { locations { RC_TEST: -2147483648 < 2147483647 } }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectSuccess();
	const auto& comparison = std::get<Comparison>(graph.nodes[graph.roots[0].expression.slot].payload);
	EXPECT_EQ(std::get<int32_t>(std::get<Value>(graph.nodes[comparison.left.slot].payload).value),
		std::numeric_limits<int32_t>::min());
	EXPECT_EQ(std::get<int32_t>(std::get<Value>(graph.nodes[comparison.right.slot].payload).value),
		std::numeric_limits<int32_t>::max());
}

TEST_F(SohTrackerValidation, PreservesNormalizedEnumExtremesWithoutReassigningValues) {
	lower(
		"enum Limit { Lowest = -2147483648, Highest = 2147483647 }\n"
		"extern define consume(value: Limit) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: consume(Limit.Highest) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectSuccess();
	expectSuccess();
	const auto& entries = project.EnumInfos.at("Limit").entries;
	ASSERT_EQ(entries.size(), 2);
	EXPECT_EQ(std::get<ast::EnumMemberInfo>(entries[0]).value, std::numeric_limits<int32_t>::min());
	EXPECT_EQ(std::get<ast::EnumMemberInfo>(entries[1]).value, std::numeric_limits<int32_t>::max());
}

TEST_F(SohTrackerValidation, PreservesSemaApprovedEnumIntConversionsAtCallBoundaries) {
	lower(
		"enum Choice { FIRST = 1, SECOND = 2 }\n"
		"extern define take_enum(value: Choice) -> Bool\n"
		"extern define take_int(value: Int) -> Bool\n"
		"define relay(value: Choice): take_int(value)\n"
		"region RR_TEST { locations { RC_TEST: take_enum(1) and relay(2) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectSuccess();
}

TEST_F(SohTrackerValidation, RejectsMissingNormalizedValueEvenOnAnUnusedMemberOfAUsedEnum) {
	lower(
		"enum Choice { FIRST, SECOND }\n"
		"extern define consume(value: Choice) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: consume(FIRST) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	auto& member = std::get<ast::EnumMemberInfo>(project.EnumInfos.at("Choice").entries[1]);
	member.value.reset();
	expectError("enum 'Choice' member 'SECOND' is missing its normalized value", &member.span);
	EXPECT_FALSE(member.value);
}

TEST_F(SohTrackerValidation, ValidatesEnumMetadataUsedOnlyByAnAdapterSignature) {
	lower(
		"enum Choice { FIRST, SECOND }\n"
		"extern define current() -> Choice\n"
		"region RR_TEST { locations { RC_TEST: current() == 0 } }\n");
	ASSERT_FALSE(HasFatalFailure());
	auto& info = project.EnumInfos.at("Choice");
	auto& member = std::get<ast::EnumMemberInfo>(info.entries[0]);
	member.value.reset();
	expectError("member 'FIRST' is missing its normalized value", &member.span);
	member.value = 0;
	info.underlyingType = ast::Type::String;
	expectError("enum 'Choice' has unsupported analyzed enum metadata", &info.span);
}

TEST_F(SohTrackerValidation, DoesNotValidateDeterministicallyUnusedEnumDeclarations) {
	lower(
		"enum Unused { FIRST, SECOND }\n"
		"region RR_TEST { locations { RC_TEST: true } }\n");
	ASSERT_FALSE(HasFatalFailure());
	std::get<ast::EnumMemberInfo>(project.EnumInfos.at("Unused").entries[1]).value.reset();
	expectSuccess();
}

TEST_F(SohTrackerValidation, RetainsUnknownExternOrdinalsAsSymbolicValues) {
	lower(
		"extern enum Item { RG_BOW, RG_* }\n"
		"extern define has(value: Item) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: has(RG_BOW) and has(RG_HOOKSHOT) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	expectSuccess();
	const auto& info = project.EnumInfos.at("Item");
	EXPECT_FALSE(std::get<ast::EnumMemberInfo>(info.entries[0]).value);
	EXPECT_TRUE(std::holds_alternative<ast::EnumPatternInfo>(info.entries[1]));
}

TEST_F(SohTrackerValidation, RejectsUnknownAndSubstitutedNormalEnumMembers) {
	lower(
		"enum Choice { FIRST, SECOND }\n"
		"extern define consume(value: Choice) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: consume(Choice.FIRST) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto root = graph.roots[0].expression;
	const auto parameter = std::get<ExternCall>(graph.nodes[root.slot].payload).parameters[0];
	const auto value = std::get<Parameter>(graph.nodes[parameter.slot].payload).value;
	auto& enumeration = std::get<EnumConstant>(std::get<Value>(graph.nodes[value.slot].payload).value);
	const ast::Name absent{"MISSING"};
	enumeration.member = &absent;
	expectError("enum constant 'MISSING' is absent from its analyzed enum");
	const ast::Name substituted{"SECOND"};
	enumeration.member = &substituted;
	expectError("enum constant differs from its source member");
}

TEST_F(SohTrackerValidation, RejectsSubstitutedLiteralAndIdentifierValues) {
	lower(
		"extern define price(value: Int) -> Bool\n"
		"region RR_TEST { locations {\n"
		"    RC_A: true\n"
		"    RC_B: price(42)\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto boolean = graph.roots[0].expression;
	std::get<Value>(graph.nodes[boolean.slot].payload).value = false;
	expectError("Bool literal differs from its source value");
	std::get<Value>(graph.nodes[boolean.slot].payload).value = true;
	const auto& call = std::get<ExternCall>(graph.nodes[graph.roots[1].expression.slot].payload);
	const auto integer = std::get<Parameter>(graph.nodes[call.parameters[0].slot].payload).value;
	std::get<Value>(graph.nodes[integer.slot].payload).value = int32_t{43};
	expectError("Int literal differs from its source value");
}

TEST_F(SohTrackerValidation, RejectsHereSubstitutionEvenWhenBothRegionsAreRegistered) {
	lower(
		"extern define at(value: Region) -> Bool\n"
		"region RR_FIRST { locations { RC_TEST: at(here) } }\n"
		"region RR_SECOND {}\n");
	ASSERT_FALSE(HasFatalFailure());
	const auto& call = std::get<ExternCall>(graph.nodes[graph.roots[0].expression.slot].payload);
	const auto value = std::get<Parameter>(graph.nodes[call.parameters[0].slot].payload).value;
	std::get<HereValue>(std::get<Value>(graph.nodes[value.slot].payload).value).region =
		project.RegionDecls.at("RR_SECOND");
	expectError("here value differs from its sema-resolved source region");
}

TEST_F(SohTrackerValidation, RejectsSubstitutedRootConditionsWithTheSameResultType) {
	lower("region RR_TEST { locations { RC_FIRST: true\n RC_SECOND: false } }\n");
	ASSERT_FALSE(HasFatalFailure());
	graph.roots[0].expression = graph.roots[1].expression;
	expectError("root 'RC_FIRST' differs from its source condition");
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
