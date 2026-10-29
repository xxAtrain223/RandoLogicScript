#include <gtest/gtest.h>

#include <limits>
#include <type_traits>

#include "parser.h"
#include "sema.h"
#include "tracker_ir.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

static_assert(!std::is_convertible_v<NodeHandle, NodeId>);
static_assert(!std::is_convertible_v<HelperHandle, NodeHandle>);
static_assert(!std::is_convertible_v<AdapterHandle, HelperHandle>);

TEST(SohTrackerIR, KeepsConsumerCountWiderThanIndex) {
	const NodeId last{std::numeric_limits<uint32_t>::max()};
	const NodeCount count{uint64_t{last.value} + 1};
	EXPECT_EQ(count.value, 4'294'967'296ULL);
}

TEST(SohTrackerIR, ChecksExactInt32BoundariesBeforeNarrowing) {
	const ast::Span span{"tracker_values.rls", {3, 5}, {3, 15}};
	const int64_t minimum = std::numeric_limits<int32_t>::min();
	const int64_t maximum = std::numeric_limits<int32_t>::max();
	for (const int64_t value : {minimum, int64_t{-1}, int64_t{0}, maximum}) {
		const auto result = CheckInt32Value(value, span);
		ASSERT_TRUE(std::holds_alternative<int32_t>(result));
		EXPECT_EQ(std::get<int32_t>(result), value);
	}
	for (const int64_t value : {minimum - 1, maximum + 1, std::numeric_limits<int64_t>::min(),
		std::numeric_limits<int64_t>::max()}) {
		const auto result = CheckInt32Value(value, span);
		ASSERT_TRUE(std::holds_alternative<ast::Diagnostic>(result));
		const auto& diagnostic = std::get<ast::Diagnostic>(result);
		EXPECT_EQ(diagnostic.level, ast::DiagnosticLevel::Error);
		EXPECT_NE(diagnostic.message.find(std::to_string(value)), std::string::npos);
		EXPECT_EQ(diagnostic.span.file, span.file);
		EXPECT_EQ(diagnostic.span.start.line, 3);
		EXPECT_EQ(diagnostic.span.start.column, 5);
	}
}

TEST(SohTrackerIR, ReadsAnalyzedExpressionAndParameterFacts) {
	ast::Project project;
	project.files.push_back(parser::ParseString(
		"enum Mode { First, Second }\n"
		"define choose(mode: Mode, enabled: Bool, count: Int, condition: Condition):\n"
		"    enabled\n",
		"tracker_types.rls"));
	for (const auto& diagnostic : project.files.front().diagnostics) {
		ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
	}
	for (const auto& diagnostic : sema::analyze(project)) {
		ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
	}

	const auto* declaration = project.DefineDecls.at("choose");
	ASSERT_EQ(declaration->params.size(), 4);
	const auto result = GetValueType(project, *declaration->body);
	ASSERT_TRUE(std::holds_alternative<ValueType>(result));
	EXPECT_EQ(std::get<ValueType>(result).sourceType, ast::Type::Bool);
	EXPECT_EQ(std::get<IntegralTarget>(std::get<ValueType>(result).representation),
		IntegralTarget::Bool);

	const auto enumResult = GetValueType(project, declaration->params[0]);
	ASSERT_TRUE(std::holds_alternative<ValueType>(enumResult));
	const auto& enumType = std::get<ValueType>(enumResult);
	EXPECT_EQ(enumType.sourceType, ast::Type::Enum);
	EXPECT_EQ(enumType.enumInfo, project.getEnumInfo("Mode"));
	EXPECT_EQ(std::get<IntegralTarget>(enumType.representation), IntegralTarget::Int32);

	const auto boolResult = GetValueType(project, declaration->params[1]);
	ASSERT_TRUE(std::holds_alternative<ValueType>(boolResult));
	EXPECT_EQ(std::get<IntegralTarget>(std::get<ValueType>(boolResult).representation),
		IntegralTarget::Bool);
	const auto intResult = GetValueType(project, declaration->params[2]);
	ASSERT_TRUE(std::holds_alternative<ValueType>(intResult));
	EXPECT_EQ(std::get<IntegralTarget>(std::get<ValueType>(intResult).representation),
		IntegralTarget::Int32);
	const auto conditionResult = GetValueType(project, declaration->params[3]);
	ASSERT_TRUE(std::holds_alternative<ValueType>(conditionResult));
	EXPECT_TRUE(std::holds_alternative<ConditionTarget>(
		std::get<ValueType>(conditionResult).representation));
}

TEST(SohTrackerIR, MapsAnalyzedKeysToInt32WithoutReclassifyingSourceTypes) {
	ast::Project project;
	auto expression = ast::makeExpr(ast::IntLiteral{0});
	for (const auto type : {ast::Type::Int, ast::Type::Region,
		ast::Type::Event, ast::Type::Location}) {
		project.setType(expression.get(), type);
		const auto result = GetValueType(project, *expression);
		ASSERT_TRUE(std::holds_alternative<ValueType>(result));
		const auto& valueType = std::get<ValueType>(result);
		EXPECT_EQ(valueType.sourceType, type);
		EXPECT_EQ(valueType.enumInfo, nullptr);
		EXPECT_EQ(std::get<IntegralTarget>(valueType.representation), IntegralTarget::Int32);
	}
}

TEST(SohTrackerIR, DiagnosesMissingAndUnsupportedSemanticFacts) {
	ast::Project project;
	auto expression = ast::makeExpr(ast::IntLiteral{0},
		ast::Span{"tracker_invalid.rls", {2, 3}, {2, 4}});
	const auto expectError = [&](const ValueTypeResult& result, std::string_view text) {
		ASSERT_TRUE(std::holds_alternative<ast::Diagnostic>(result));
		const auto& diagnostic = std::get<ast::Diagnostic>(result);
		EXPECT_EQ(diagnostic.level, ast::DiagnosticLevel::Error);
		EXPECT_EQ(diagnostic.span.file, expression->span.file);
		EXPECT_EQ(diagnostic.span.start.line, 2);
		EXPECT_EQ(diagnostic.span.start.column, 3);
		EXPECT_NE(diagnostic.message.find(text), std::string::npos);
	};

	expectError(GetValueType(project, *expression), "analyzed value type");
	project.setType(expression.get(), ast::Type::Enum);
	expectError(GetValueType(project, *expression), "enum identity");
	project.setEnumType(expression.get(), "Missing");
	expectError(GetValueType(project, *expression), "Missing");

	for (const auto& [type, name] : {
		std::pair{ast::Type::String, "String"},
		std::pair{ast::Type::List, "List"},
		std::pair{ast::Type::Callable, "Callable"},
		std::pair{ast::Type::Void, "Void"},
		std::pair{ast::Type::Error, "Error"}}) {
		project.setType(expression.get(), type);
		expectError(GetValueType(project, *expression), name);
	}
}

TEST(SohTrackerIR, StoresAllSevenIntegralConversionTargets) {
	const IntegralTarget targets[] = {IntegralTarget::Bool, IntegralTarget::Int8,
		IntegralTarget::Int16, IntegralTarget::Int32, IntegralTarget::UInt8,
		IntegralTarget::UInt16, IntegralTarget::UInt32};
	for (const auto target : targets) {
		const Payload payload = Conversion{target, NodeHandle{42}};
		EXPECT_EQ(std::get<Conversion>(payload).target, target);
		EXPECT_EQ(std::get<Conversion>(payload).operand, NodeHandle{42});
	}
	static_assert(!std::is_constructible_v<Conversion, ConditionTarget, NodeHandle>);
}

TEST(SohTrackerIR, ChainCollectionPreservesIntermediateRepresentationBarriers) {
	ast::Project project;
	project.files.push_back(parser::ParseString(
		"define chain(a: Int, b: Int, c: Int): (a + b) + c\n", "tracker_chain.rls"));
	for (const auto& diagnostic : project.files[0].diagnostics) {
		ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
	}
	for (const auto& diagnostic : sema::analyze(project)) {
		ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
	}
	const auto& expression = *project.DefineDecls.at("chain")->body;
	const auto& binary = std::get<ast::BinaryExpr>(expression.node);
	const auto flattened = CollectChainSources(project, expression);
	ASSERT_TRUE(std::holds_alternative<ChainSources>(flattened));
	EXPECT_EQ(std::get<ChainSources>(flattened).operands.size(), 3);

	// Synthetic analyzed facts exercise a representation barrier the source grammar
	// cannot currently express as a same-operator arithmetic intermediate.
	project.setType(binary.left.get(), ast::Type::Bool);
	const auto collected = CollectChainSources(project, expression);
	ASSERT_TRUE(std::holds_alternative<ChainSources>(collected));
	const auto& sources = std::get<ChainSources>(collected);
	EXPECT_EQ(sources.provenance.expressions, (std::vector<const ast::Expr*>{&expression}));
	EXPECT_EQ(sources.operands, (std::vector<const ast::Expr*>{binary.left.get(), binary.right.get()}));
}

TEST(SohTrackerIR, ChainCollectionDiagnosesMissingMetadataAndIncompleteBinarySources) {
	ast::Project project;
	project.files.push_back(parser::ParseString(
		"define chain(): 1 + 2\n", "tracker_chain.rls"));
	ASSERT_TRUE(project.files[0].diagnostics.empty());
	auto& expression = *std::get<ast::DefineDecl>(project.files[0].declarations[0]).body;
	const auto missing = CollectChainSources(project, expression);
	ASSERT_TRUE(std::holds_alternative<ast::Diagnostic>(missing));
	EXPECT_NE(std::get<ast::Diagnostic>(missing).message.find("analyzed value type"), std::string::npos);
	EXPECT_EQ(std::get<ast::Diagnostic>(missing).span.file, expression.span.file);

	project.setType(&expression, ast::Type::Int);
	std::get<ast::BinaryExpr>(expression.node).left.reset();
	const auto incomplete = CollectChainSources(project, expression);
	ASSERT_TRUE(std::holds_alternative<ast::Diagnostic>(incomplete));
	EXPECT_NE(std::get<ast::Diagnostic>(incomplete).message.find("complete binary expression"), std::string::npos);
	EXPECT_EQ(std::get<ast::Diagnostic>(incomplete).span.file, expression.span.file);
}

TEST(SohTrackerIR, PreservesOrderedAndRepeatedNaryOperands) {
	const std::vector<NodeHandle> children{{3}, {1}, {3}, {2}};
	const Payload logical = Logical{LogicalOperator::And, children};
	const Payload arithmetic = Arithmetic{ArithmeticOperator::Divide, children};
	EXPECT_EQ(std::get<Logical>(logical).children, children);
	EXPECT_EQ(std::get<Arithmetic>(arithmetic).children, children);
}

TEST(SohTrackerIR, RepresentsForwardAndCyclicHelperReferencesWithoutInlining) {
	const ast::DefineDecl first{ast::Name{"first"}, {}, nullptr};
	const ast::DefineDecl second{ast::Name{"second"}, {}, nullptr};
	Graph graph;
	graph.helpers.push_back({&first, std::nullopt});
	graph.helpers.push_back({&second, std::nullopt});

	const ValueType boolType{ast::Type::Bool, nullptr, IntegralTarget::Bool};
	graph.nodes.push_back({HelperCall{HelperHandle{1}, {}}, boolType, {}});
	graph.nodes.push_back({HelperCall{HelperHandle{0}, {}}, boolType, {}});
	graph.helpers[0].body = NodeHandle{0};
	graph.helpers[1].body = NodeHandle{1};

	ASSERT_EQ(graph.nodes.size(), 2);
	ASSERT_EQ(graph.helpers.size(), 2);
	EXPECT_EQ(graph.helpers[0].declaration, &first);
	EXPECT_EQ(std::get<HelperCall>(graph.nodes[0].payload).definition, HelperHandle{1});
	EXPECT_EQ(std::get<HelperCall>(graph.nodes[1].payload).definition, HelperHandle{0});
	EXPECT_EQ(graph.helpers[0].body, NodeHandle{0});
	EXPECT_EQ(graph.helpers[1].body, NodeHandle{1});
}

TEST(SohTrackerIR, RetainsStructuralMatchAndConversionBoundaries) {
	const Payload patterns = MatchPatterns{{}};
	EXPECT_TRUE(std::get<MatchPatterns>(patterns).patterns.empty());
	const Payload arm = MatchArm{NodeHandle{0}, NodeHandle{1}, true};
	EXPECT_TRUE(std::get<MatchArm>(arm).fallThrough);
	const Payload match = Match{IntegralTarget::Int32, NodeHandle{2}, {{3}, {4}}};
	EXPECT_EQ(std::get<Match>(match).arms, (std::vector<NodeHandle>{{3}, {4}}));
	const Payload conversion = Conversion{IntegralTarget::Bool, NodeHandle{5}};
	EXPECT_EQ(std::get<Conversion>(conversion).operand, NodeHandle{5});
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
