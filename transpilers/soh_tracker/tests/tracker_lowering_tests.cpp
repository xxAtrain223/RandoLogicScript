#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <type_traits>

#include "parser.h"
#include "sema.h"
#include "tracker_lowering.h"
#include "tracker_validation.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

class SohTrackerLowering : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_lowering";
	ast::Project project;
	Graph graph;
	std::vector<ast::Diagnostic> diagnostics;
	std::optional<NodeHandle> expression;

	void addSource(const std::string& text, const std::string& path = "expressions.rls") {
		project.files.push_back(parser::ParseString(text, (sourceRoot / path).string()));
	}

	void analyze() {
		for (const auto& file : project.files) {
			for (const auto& diagnostic : file.diagnostics) {
				ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
			}
		}
		for (const auto& diagnostic : sema::analyze(project)) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
		}
	}

	bool lowerBody(const std::string& name = "sample") {
		const auto* declaration = project.DefineDecls.at(name);
		ExpressionLowerer lowerer(project, graph, diagnostics);
		expression = lowerer.Lower(*declaration->body, declaration);
		for (const auto& diagnostic : diagnostics) {
			ADD_FAILURE() << diagnostic.message;
		}
		return expression.has_value() && diagnostics.empty();
	}

	template <typename T>
	const T& payload(NodeHandle handle) const {
		return std::get<T>(graph.nodes.at(handle.slot).payload);
	}

	std::string shape(NodeHandle handle) const {
		const auto list = [&](std::string name, const std::vector<NodeHandle>& children) {
			name += "(";
			for (size_t i = 0; i < children.size(); ++i) {
				if (i) name += ",";
				name += shape(children[i]);
			}
			return name + ")";
		};
		return std::visit([&](const auto& node) -> std::string {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, Value>) {
				return std::visit([](const auto& value) -> std::string {
					using V = std::decay_t<decltype(value)>;
					if constexpr (std::is_same_v<V, bool>) return value ? "true" : "false";
					else if constexpr (std::is_same_v<V, int32_t>) return std::to_string(value);
					else if constexpr (std::is_same_v<V, EnumConstant>) return value.member->text;
					else if constexpr (std::is_same_v<V, KeyConstant>) return value.name->text;
					else if constexpr (std::is_same_v<V, ParameterRead>) return value.declaration->name.text;
					else return "here:" + value.region->key.text;
				}, node.value);
			} else if constexpr (std::is_same_v<T, Logical>) {
				return list(node.operation == LogicalOperator::And ? "and" : "or", node.children);
			} else if constexpr (std::is_same_v<T, Arithmetic>) {
				switch (node.operation) {
				case ArithmeticOperator::Add: return list("+", node.children);
				case ArithmeticOperator::Subtract: return list("-", node.children);
				case ArithmeticOperator::Multiply: return list("*", node.children);
				case ArithmeticOperator::Divide: return list("/", node.children);
				}
				return "invalid arithmetic";
			} else if constexpr (std::is_same_v<T, Comparison>) {
				return list("compare", {node.left, node.right});
			} else if constexpr (std::is_same_v<T, Not>) {
				return list("not", {node.operand});
			} else if constexpr (std::is_same_v<T, Conversion>) {
				return list(node.target == IntegralTarget::Bool ? "bool" : "int", {node.operand});
			} else if constexpr (std::is_same_v<T, Ternary>) {
				return list("?:", {node.condition, node.trueBranch, node.falseBranch});
			} else if constexpr (std::is_same_v<T, Match>) {
				auto children = node.arms;
				children.insert(children.begin(), node.discriminant);
				return list(node.target == IntegralTarget::Bool ? "match-bool" : "match-int", children);
			} else if constexpr (std::is_same_v<T, MatchArm>) {
				return list(node.fallThrough ? "fallthrough" : "arm", {node.patterns, node.body});
			} else if constexpr (std::is_same_v<T, MatchPatterns>) {
				return list("patterns", node.patterns);
			} else {
				return "unexpected callable";
			}
		}, graph.nodes.at(handle.slot).payload);
	}

	void expectError(std::string_view text) const {
		EXPECT_TRUE(std::any_of(diagnostics.begin(), diagnostics.end(),
			[&](const ast::Diagnostic& diagnostic) {
				return diagnostic.level == ast::DiagnosticLevel::Error &&
					diagnostic.message.find(text) != std::string::npos;
			})) << text;
	}
};

TEST_F(SohTrackerLowering, FlattensOnlyHomogeneousLeftAssociatedArithmetic) {
	const std::pair<std::string, std::string> cases[] = {
		{"2 + 3 + 4", "+(2,3,4)"},
		{"2 * 3 * 4", "*(2,3,4)"},
		{"20 - 3 - 2", "-(20,3,2)"},
		{"24 / 3 / 4", "/(24,3,4)"},
		{"a + b + c", "+(a,b,c)"},
		{"a * b * c", "*(a,b,c)"},
		{"a - b - c", "-(a,b,c)"},
		{"a / b / c", "/(a,b,c)"},
		{"a / 0 / c", "/(a,0,c)"},
		{"20 - (3 - 2)", "-(20,-(3,2))"},
		{"24 / (3 / 4)", "/(24,/(3,4))"},
		{"2 + (3 + 4)", "+(2,+(3,4))"},
		{"2 * (3 * 4)", "*(2,*(3,4))"},
		{"2 + 3 * 4", "+(2,*(3,4))"},
		{"(2 + 3) * 4", "*(+(2,3),4)"},
		{"a / b * c", "*(/(a,b),c)"},
		{"a - b + c", "+(-(a,b),c)"}};
	std::string source;
	for (size_t i = 0; i < std::size(cases); ++i) {
		source += "define sample" + std::to_string(i) + "(a: Int, b: Int, c: Int): " +
			cases[i].first + "\n";
	}
	addSource(source);
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	for (size_t i = 0; i < std::size(cases); ++i) {
		SCOPED_TRACE(cases[i].first);
		graph = {};
		ASSERT_TRUE(lowerBody("sample" + std::to_string(i)));
		EXPECT_EQ(shape(*expression), cases[i].second);
		EXPECT_EQ(graph.nodes[expression->slot].valueType->sourceType, ast::Type::Int);
		for (const auto& diagnostic : ValidateGraph(project, graph, sourceRoot)) {
			ADD_FAILURE() << diagnostic.message;
		}
	}
}

TEST_F(SohTrackerLowering, FlattensLogicalChainsWithoutCrossingOtherOperatorsOrConversions) {
	addSource(
		"define both(a: Bool, b: Bool, c: Bool, d: Bool): (a and b) and (c and d)\n"
		"define either(a: Bool, b: Bool, c: Bool): a or (b or c)\n"
		"define mixed(a: Bool, b: Bool, c: Bool): a and (b or c)\n"
		"define normalized(n: Int, b: Bool): not n and (n ? 7 : false) and b\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lowerBody("both"));
	EXPECT_EQ(shape(*expression), "and(a,b,c,d)");
	EXPECT_EQ(graph.nodes[expression->slot].provenance.expressions.size(), 3);
	ASSERT_TRUE(lowerBody("either"));
	EXPECT_EQ(shape(*expression), "or(a,b,c)");
	ASSERT_TRUE(lowerBody("mixed"));
	EXPECT_EQ(shape(*expression), "and(a,or(b,c))");
	ASSERT_TRUE(lowerBody("normalized"));
	EXPECT_EQ(shape(*expression), "and(not(bool(n)),?:(bool(n),bool(7),false),b)");
	for (const auto& diagnostic : ValidateGraph(project, graph, sourceRoot)) {
		ADD_FAILURE() << diagnostic.message;
	}
}

TEST_F(SohTrackerLowering, PreservesParameterDeclarationsAndSeparateRepeatedOccurrences) {
	addSource("define sample(a: Int, b: Int, c: Int): a / a / c\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lowerBody());
	const auto& arithmetic = payload<Arithmetic>(*expression);
	ASSERT_EQ(arithmetic.children.size(), 3);
	EXPECT_NE(arithmetic.children[0], arithmetic.children[1]);
	const auto* parameter = &project.DefineDecls.at("sample")->params[0];
	for (size_t i = 0; i < 2; ++i) {
		EXPECT_EQ(std::get<ParameterRead>(payload<Value>(arithmetic.children[i]).value).declaration,
			parameter);
	}
	EXPECT_EQ(graph.nodes.size(), 4);
	EXPECT_EQ(graph.nodes[expression->slot].provenance.expressions.size(), 2);
}

TEST_F(SohTrackerLowering, LowersAllComparisonOperatorsAndEnumNumericOperands) {
	addSource(
		"enum Distance { Close, Far }\n"
		"define equal(): Close is Distance.Far\n"
		"define different(): Close is not 2\n"
		"define less(): Close < 2\n"
		"define atMost(): 2 <= Far\n"
		"define greater(): 2 > 1\n"
		"define atLeast(): 2 >= 1\n"
		"define numeric(): Distance.Close + Far + 2\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	const std::pair<std::string, ComparisonOperator> cases[] = {
		{"equal", ComparisonOperator::Equal}, {"different", ComparisonOperator::NotEqual},
		{"less", ComparisonOperator::Less}, {"atMost", ComparisonOperator::LessEqual},
		{"greater", ComparisonOperator::Greater}, {"atLeast", ComparisonOperator::GreaterEqual}};
	for (const auto& [name, operation] : cases) {
		ASSERT_TRUE(lowerBody(name));
		EXPECT_EQ(payload<Comparison>(*expression).operation, operation);
		EXPECT_EQ(graph.nodes[expression->slot].valueType->sourceType, ast::Type::Bool);
	}
	ASSERT_TRUE(lowerBody("numeric"));
	EXPECT_EQ(shape(*expression), "+(Close,Far,2)");
	const auto& numeric = payload<Arithmetic>(*expression);
	const auto& member = std::get<EnumConstant>(payload<Value>(numeric.children[0]).value);
	EXPECT_EQ(member.declaration, project.getEnumInfo("Distance"));
	EXPECT_EQ(graph.nodes[numeric.children[0].slot].valueType->enumInfo, member.declaration);
}

TEST_F(SohTrackerLowering, NormalizesMixedConditionalBranchesBeforeIntegralConversion) {
	addSource(
		"define sample(enabled: Bool): enabled ? 99 : false\n"
		"define arithmetic(enabled: Bool): (enabled ? 99 : 1) + 2 + 3\n"
		"define nested(a: Bool, b: Bool): a ? 999 : b ? 500 : 0\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lowerBody());
	EXPECT_EQ(shape(*expression), "?:(enabled,bool(99),false)");
	ExpressionLowerer lowerer(project, graph, diagnostics);
	const auto converted = lowerer.Convert(*expression, IntegralTarget::Int32,
		*project.DefineDecls.at("sample")->body);
	ASSERT_TRUE(converted);
	EXPECT_EQ(shape(*converted), "int(?:(enabled,bool(99),false))");
	EXPECT_EQ(graph.nodes[payload<Conversion>(*converted).operand.slot].valueType->sourceType,
		ast::Type::Bool);
	ASSERT_TRUE(lowerBody("arithmetic"));
	EXPECT_EQ(shape(*expression), "+(?:(enabled,99,1),2,3)");
	ASSERT_TRUE(lowerBody("nested"));
	EXPECT_EQ(shape(*expression), "?:(a,999,?:(b,500,0))");
}

TEST_F(SohTrackerLowering, LowersExplicitMatchPatternsDefaultAndFallthrough) {
	addSource(
		"enum Distance { Close, Short, Hookshot }\n"
		"define sample(distance: Distance, sword: Bool, hook: Bool):\n"
		"    match distance {\n"
		"        Close or Short: sword or\n"
		"        Hookshot: hook\n"
		"        _: never\n"
		"    }\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lowerBody());
	EXPECT_EQ(shape(*expression),
		"match-bool(distance,fallthrough(patterns(Close,Short),sword),"
		"arm(patterns(Hookshot),hook),arm(patterns(),false))");
	const auto& match = payload<Match>(*expression);
	ASSERT_EQ(match.arms.size(), 3);
	for (const auto arm : match.arms) {
		EXPECT_FALSE(graph.nodes[arm.slot].valueType.has_value());
		EXPECT_FALSE(graph.nodes[payload<MatchArm>(arm).patterns.slot].valueType.has_value());
	}
}

TEST_F(SohTrackerLowering, KeepsTypedUnmatchedMatchesWithoutSyntheticFallbackArms) {
	addSource(
		"enum Mode { First, Second }\n"
		"define number(x: Mode): match x { First: 99\n"
		"}\n"
		"define mode(x: Mode): match x { First: Mode.First\n"
		"}\n"
		"define mixed(x: Mode): match x { First: 9\n"
		"                               Second: false\n"
		"}\n"
		"define nested(x: Mode): match x { First: match x { Second: 7\n"
		"}\n"
		"                               _: 3\n"
		"}\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lowerBody("number"));
	EXPECT_EQ(shape(*expression), "match-int(x,arm(patterns(First),99))");
	EXPECT_EQ(payload<Match>(*expression).arms.size(), 1);
	ASSERT_TRUE(lowerBody("mode"));
	EXPECT_EQ(payload<Match>(*expression).target, IntegralTarget::Int32);
	EXPECT_EQ(graph.nodes[expression->slot].valueType->enumInfo, project.getEnumInfo("Mode"));
	EXPECT_EQ(payload<Match>(*expression).arms.size(), 1);
	ASSERT_TRUE(lowerBody("mixed"));
	EXPECT_EQ(shape(*expression),
		"match-bool(x,arm(patterns(First),bool(9)),arm(patterns(Second),false))");
	ASSERT_TRUE(lowerBody("nested"));
	EXPECT_EQ(shape(*expression),
		"match-int(x,arm(patterns(First),match-int(x,arm(patterns(Second),7))),arm(patterns(),3))");
}

TEST_F(SohTrackerLowering, LowersEveryComposedRootAndNumbersOnlyEmittedDescriptors) {
	addSource(
		"extend region RR_MQ {\n"
		"    locations { RC_SHARED: here is RR_MQ }\n"
		"    events { LOGIC_CLEAR: 1 }\n"
		"}\n", "a_shuffle.rls");
	addSource(
		"define unused(): \"not executable\"\n"
		"region RR_VANILLA {\n"
		"    data: [\"ignored metadata\"]\n"
		"    locations { RC_SHARED: always }\n"
		"    events { LOGIC_CLEAR: never }\n"
		"    exits { RR_MQ: (2 + 3 + 4) > 0 }\n"
		"}\n"
		"region RR_MQ { exits { RR_VANILLA: false } }\n", "z_base.rls");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	auto result = LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Graph>(result));
	graph = std::get<Graph>(std::move(result));
	ASSERT_EQ(graph.roots.size(), 6);
	EXPECT_TRUE(graph.helpers.empty());
	EXPECT_TRUE(graph.adapters.empty());
	EXPECT_EQ(shape(graph.roots[0].expression), "compare(here:RR_MQ,RR_MQ)");
	EXPECT_EQ(shape(graph.roots[1].expression), "bool(1)");
	const auto& comparison = payload<Comparison>(graph.roots[0].expression);
	EXPECT_EQ(std::get<HereValue>(payload<Value>(comparison.left).value).region,
		project.RegionDecls.at("RR_MQ"));
	EXPECT_EQ(graph.roots[0].owner, project.RegionDecls.at("RR_MQ"));
	EXPECT_EQ(graph.roots[2].owner, project.RegionDecls.at("RR_VANILLA"));
	for (const auto& root : graph.roots) {
		EXPECT_EQ(root.occurrence, 0);
		EXPECT_EQ(graph.nodes[root.expression.slot].valueType->sourceType, ast::Type::Bool);
		EXPECT_EQ(root.provenance.span.file, root.entry->span.file);
	}
	const auto numbered = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(numbered));
	EXPECT_EQ(std::get<Numbering>(numbered).count.value, graph.nodes.size());
	EXPECT_TRUE(std::all_of(std::get<Numbering>(numbered).idsBySlot.begin(),
		std::get<Numbering>(numbered).idsBySlot.end(), [](auto id) { return id.has_value(); }));
}

TEST_F(SohTrackerLowering, RetainsDeclaredKeyCategoriesWithoutGuessingPrefixes) {
	addSource(
		"region Owner {\n"
		"    events { State: true }\n"
		"    locations { Check: State is State }\n"
		"    exits { Other: Check is Check }\n"
		"}\n"
		"region Other {}\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	const auto result = LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Graph>(result));
	graph = std::get<Graph>(result);
	const auto eventRead = payload<Comparison>(graph.roots[1].expression).left;
	const auto locationRead = payload<Comparison>(graph.roots[2].expression).left;
	EXPECT_EQ(graph.nodes[eventRead.slot].valueType->sourceType, ast::Type::Event);
	EXPECT_EQ(graph.nodes[locationRead.slot].valueType->sourceType, ast::Type::Location);
	EXPECT_EQ(std::get<KeyConstant>(payload<Value>(eventRead).value).name->text, "State");
	EXPECT_EQ(std::get<KeyConstant>(payload<Value>(locationRead).value).name->text, "Check");
}

TEST_F(SohTrackerLowering, ConnectsTypedNumericMatchesToBooleanRootsWithoutOrphanNodes) {
	addSource(
		"enum Mode { First, Second }\n"
		"region RR_TEST { locations {\n"
		"    RC_TEST: match First { First: 99 }\n"
		"} }\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	auto result = LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Graph>(result));
	graph = std::get<Graph>(std::move(result));
	ASSERT_EQ(graph.roots.size(), 1);
	EXPECT_EQ(shape(graph.roots[0].expression),
		"bool(match-int(First,arm(patterns(First),99)))");
	const auto& conversion = payload<Conversion>(graph.roots[0].expression);
	EXPECT_EQ(payload<Match>(conversion.operand).target, IntegralTarget::Int32);
	EXPECT_EQ(payload<Match>(conversion.operand).arms.size(), 1);
	const auto numbering = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(numbering));
	EXPECT_EQ(std::get<Numbering>(numbering).count.value, graph.nodes.size());
}

TEST_F(SohTrackerLowering, ReportsMissingSemanticFactsAndExecutableUnsupportedValues) {
	addSource(
		"define sample(a: Int): a + 1\n"
		"define text(): \"unsupported\"\n"
		"define list(): [1, 2]\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ExpressionLowerer lowerer(project, graph, diagnostics);
	auto missing = ast::makeExpr(ast::IntLiteral{1}, ast::Span{"missing.rls", {3, 4}, {3, 5}});
	EXPECT_FALSE(lowerer.Lower(*missing));
	expectError("analyzed value type");
	EXPECT_EQ(diagnostics.back().span.start.line, 3);
	EXPECT_FALSE(lowerer.Lower(*project.DefineDecls.at("text")->body));
	expectError("String");
	EXPECT_FALSE(lowerer.Lower(*project.DefineDecls.at("list")->body));
	expectError("List");
	const auto& binary = std::get<ast::BinaryExpr>(project.DefineDecls.at("sample")->body->node);
	EXPECT_FALSE(lowerer.Lower(*binary.left));
	expectError("lexical declaration");
}

TEST_F(SohTrackerLowering, DoesNotGuessUnresolvedEnumsIdentifiersOrHere) {
	ExpressionLowerer lowerer(project, graph, diagnostics);
	auto unresolved = ast::makeExpr(ast::Identifier{ast::Name{"unknown"}});
	project.setType(unresolved.get(), ast::Type::Int);
	EXPECT_FALSE(lowerer.Lower(*unresolved));
	expectError("resolved identifier");
	auto member = ast::makeExpr(ast::MemberExpr{ast::Name{"Mode"}, ast::Name{"First"}});
	project.setType(member.get(), ast::Type::Enum);
	EXPECT_FALSE(lowerer.Lower(*member));
	expectError("enum identity");
	auto here = ast::makeExpr(ast::HereRef{});
	project.setType(here.get(), ast::Type::Region);
	EXPECT_FALSE(lowerer.Lower(*here));
	expectError("sema-resolved region");
}

TEST_F(SohTrackerLowering, RetainsCallsInUnselectedBranchesWithoutPruning) {
	addSource(
		"extern define host() -> Bool\n"
		"region RR_TEST { locations {\n"
		"    RC_GOOD: true\n"
		"    RC_BAD: false and host()\n"
		"} }\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	const auto result = LowerRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Graph>(result));
	graph = std::get<Graph>(result);
	ASSERT_EQ(graph.roots.size(), 2);
	const auto& logical = payload<Logical>(graph.roots[1].expression);
	ASSERT_EQ(logical.children.size(), 2);
	EXPECT_TRUE(std::holds_alternative<ExternCall>(graph.nodes[logical.children[1].slot].payload));
	ASSERT_EQ(graph.adapters.size(), 1);
	EXPECT_EQ(graph.adapters[0].declaration, project.ExternDefineDecls.at("host"));
}

TEST_F(SohTrackerLowering, RejectsConditionConversionsAndCallableValuedMatches) {
	addSource(
		"enum Mode { First }\n"
		"define sample(value: Condition): value\n"
		"define matched(x: Mode, value: Condition): match x { First: value\n"
		"}\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lowerBody());
	ExpressionLowerer lowerer(project, graph, diagnostics);
	EXPECT_FALSE(lowerer.Convert(*expression, IntegralTarget::Bool,
		*project.DefineDecls.at("sample")->body));
	expectError("cannot convert a Condition");
	const auto* matched = project.DefineDecls.at("matched");
	EXPECT_FALSE(lowerer.Lower(*matched->body, matched));
	expectError("matches cannot return Condition");
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
