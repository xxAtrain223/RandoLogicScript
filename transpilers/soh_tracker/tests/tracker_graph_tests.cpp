#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string_view>

#include "parser.h"
#include "tracker_graph.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

class SohTrackerGraph : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_graph";
	const ast::RegionDecl owner{ast::Name{"RR_TEST"}, ast::RegionBody{{}, {}}};
	const ast::Entry first{ast::Name{"RC_FIRST"}, nullptr};
	const ast::Entry second{ast::Name{"RC_SECOND"}, nullptr};
	const ast::Span span{"src/graph.rls", {2, 3}, {2, 4}};
	const ValueType boolType{ast::Type::Bool, nullptr, IntegralTarget::Bool};

	Node node(Payload payload) const {
		return {std::move(payload), boolType, {span, {}}};
	}

	Root root(NodeHandle handle, const ast::Entry& entry) const {
		return {ast::SectionKind::Locations, &owner, &entry, 0, handle, {span, {}}};
	}

	void expectError(const std::vector<ast::Diagnostic>& diagnostics, std::string_view text) const {
		EXPECT_TRUE(std::any_of(diagnostics.begin(), diagnostics.end(),
			[&](const ast::Diagnostic& diagnostic) {
				return diagnostic.level == ast::DiagnosticLevel::Error &&
					diagnostic.message.find(text) != std::string::npos;
			})) << "Expected error containing: " << text;
	}
};

TEST_F(SohTrackerGraph, ChecksExactNodeAndParameterAbiBoundariesWithoutAllocatingGraphs) {
	constexpr uint64_t last = std::numeric_limits<uint32_t>::max();
	for (const uint64_t count : {uint64_t{0}, last, last + 1}) {
		const auto result = CheckNodeCount(count, span);
		ASSERT_TRUE(std::holds_alternative<NodeCount>(result));
		EXPECT_EQ(std::get<NodeCount>(result).value, count);
	}
	for (const uint64_t count : {last + 2, std::numeric_limits<uint64_t>::max()}) {
		const auto result = CheckNodeCount(count, span);
		ASSERT_TRUE(std::holds_alternative<ast::Diagnostic>(result));
		EXPECT_EQ(std::get<ast::Diagnostic>(result).span.file, span.file);
	}
	for (const uint64_t index : {uint64_t{0}, last}) {
		const auto result = CheckNodeId(index, span);
		ASSERT_TRUE(std::holds_alternative<NodeId>(result));
		EXPECT_EQ(std::get<NodeId>(result).value, index);
	}
	EXPECT_TRUE(std::holds_alternative<ast::Diagnostic>(CheckNodeId(last + 1, span)));
	EXPECT_TRUE(std::holds_alternative<ast::Diagnostic>(
		CheckNodeId(std::numeric_limits<uint64_t>::max(), span)));

	for (uint64_t position = 0; position <= 255; ++position) {
		const auto result = CheckParameterPosition(position, span);
		ASSERT_TRUE(std::holds_alternative<uint8_t>(result));
		EXPECT_EQ(std::get<uint8_t>(result), position);
	}
	for (const uint64_t position : {uint64_t{256}, std::numeric_limits<uint64_t>::max()}) {
		const auto result = CheckParameterPosition(position, span);
		ASSERT_TRUE(std::holds_alternative<ast::Diagnostic>(result));
		EXPECT_EQ(std::get<ast::Diagnostic>(result).level, ast::DiagnosticLevel::Error);
		EXPECT_EQ(std::get<ast::Diagnostic>(result).span.start.line, 2);
		EXPECT_EQ(std::get<ast::Diagnostic>(result).span.start.column, 3);
	}
}

TEST_F(SohTrackerGraph, NormalizesRelativeAndAbsolutePathsWithoutFilesystemAccess) {
	const auto relocatedRoot = sourceRoot / "another_checkout";
	const auto nested = std::filesystem::path{"src"} / "nested" / ".." / "graph.rls";
	for (const auto& path : {nested, sourceRoot / nested}) {
		const auto result = NormalizeSourcePath(ast::Span{path.string()}, sourceRoot);
		ASSERT_TRUE(std::holds_alternative<std::string>(result));
		EXPECT_EQ(std::get<std::string>(result), "src/graph.rls");
	}
	const auto relocated = NormalizeSourcePath(
		ast::Span{(relocatedRoot / nested).string()}, relocatedRoot);
	ASSERT_TRUE(std::holds_alternative<std::string>(relocated));
	EXPECT_EQ(std::get<std::string>(relocated), "src/graph.rls");

	const auto outside = NormalizeSourcePath(
		ast::Span{(std::filesystem::path{".."} / "shared.rls").string()}, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::string>(outside));
	EXPECT_EQ(std::get<std::string>(outside), "../shared.rls");
	EXPECT_TRUE(std::holds_alternative<ast::Diagnostic>(NormalizeSourcePath({}, sourceRoot)));
	EXPECT_TRUE(std::holds_alternative<ast::Diagnostic>(
		NormalizeSourcePath(ast::Span{sourceRoot.string()}, sourceRoot)));
	EXPECT_TRUE(std::holds_alternative<ast::Diagnostic>(NormalizeSourcePath(span, "relative_root")));
#ifdef _WIN32
	const auto otherDrive = sourceRoot.root_name() == "Z:" ? "Y:\\graph.rls" : "Z:\\graph.rls";
	EXPECT_TRUE(std::holds_alternative<ast::Diagnostic>(
		NormalizeSourcePath(ast::Span{otherDrive}, sourceRoot)));
#endif
}

TEST_F(SohTrackerGraph, OrdersFilesWithoutReorderingDeclarationsExtensionsOrEntries) {
	ast::Project project;
	project.files.push_back(parser::ParseString(
		"extend region RR_TEST {\n"
		"    locations {\n"
		"        RC_Z: true\n"
		"        RC_A: false\n"
		"    }\n"
		"}\n", (sourceRoot / "z.rls").string()));
	project.files.push_back(parser::ParseString(
		"define z_first():\n"
		"    true\n"
		"define a_second():\n"
		"    false\n", (sourceRoot / "a.rls").string()));
	for (const auto& file : project.files) {
		for (const auto& diagnostic : file.diagnostics) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
		}
	}
	const auto result = OrderSources(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<OrderedSource>>(result));
	const auto& ordered = std::get<std::vector<OrderedSource>>(result);
	ASSERT_EQ(ordered.size(), 2);
	EXPECT_EQ(ordered[0].path, "a.rls");
	EXPECT_EQ(ordered[1].path, "z.rls");
	EXPECT_EQ(ordered[0].file, &project.files[1]);
	EXPECT_EQ(std::get<ast::DefineDecl>(ordered[0].file->declarations[0]).name.text, "z_first");
	EXPECT_EQ(std::get<ast::DefineDecl>(ordered[0].file->declarations[1]).name.text, "a_second");
	const auto& extension = std::get<ast::ExtendRegionDecl>(ordered[1].file->declarations[0]);
	EXPECT_EQ(extension.sections[0].entries[0].name.text, "RC_Z");
	EXPECT_EQ(extension.sections[0].entries[1].name.text, "RC_A");
	EXPECT_EQ(project.files[0].path, (sourceRoot / "z.rls").string());

	std::reverse(project.files.begin(), project.files.end());
	const auto reordered = OrderSources(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<OrderedSource>>(reordered));
	const auto& reorderedFiles = std::get<std::vector<OrderedSource>>(reordered);
	EXPECT_EQ(reorderedFiles[0].path, ordered[0].path);
	EXPECT_EQ(reorderedFiles[1].path, ordered[1].path);
}

TEST_F(SohTrackerGraph, DiagnosesDuplicateNormalizedSourcePaths) {
	ast::Project project;
	project.files.push_back(ast::File{(sourceRoot / "src" / "graph.rls").string()});
	project.files.push_back(ast::File{"src/./graph.rls"});
	const auto result = OrderSources(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(result));
	expectError(std::get<std::vector<ast::Diagnostic>>(result), "duplicate normalized path");
}

TEST_F(SohTrackerGraph, RegistersOneHelperHandleAndKeepsItsBodyAcrossRepeatRegistration) {
	const ast::DefineDecl declaration{ast::Name{"helper"}, {}, nullptr};
	const ast::DefineDecl other{ast::Name{"other"}, {}, nullptr};
	Graph graph;
	const auto handle = graph.RegisterHelper(declaration);
	EXPECT_FALSE(graph.helpers.at(handle.slot).body);
	graph.RegisterHelper(other);
	EXPECT_EQ(graph.RegisterHelper(declaration), handle);
	graph.helpers.at(handle.slot).body = NodeHandle{42};
	EXPECT_EQ(graph.RegisterHelper(declaration), handle);
	ASSERT_EQ(graph.helpers.size(), 2);
	EXPECT_EQ(graph.helpers.at(handle.slot).body, NodeHandle{42});
}

TEST_F(SohTrackerGraph, Allows256CallParametersAndRejectsThe257thForBothCallKinds) {
	ast::DefineDecl helper{ast::Name{"helper"}, {}, nullptr};
	ast::ExternDefineDecl host{ast::Name{"host"}, {}};
	for (size_t i = 0; i < 256; ++i) {
		helper.params.emplace_back(ast::Name{"p" + std::to_string(i)}, std::nullopt, nullptr);
		host.params.emplace_back(ast::Name{"p" + std::to_string(i)}, std::nullopt, nullptr);
	}
	for (const bool external : {false, true}) {
		Graph graph;
		graph.helpers.push_back({&helper, NodeHandle{0}});
		graph.adapters.push_back({&host, boolType, std::vector<ValueType>(256, boolType)});
		graph.nodes.push_back(node(Value{true}));
		std::vector<NodeHandle> parameters;
		for (size_t i = 0; i < 256; ++i) {
			const auto position = CheckParameterPosition(i, span);
			ASSERT_TRUE(std::holds_alternative<uint8_t>(position));
			const auto* declaration = external ? &host.params[i] : &helper.params[i];
			parameters.push_back(NodeHandle{graph.nodes.size()});
			graph.nodes.push_back(node(Parameter{
				std::get<uint8_t>(position), declaration, NodeHandle{0}}));
		}
		const NodeHandle call{graph.nodes.size()};
		graph.nodes.push_back(node(external
			? Payload{ExternCall{AdapterHandle{0}, parameters}}
			: Payload{HelperCall{HelperHandle{0}, parameters}}));
		graph.roots.push_back(root(call, first));
		const auto valid = NumberGraph(graph, sourceRoot);
		ASSERT_TRUE(std::holds_alternative<Numbering>(valid));
		EXPECT_EQ(std::get<Numbering>(valid).count.value, 258);

		parameters.push_back(parameters.back());
		graph.nodes[call.slot].payload = external
			? Payload{ExternCall{AdapterHandle{0}, parameters}}
			: Payload{HelperCall{HelperHandle{0}, parameters}};
		const auto invalid = NumberGraph(graph, sourceRoot);
		ASSERT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(invalid));
		expectError(std::get<std::vector<ast::Diagnostic>>(invalid), "indices 0 through 255");
	}
}

TEST_F(SohTrackerGraph, NumbersInSourceAndChildOrderIndependentlyOfStorageAndCheckoutPaths) {
	Graph a;
	a.nodes = {node(Value{false}), node(Value{true}),
		node(Logical{LogicalOperator::And, {{1}, {0}, {1}}})};
	a.roots = {root(NodeHandle{2}, second), root(NodeHandle{1}, first)};
	a.roots[0].provenance.span.file = "src/b.rls";
	a.roots[1].provenance.span.file = "src/a.rls";

	const auto relocatedRoot = sourceRoot / "relocated";
	Graph b;
	b.nodes = {node(Logical{LogicalOperator::And, {{2}, {1}, {2}}}),
		node(Value{false}), node(Value{true})};
	b.roots = {root(NodeHandle{2}, first), root(NodeHandle{0}, second)};
	b.roots[0].provenance.span.file = (relocatedRoot / "src" / "a.rls").string();
	b.roots[1].provenance.span.file = (relocatedRoot / "src" / "b.rls").string();

	const auto resultA = NumberGraph(a, sourceRoot);
	const auto resultB = NumberGraph(b, relocatedRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(resultA));
	ASSERT_TRUE(std::holds_alternative<Numbering>(resultB));
	const auto& idsA = std::get<Numbering>(resultA);
	const auto& idsB = std::get<Numbering>(resultB);
	EXPECT_EQ(idsA.count, (NodeCount{3}));
	EXPECT_EQ(idsB.count, idsA.count);
	EXPECT_EQ(idsA.nodesById, (std::vector<NodeHandle>{{1}, {2}, {0}}));
	EXPECT_EQ(idsB.nodesById, (std::vector<NodeHandle>{{2}, {0}, {1}}));
	EXPECT_EQ(idsA.rootsInOrder, (std::vector<size_t>{1, 0}));
	EXPECT_EQ(idsB.rootsInOrder, (std::vector<size_t>{0, 1}));
	const auto& childrenA = std::get<Logical>(a.nodes[2].payload).children;
	const auto& childrenB = std::get<Logical>(b.nodes[0].payload).children;
	for (size_t i = 0; i < childrenA.size(); ++i) {
		EXPECT_EQ(idsA.idsBySlot[childrenA[i].slot], idsB.idsBySlot[childrenB[i].slot]);
	}
	EXPECT_EQ(childrenA, (std::vector<NodeHandle>{{1}, {0}, {1}}));
	EXPECT_EQ(a.roots[0].entry, &second);
}

TEST_F(SohTrackerGraph, UsesEntrySourceOrderWithinAFileRatherThanKeyOrder) {
	Graph graph;
	graph.nodes = {node(Value{true}), node(Value{false})};
	graph.roots = {root(NodeHandle{0}, first), root(NodeHandle{1}, second)};
	graph.roots[0].provenance.span.start.line = 20;
	graph.roots[1].provenance.span.start.line = 10;
	const auto result = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(result));
	EXPECT_EQ(std::get<Numbering>(result).nodesById, (std::vector<NodeHandle>{{1}, {0}}));
}

TEST_F(SohTrackerGraph, DiscoversMutualAndSelfReferencesAndOmitsUnreachableDefinitions) {
	const ast::DefineDecl a{ast::Name{"a"}, {}, nullptr};
	const ast::DefineDecl b{ast::Name{"b"}, {}, nullptr};
	const ast::DefineDecl unused{ast::Name{"unused"}, {}, nullptr};
	Graph graph;
	const auto aHandle = graph.RegisterHelper(a);
	const auto bHandle = graph.RegisterHelper(b);
	const auto unusedHandle = graph.RegisterHelper(unused);
	graph.nodes = {node(HelperCall{bHandle, {}}), node(HelperCall{aHandle, {}}),
		node(HelperCall{aHandle, {}}), node(Value{false})};
	graph.helpers[aHandle.slot].body = NodeHandle{0};
	graph.helpers[bHandle.slot].body = NodeHandle{1};
	graph.helpers[unusedHandle.slot].body = NodeHandle{3};
	graph.roots.push_back(root(NodeHandle{2}, first));
	const auto result = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(result));
	const auto& numbering = std::get<Numbering>(result);
	EXPECT_EQ(numbering.nodesById, (std::vector<NodeHandle>{{2}, {0}, {1}}));
	EXPECT_FALSE(numbering.idsBySlot[3]);
	EXPECT_NE(numbering.idsBySlot[1], numbering.idsBySlot[2]);

	graph.helpers[aHandle.slot].body = NodeHandle{2};
	const auto self = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(self));
	EXPECT_EQ(std::get<Numbering>(self).nodesById, (std::vector<NodeHandle>{{2}}));
}

TEST_F(SohTrackerGraph, FollowsEveryPayloadReferenceIncludingParametersAndMatchPatterns) {
	const ast::DefineDecl helper{ast::Name{"helper"}, {}, nullptr};
	ast::ExternDefineDecl host{ast::Name{"host"}, {}};
	host.params.emplace_back(ast::Name{"value"}, std::nullopt, nullptr);
	Graph graph;
	const auto handle = graph.RegisterHelper(helper);
	graph.adapters.push_back({&host, boolType, {boolType}});
	graph.nodes = {
		node(Value{true}),
		node(Parameter{0, &host.params[0], NodeHandle{0}}),
		node(Not{NodeHandle{1}}),
		node(Conversion{IntegralTarget::Bool, NodeHandle{2}}),
		node(Comparison{ComparisonOperator::Equal, NodeHandle{3}, NodeHandle{0}}),
		node(Ternary{NodeHandle{4}, NodeHandle{3}, NodeHandle{2}}),
		node(Condition{NodeHandle{5}}),
		node(Invoke{NodeHandle{6}}),
		node(MatchPatterns{{{0}, {3}}}),
		node(MatchArm{NodeHandle{8}, NodeHandle{7}, true}),
		node(Match{IntegralTarget::Bool, NodeHandle{0}, {{9}}}),
		node(ExternCall{AdapterHandle{0}, {{1}}}),
		node(HelperCall{handle, {}}),
		node(Logical{LogicalOperator::Or, {{11}, {12}}}),
		node(Arithmetic{ArithmeticOperator::Add, {{13}, {0}}})
	};
	graph.helpers[handle.slot].body = NodeHandle{10};
	graph.roots.push_back(root(NodeHandle{14}, first));
	const auto result = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(result));
	const auto& numbering = std::get<Numbering>(result);
	EXPECT_EQ(numbering.count.value, graph.nodes.size());
	for (const auto& id : numbering.idsBySlot) {
		EXPECT_TRUE(id.has_value());
	}
	EXPECT_EQ(numbering.nodesById,
		(std::vector<NodeHandle>{{14}, {13}, {11}, {1}, {0}, {12}, {10},
			{9}, {8}, {3}, {2}, {7}, {6}, {5}, {4}}));
}

TEST_F(SohTrackerGraph, RejectsDanglingEdgesEvenInUnreachableNodes) {
	const NodeHandle missing{99};
	const ast::DefineDecl helper{ast::Name{"helper"}, {}, nullptr};
	const std::vector<Payload> broken = {
		Logical{LogicalOperator::And, {{0}, missing}},
		Arithmetic{ArithmeticOperator::Add, {missing, {0}}},
		Not{missing}, Conversion{IntegralTarget::Bool, missing},
		Comparison{ComparisonOperator::Equal, {0}, missing},
		Ternary{{0}, {0}, missing}, Condition{missing}, Invoke{missing},
		Match{IntegralTarget::Bool, {0}, {missing}},
		Match{IntegralTarget::Bool, missing, {{0}}},
		MatchArm{{0}, missing, false}, MatchArm{missing, {0}, false},
		MatchPatterns{{missing}},
		HelperCall{HelperHandle{0}, {missing}}
	};
	for (const auto& payload : broken) {
		Graph graph;
		graph.nodes = {node(Value{true}), node(payload)};
		graph.helpers.push_back({&helper, NodeHandle{0}});
		graph.roots.push_back(root(NodeHandle{0}, first));
		const auto result = NumberGraph(graph, sourceRoot);
		ASSERT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(result));
		const auto& diagnostics = std::get<std::vector<ast::Diagnostic>>(result);
		expectError(diagnostics, "invalid node handle");
		EXPECT_EQ(diagnostics.front().span.file, span.file);
	}
}

TEST_F(SohTrackerGraph, ReportsIncompleteDuplicateAndDanglingMetadata) {
	const ast::DefineDecl helper{ast::Name{"helper"}, {}, nullptr, span};
	Graph graph;
	graph.helpers = {{&helper, std::nullopt}, {&helper, NodeHandle{99}}, {nullptr, std::nullopt}};
	graph.adapters.push_back({nullptr, boolType, {}});
	graph.nodes = {
		node(Value{true}), node(HelperCall{HelperHandle{99}, {}}),
		node(ExternCall{AdapterHandle{99}, {}}),
		node(Parameter{0, nullptr, NodeHandle{0}}),
		node(Logical{LogicalOperator::And, {{0}}}),
		node(Arithmetic{ArithmeticOperator::Divide, {}})
	};
	graph.roots = {root(NodeHandle{0}, first), root(NodeHandle{0}, first)};
	const auto result = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(result));
	const auto& diagnostics = std::get<std::vector<ast::Diagnostic>>(result);
	for (const auto text : {"incomplete", "more than one registered", "missing its declaration",
		"invalid helper handle", "invalid host adapter handle", "invalid node handle",
		"at least two children", "duplicate root identity"}) {
		expectError(diagnostics, text);
	}
}

TEST_F(SohTrackerGraph, RejectsInvalidRootReferencesAndMissingSourceIdentity) {
	Graph graph;
	graph.nodes = {node(Value{true})};
	graph.roots.push_back(root(NodeHandle{99}, first));
	graph.roots[0].owner = nullptr;
	graph.roots[0].entry = nullptr;
	graph.roots[0].provenance.span.file.clear();
	const auto result = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(result));
	const auto& diagnostics = std::get<std::vector<ast::Diagnostic>>(result);
	expectError(diagnostics, "invalid node handle");
	expectError(diagnostics, "missing its owner or entry");
	expectError(diagnostics, "requires a source file path");
}

TEST_F(SohTrackerGraph, NumbersDeepGraphsIterativelyAndHandlesEmptyGraphs) {
	EXPECT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(
		NumberGraph(Graph{}, "relative_root")));
	EXPECT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(
		OrderSources(ast::Project{}, "relative_root")));
	const auto empty = NumberGraph(Graph{}, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(empty));
	EXPECT_EQ(std::get<Numbering>(empty).count, (NodeCount{0}));
	EXPECT_TRUE(std::get<Numbering>(empty).nodesById.empty());
	EXPECT_TRUE(std::get<Numbering>(empty).idsBySlot.empty());
	EXPECT_TRUE(std::get<Numbering>(empty).rootsInOrder.empty());

	Graph graph;
	graph.nodes.push_back(node(Value{true}));
	for (size_t i = 1; i < 20'000; ++i) {
		graph.nodes.push_back(node(Not{NodeHandle{i - 1}}));
	}
	graph.roots.push_back(root(NodeHandle{graph.nodes.size() - 1}, first));
	const auto result = NumberGraph(graph, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<Numbering>(result));
	const auto& numbering = std::get<Numbering>(result);
	EXPECT_EQ(numbering.count.value, graph.nodes.size());
	EXPECT_EQ(numbering.idsBySlot.back(), (NodeId{0}));
	EXPECT_EQ(numbering.idsBySlot.front(), (NodeId{19'999}));
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
