#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "parser.h"
#include "sema.h"
#include "tracker_lowering.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

class SohTrackerCalls : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_calls";
	ast::Project project;
	Graph graph;
	std::vector<ast::Diagnostic> diagnostics;

	void parse(const std::string& source) {
		project.files.push_back(parser::ParseString(source, (sourceRoot / "calls.rls").string()));
		for (const auto& diagnostic : project.files.back().diagnostics) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
		}
	}

	void analyze() {
		for (const auto& diagnostic : sema::analyze(project)) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
		}
	}

	bool lower() {
		auto result = LowerRoots(project, sourceRoot);
		if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result)) {
			diagnostics = *errors;
			return false;
		}
		graph = std::get<Graph>(std::move(result));
		return true;
	}

	template <typename T>
	const T& payload(NodeHandle handle) const {
		return std::get<T>(graph.nodes.at(handle.slot).payload);
	}

	const ast::Expr& rootCondition(size_t index = 0) const {
		return *project.RegionDecls.at("RR_TEST")->body.sections[0].entries[index].condition;
	}

	void expectError(std::string_view text) const {
		EXPECT_TRUE(std::any_of(diagnostics.begin(), diagnostics.end(),
			[&](const ast::Diagnostic& diagnostic) {
				return diagnostic.level == ast::DiagnosticLevel::Error &&
					diagnostic.message.find(text) != std::string::npos;
			})) << text;
	}

	void expectFullyReachable() const {
		const auto numbering = NumberGraph(graph, sourceRoot);
		ASSERT_TRUE(std::holds_alternative<Numbering>(numbering));
		EXPECT_EQ(std::get<Numbering>(numbering).count.value, graph.nodes.size());
	}
};

TEST_F(SohTrackerCalls, UsesSemaOrderAndDefaultsWithDistinctCallAndArgumentOccurrences) {
	parse(
		"define choose(first: Bool, second: Bool = false, third: Bool = true):\n"
		"    first and (second or third)\n"
		"define unused(): \"not executable\"\n"
		"extern define unused_host(value: List) -> Bool\n"
		"region RR_TEST { locations {\n"
		"    RC_TEST: choose(third: false, first: true) and choose(true)\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	ASSERT_EQ(graph.helpers.size(), 1);
	EXPECT_EQ(graph.helpers[0].declaration, project.DefineDecls.at("choose"));
	const auto& logical = payload<Logical>(graph.roots[0].expression);
	ASSERT_EQ(logical.children.size(), 2);
	EXPECT_NE(logical.children[0], logical.children[1]);
	const auto& first = payload<HelperCall>(logical.children[0]);
	const auto& second = payload<HelperCall>(logical.children[1]);
	EXPECT_EQ(first.definition, second.definition);
	ASSERT_EQ(first.parameters.size(), 3);
	ASSERT_EQ(second.parameters.size(), 3);
	const auto& source = std::get<ast::BinaryExpr>(rootCondition().node);
	const auto& firstSource = std::get<ast::CallExpr>(source.left->node);
	const auto* resolved = project.getResolvedCallArgs(&firstSource);
	ASSERT_NE(resolved, nullptr);
	for (size_t i = 0; i < 3; ++i) {
		const auto& wrapper = payload<Parameter>(first.parameters[i]);
		EXPECT_EQ(wrapper.position, i);
		EXPECT_EQ(wrapper.declaration, &project.DefineDecls.at("choose")->params[i]);
		EXPECT_NE(first.parameters[i], second.parameters[i]);
		EXPECT_NE(wrapper.value, payload<Parameter>(second.parameters[i]).value);
		EXPECT_EQ(graph.nodes[first.parameters[i].slot].provenance.expressions[0], (*resolved)[i]);
	}
	EXPECT_TRUE(std::get<bool>(payload<Value>(payload<Parameter>(first.parameters[0]).value).value));
	EXPECT_FALSE(std::get<bool>(payload<Value>(payload<Parameter>(first.parameters[1]).value).value));
	EXPECT_FALSE(std::get<bool>(payload<Value>(payload<Parameter>(first.parameters[2]).value).value));
	EXPECT_TRUE(std::get<bool>(payload<Value>(payload<Parameter>(second.parameters[2]).value).value));
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, KeepsArgumentsInCallerScopeAndBodiesInDeclarationScope) {
	parse(
		"define inner(value: Bool): value\n"
		"define outer(value: Bool): inner(value)\n"
		"region RR_TEST { locations { RC_TEST: outer(true) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	ASSERT_EQ(graph.helpers.size(), 2);
	const auto& outer = payload<HelperCall>(graph.roots[0].expression);
	const auto& inner = payload<HelperCall>(*graph.helpers[outer.definition.slot].body);
	const auto& argument = payload<Parameter>(inner.parameters[0]);
	EXPECT_EQ(argument.declaration, &project.DefineDecls.at("inner")->params[0]);
	EXPECT_EQ(std::get<ParameterRead>(payload<Value>(argument.value).value).declaration,
		&project.DefineDecls.at("outer")->params[0]);
	EXPECT_EQ(std::get<ParameterRead>(
		payload<Value>(*graph.helpers[inner.definition.slot].body).value).declaration,
		&project.DefineDecls.at("inner")->params[0]);
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, BuildsAllSevenSpiritSharedWrappersAndCheckPriceDefault) {
	parse(
		"extern enum Region { RR_* }\n"
		"extern enum Item { RG_* }\n"
		"extern enum Location { RC_* }\n"
		"extern define can_use(item: Item) -> Bool\n"
		"extern define spirit_shared(first_region: Region, first_condition: Condition,\n"
		"    any_age: Bool = false, second_region: Region = RR_NONE,\n"
		"    second_condition: Condition = false, third_region: Region = RR_NONE,\n"
		"    third_condition: Condition = false) -> Bool\n"
		"extern define check_price(chk: Location = RC_UNKNOWN_CHECK) -> Int\n"
		"region RR_TEST { locations {\n"
		"    RC_TEST: spirit_shared(here, can_use(RG_HOOKSHOT), any_age: true)\n"
		"    RC_PRICE: check_price() > 0\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	const auto& call = payload<ExternCall>(graph.roots[0].expression);
	const auto* declaration = project.ExternDefineDecls.at("spirit_shared");
	EXPECT_EQ(graph.adapters[call.adapter.slot].declaration, declaration);
	ASSERT_EQ(call.parameters.size(), 7);
	for (size_t i = 0; i < 7; ++i) {
		const auto& parameter = payload<Parameter>(call.parameters[i]);
		EXPECT_EQ(parameter.position, i);
		EXPECT_EQ(parameter.declaration, &declaration->params[i]);
	}
	const auto value = [&](size_t position) {
		return payload<Parameter>(call.parameters[position]).value;
	};
	EXPECT_EQ(std::get<HereValue>(payload<Value>(value(0)).value).region,
		project.RegionDecls.at("RR_TEST"));
	const auto conditionBody = payload<Condition>(value(1)).body;
	EXPECT_TRUE(std::holds_alternative<ExternCall>(graph.nodes[conditionBody.slot].payload));
	EXPECT_TRUE(std::get<bool>(payload<Value>(value(2)).value));
	for (const size_t i : {size_t{3}, size_t{5}}) {
		EXPECT_EQ(std::get<EnumConstant>(payload<Value>(value(i)).value).member->text, "RR_NONE");
	}
	for (const size_t i : {size_t{4}, size_t{6}}) {
		const auto body = payload<Condition>(value(i)).body;
		EXPECT_FALSE(std::get<bool>(payload<Value>(body).value));
		EXPECT_TRUE(std::holds_alternative<ConditionTarget>(graph.nodes[value(i).slot].valueType->representation));
	}
	const auto& price = payload<ExternCall>(payload<Comparison>(graph.roots[1].expression).left);
	ASSERT_EQ(price.parameters.size(), 1);
	EXPECT_EQ(std::get<EnumConstant>(
		payload<Value>(payload<Parameter>(price.parameters[0]).value).value).member->text,
		"RC_UNKNOWN_CHECK");
	EXPECT_EQ(graph.adapters[price.adapter.slot].result.sourceType, ast::Type::Int);
	EXPECT_EQ(std::get<IntegralTarget>(graph.adapters[price.adapter.slot].result.representation),
		IntegralTarget::Int32);
	EXPECT_EQ(graph.adapters.size(), 3);
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, WrapsBooleanCompatibleConditionsButForwardsExistingConditions) {
	parse(
		"extern define any_age(condition: Condition) -> Bool\n"
		"define relay(condition: Condition): any_age(condition)\n"
		"region RR_TEST { locations {\n"
		"    RC_INT: relay(42)\n"
		"    RC_BOOL: relay(false)\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	ASSERT_EQ(graph.helpers.size(), 1);
	const auto& first = payload<HelperCall>(graph.roots[0].expression);
	const auto condition = payload<Parameter>(first.parameters[0]).value;
	const auto normalized = payload<Condition>(condition).body;
	EXPECT_EQ(payload<Conversion>(normalized).target, IntegralTarget::Bool);
	EXPECT_EQ(std::get<int32_t>(payload<Value>(payload<Conversion>(normalized).operand).value), 42);
	const auto& host = payload<ExternCall>(*graph.helpers[0].body);
	const auto forwarded = payload<Parameter>(host.parameters[0]).value;
	EXPECT_EQ(std::get<ParameterRead>(payload<Value>(forwarded).value).declaration,
		&project.DefineDecls.at("relay")->params[0]);
	EXPECT_TRUE(std::holds_alternative<ConditionTarget>(graph.nodes[forwarded.slot].valueType->representation));
	EXPECT_EQ(std::count_if(graph.nodes.begin(), graph.nodes.end(),
		[](const Node& node) { return std::holds_alternative<Condition>(node.payload); }), 2);
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, LowersEightShopRootsAndFiveBossRootsWithDistinctSetterAndReaders) {
	std::string source =
		"extern enum Enemy { RE_* }\n"
		"extern define can_kill(enemy: Enemy) -> Bool\n"
		"extern define flag(event: Event) -> Bool\n"
		"region RR_DEKU_TREE_BOSS_EXIT {}\n"
		"region RR_KF_OUTSIDE_DEKU_TREE {}\n"
		"region RR_DEKU_TREE_BOSS_ROOM {\n"
		"    events { LOGIC_DEKU_TREE_CLEAR: can_kill(RE_GOHMA) }\n"
		"    locations {\n"
		"        RC_QUEEN_GOHMA: flag(LOGIC_DEKU_TREE_CLEAR)\n"
		"        RC_DEKU_TREE_QUEEN_GOHMA_HEART: flag(LOGIC_DEKU_TREE_CLEAR)\n"
		"    }\n"
		"    exits {\n"
		"        RR_DEKU_TREE_BOSS_EXIT: always\n"
		"        RR_KF_OUTSIDE_DEKU_TREE: flag(LOGIC_DEKU_TREE_CLEAR)\n"
		"    }\n"
		"}\n"
		"region RR_KF_KOKIRI_SHOP { locations {\n";
	for (int i = 1; i <= 8; ++i) {
		source += "    RC_KF_SHOP_ITEM_" + std::to_string(i) +
			(i % 2 == 0 ? ": never\n" : ": always\n");
	}
	source += "} }\n";
	parse(source);
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	ASSERT_EQ(graph.roots.size(), 13);
	EXPECT_EQ(graph.roots[0].kind, ast::SectionKind::Events);
	const auto& setter = payload<ExternCall>(graph.roots[0].expression);
	EXPECT_EQ(graph.adapters[setter.adapter.slot].declaration,
		project.ExternDefineDecls.at("can_kill"));
	for (const size_t i : {size_t{1}, size_t{2}, size_t{4}}) {
		const auto& reader = payload<ExternCall>(graph.roots[i].expression);
		EXPECT_EQ(graph.adapters[reader.adapter.slot].declaration,
			project.ExternDefineDecls.at("flag"));
		const auto argument = payload<Parameter>(reader.parameters[0]).value;
		EXPECT_EQ(std::get<KeyConstant>(payload<Value>(argument).value).name->text,
			"LOGIC_DEKU_TREE_CLEAR");
	}
	EXPECT_EQ(graph.roots[3].entry->name.text, "RR_DEKU_TREE_BOSS_EXIT");
	EXPECT_EQ(graph.roots[4].entry->name.text, "RR_KF_OUTSIDE_DEKU_TREE");
	for (size_t i = 5; i < graph.roots.size(); ++i) {
		EXPECT_EQ(graph.roots[i].owner, project.RegionDecls.at("RR_KF_KOKIRI_SHOP"));
		EXPECT_EQ(graph.roots[i].entry->name.text, "RC_KF_SHOP_ITEM_" + std::to_string(i - 4));
	}
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, KeepsDefaultHostCallsOccurrenceSpecificWhileSharingAdapters) {
	parse(
		"extern define live() -> Bool\n"
		"define sample(value: Bool = live()): value\n"
		"region RR_TEST { locations { RC_TEST: sample() and sample() } }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	const auto& logical = payload<Logical>(graph.roots[0].expression);
	const auto& first = payload<HelperCall>(logical.children[0]);
	const auto& second = payload<HelperCall>(logical.children[1]);
	const auto firstValue = payload<Parameter>(first.parameters[0]).value;
	const auto secondValue = payload<Parameter>(second.parameters[0]).value;
	EXPECT_NE(firstValue, secondValue);
	EXPECT_EQ(payload<ExternCall>(firstValue).adapter, payload<ExternCall>(secondValue).adapter);
	EXPECT_EQ(graph.adapters.size(), 1);
	EXPECT_EQ(graph.helpers.size(), 1);
	EXPECT_EQ(graph.nodes[firstValue.slot].provenance.expressions,
		graph.nodes[secondValue.slot].provenance.expressions);
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, ForwardsReturnedConditionsAndRetainsLexicalReadsInsideLazyBodies) {
	parse(
		"extern enum Item { RG_* }\n"
		"extern define has(item: Item) -> Bool\n"
		"extern define any_age(condition: Condition) -> Bool\n"
		"define identity(value: Condition): value\n"
		"define delayed(item: Item): identity(has(item))\n"
		"region RR_TEST { locations { RC_TEST: any_age(delayed(RG_HOOKSHOT)) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	const auto& host = payload<ExternCall>(graph.roots[0].expression);
	const auto forwarded = payload<Parameter>(host.parameters[0]).value;
	const auto& delayed = payload<HelperCall>(forwarded);
	EXPECT_TRUE(std::holds_alternative<ConditionTarget>(graph.nodes[forwarded.slot].valueType->representation));
	const auto& identity = payload<HelperCall>(*graph.helpers[delayed.definition.slot].body);
	const auto& lazy = payload<Condition>(payload<Parameter>(identity.parameters[0]).value);
	const auto& query = payload<ExternCall>(lazy.body);
	const auto lexicalRead = payload<Parameter>(query.parameters[0]).value;
	EXPECT_EQ(std::get<ParameterRead>(payload<Value>(lexicalRead).value).declaration,
		&project.DefineDecls.at("delayed")->params[0]);
	const auto identityResult = *graph.helpers[identity.definition.slot].body;
	EXPECT_EQ(std::get<ParameterRead>(payload<Value>(identityResult).value).declaration,
		&project.DefineDecls.at("identity")->params[0]);
	EXPECT_EQ(std::count_if(graph.nodes.begin(), graph.nodes.end(),
		[](const Node& node) { return std::holds_alternative<Condition>(node.payload); }), 1);
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, RetainsCallBoundariesInLogicalAndDynamicArithmeticChains) {
	parse(
		"extern define query() -> Int\n"
		"define number(): query() / 2\n"
		"define available(): true and false\n"
		"region RR_TEST { locations {\n"
		"    RC_NUMBER: number() / query() / query() > 0\n"
		"    RC_BOOL: true and available() and false\n"
		"} }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	const auto& arithmetic = payload<Arithmetic>(payload<Comparison>(graph.roots[0].expression).left);
	ASSERT_EQ(arithmetic.children.size(), 3);
	EXPECT_TRUE(std::holds_alternative<HelperCall>(graph.nodes[arithmetic.children[0].slot].payload));
	EXPECT_TRUE(std::holds_alternative<ExternCall>(graph.nodes[arithmetic.children[1].slot].payload));
	EXPECT_TRUE(std::holds_alternative<ExternCall>(graph.nodes[arithmetic.children[2].slot].payload));
	EXPECT_NE(arithmetic.children[1], arithmetic.children[2]);
	const auto& logical = payload<Logical>(graph.roots[1].expression);
	ASSERT_EQ(logical.children.size(), 3);
	const auto& call = payload<HelperCall>(logical.children[1]);
	EXPECT_TRUE(std::holds_alternative<Logical>(
		graph.nodes[graph.helpers[call.definition.slot].body->slot].payload));
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, KeepsDefaultCallResolutionIndependentOfCallerParameterNames) {
	parse(
		"extern define live() -> Bool\n"
		"define target(value: Bool = live()): value\n"
		"define caller(live: Condition): target()\n"
		"region RR_TEST { locations { RC_TEST: caller(false) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	ASSERT_TRUE(lower());
	const auto& caller = payload<HelperCall>(graph.roots[0].expression);
	const auto& target = payload<HelperCall>(*graph.helpers[caller.definition.slot].body);
	const auto& host = payload<ExternCall>(payload<Parameter>(target.parameters[0]).value);
	EXPECT_EQ(graph.adapters[host.adapter.slot].declaration, project.ExternDefineDecls.at("live"));
	expectFullyReachable();
}

TEST_F(SohTrackerCalls, RegistersSyntheticSelfAndMutualDefinitionsBeforeTheirBodies) {
	for (const bool mutual : {false, true}) {
		SCOPED_TRACE(mutual);
		project = {};
		graph = {};
		parse(std::string("define first(): ") + (mutual ? "second()\n" : "first()\n") +
			(mutual ? "define second(): first()\n" : "") +
			"region RR_TEST { locations { RC_TEST: first() } }\n");
		ASSERT_FALSE(HasFatalFailure());
		// Sema still rejects recursion. Supply established facts only for this architecture fixture.
		for (const auto& declaration : project.files[0].declarations) {
			if (const auto* helper = std::get_if<ast::DefineDecl>(&declaration)) {
				project.DefineDecls.emplace(helper->name.text, helper);
				project.setType(helper->body.get(), ast::Type::Bool);
				project.setResolvedCallArgs(&std::get<ast::CallExpr>(helper->body->node), {});
			} else if (const auto* region = std::get_if<ast::RegionDecl>(&declaration)) {
				project.RegionDecls.emplace(region->key.text, region);
			}
		}
		project.setType(&rootCondition(), ast::Type::Bool);
		project.setResolvedCallArgs(&std::get<ast::CallExpr>(rootCondition().node), {});
		ASSERT_TRUE(lower());
		EXPECT_EQ(graph.helpers.size(), mutual ? 2 : 1);
		EXPECT_EQ(graph.nodes.size(), mutual ? 3 : 2);
		for (const auto& helper : graph.helpers) {
			ASSERT_TRUE(helper.body);
			const auto& call = payload<HelperCall>(*helper.body);
			EXPECT_LT(call.definition.slot, graph.helpers.size());
		}
		expectFullyReachable();
		const auto firstNumbering = NumberGraph(graph, sourceRoot);
		const auto secondNumbering = NumberGraph(graph, sourceRoot);
		EXPECT_EQ(std::get<Numbering>(firstNumbering).nodesById,
			std::get<Numbering>(secondNumbering).nodesById);
	}
}

TEST_F(SohTrackerCalls, ChecksTheExactParameterPositionLimitDuringLowering) {
	for (const bool external : {false, true}) {
		for (const size_t count : {size_t{256}, size_t{257}}) {
			SCOPED_TRACE(count);
			SCOPED_TRACE(external);
			project = {};
			graph = {};
			std::string source = external ? "extern define many(" : "define many(";
			for (size_t i = 0; i < count; ++i) {
				if (i) source += ", ";
				source += "p" + std::to_string(i) + ": Bool = true";
			}
			source += external ? ") -> Bool\n" : "): true\n";
			source += "region RR_TEST { locations { RC_TEST: many() } }\n";
			parse(source);
			ASSERT_FALSE(HasFatalFailure());
			analyze();
			ASSERT_FALSE(HasFatalFailure());
			if (count == 256) {
				ASSERT_TRUE(lower());
				const auto root = graph.roots[0].expression;
				const auto& parameters = external ? payload<ExternCall>(root).parameters :
					payload<HelperCall>(root).parameters;
				ASSERT_EQ(parameters.size(), 256);
				EXPECT_EQ(payload<Parameter>(parameters.back()).position, 255);
				expectFullyReachable();
			} else {
				EXPECT_FALSE(lower());
				expectError("uint8_t ABI");
			}
		}
	}
}

TEST_F(SohTrackerCalls, DiagnosesIncompleteBindingsInSkippedBranchesWithoutPartialSuccess) {
	parse(
		"extern define host(value: Bool = false) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: false and host() } }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	const auto& binary = std::get<ast::BinaryExpr>(rootCondition().node);
	const auto* call = &std::get<ast::CallExpr>(binary.right->node);
	project.setResolvedCallArgs(call, {});
	EXPECT_FALSE(lower());
	expectError("complete sema-resolved arguments");
	EXPECT_EQ(diagnostics[0].span.file, (sourceRoot / "calls.rls").string());
	project.setResolvedCallArgs(call, {nullptr});
	EXPECT_FALSE(lower());
	expectError("missing sema-resolved argument");
}

TEST_F(SohTrackerCalls, NeverRebindsArgumentsOrGuessesMissingDeclarationsAndTypes) {
	ast::ExternDefineDecl host{ast::Name{"host"}, {}};
	project.ExternDefineDecls.emplace("host", &host);
	auto call = ast::makeExpr(ast::CallExpr{ast::Name{"host"}, {}});
	project.setType(call.get(), ast::Type::Bool);
	ExpressionLowerer lowerer(project, graph, diagnostics);
	EXPECT_FALSE(lowerer.Lower(*call));
	expectError("complete sema-resolved arguments");
	project.setResolvedCallArgs(&std::get<ast::CallExpr>(call->node), {});
	project.ExternDefineDecls.clear();
	EXPECT_FALSE(lowerer.Lower(*call));
	expectError("analyzed declaration");

	host.params.emplace_back(ast::Name{"value"}, std::nullopt, nullptr);
	auto value = ast::makeExpr(ast::BoolLiteral{true});
	project.setType(value.get(), ast::Type::Bool);
	project.ExternDefineDecls.emplace("host", &host);
	project.setResolvedCallArgs(&std::get<ast::CallExpr>(call->node), {value.get()});
	EXPECT_FALSE(lowerer.Lower(*call));
	expectError("analyzed value type");
}

TEST_F(SohTrackerCalls, RejectsUnsupportedReachableSignatures) {
	parse(
		"extern define unsupported(value: String) -> Bool\n"
		"region RR_TEST { locations { RC_TEST: unsupported(\"value\") } }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	EXPECT_FALSE(lower());
	expectError("String");
}

TEST_F(SohTrackerCalls, DoesNotMistakeCallableParameterInvocationForAGlobalHelper) {
	parse(
		"define condition(): true\n"
		"define sample(condition: Condition): condition()\n"
		"region RR_TEST { locations { RC_TEST: sample(false) } }\n");
	ASSERT_FALSE(HasFatalFailure());
	analyze();
	ASSERT_FALSE(HasFatalFailure());
	EXPECT_FALSE(lower());
	expectError("callable-parameter invocation lowering is not implemented");
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
