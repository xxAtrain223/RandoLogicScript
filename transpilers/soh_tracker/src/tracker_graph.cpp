#include "tracker_graph.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <set>
#include <span>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace rls::transpilers::soh_tracker::ir {

namespace {

ast::Diagnostic error(const ast::Span& span, std::string message) {
	return {"", span, ast::DiagnosticLevel::Error, std::move(message)};
}

// Dependency order defines numbering order, not eager runtime evaluation.
// Keep repeated handles: separate child positions still need separate traces.
std::vector<NodeHandle> references(const Graph& graph, const Node& node,
	std::vector<ast::Diagnostic>& diagnostics)
{
	return std::visit([&](const auto& payload) -> std::vector<NodeHandle> {
		using T = std::decay_t<decltype(payload)>;
		if constexpr (std::is_same_v<T, HelperCall> || std::is_same_v<T, ExternCall>) {
			if (!payload.parameters.empty()) {
				const auto position = CheckParameterPosition(
					payload.parameters.size() - 1, node.provenance.span);
				if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&position)) {
					diagnostics.push_back(*diagnostic);
				}
			}
		}
		if constexpr (std::is_same_v<T, Value>) {
			return {};
		} else if constexpr (std::is_same_v<T, Logical> || std::is_same_v<T, Arithmetic>) {
			if (payload.children.size() < 2) {
				diagnostics.push_back(error(node.provenance.span,
					"Tracker logical/arithmetic nodes require at least two children."));
			}
			return payload.children;
		} else if constexpr (std::is_same_v<T, Not> || std::is_same_v<T, Conversion>) {
			return {payload.operand};
		} else if constexpr (std::is_same_v<T, Comparison>) {
			return {payload.left, payload.right};
		} else if constexpr (std::is_same_v<T, Ternary>) {
			return {payload.condition, payload.trueBranch, payload.falseBranch};
		} else if constexpr (std::is_same_v<T, HelperCall>) {
			// Discover arguments before the shared body; do not inline the definition.
			auto children = payload.parameters;
			if (payload.definition.slot >= graph.helpers.size()) {
				diagnostics.push_back(error(node.provenance.span,
					"Tracker call references an invalid helper handle."));
			} else if (const auto body = graph.helpers[payload.definition.slot].body) {
				children.push_back(*body);
			}
			return children;
		} else if constexpr (std::is_same_v<T, ExternCall>) {
			if (payload.adapter.slot >= graph.adapters.size()) {
				diagnostics.push_back(error(node.provenance.span,
					"Tracker call references an invalid host adapter handle."));
			}
			return payload.parameters;
		} else if constexpr (std::is_same_v<T, Parameter>) {
			if (!payload.declaration) {
				diagnostics.push_back(error(node.provenance.span,
					"Tracker parameter is missing its declaration."));
			}
			return {payload.value};
		} else if constexpr (std::is_same_v<T, Condition>) {
			return {payload.body};
		} else if constexpr (std::is_same_v<T, Invoke>) {
			return {payload.callable};
		} else if constexpr (std::is_same_v<T, Match>) {
			auto children = payload.arms;
			children.insert(children.begin(), payload.discriminant);
			return children;
		} else if constexpr (std::is_same_v<T, MatchArm>) {
			return {payload.patterns, payload.body};
		} else if constexpr (std::is_same_v<T, MatchPatterns>) {
			return payload.patterns;
		} else {
			static_assert(std::is_same_v<T, void>, "Unhandled tracker payload");
		}
	}, node.payload);
}

struct NodeKey {
	size_t kind;
	std::vector<uint64_t> values;
	std::vector<const void*> declarations;
	std::vector<std::string> names;
	std::vector<size_t> children;
	bool operator==(const NodeKey&) const = default;
};

struct NodeKeyHash {
	size_t operator()(const NodeKey& key) const {
		size_t hash = key.kind;
		const auto add = [&](const auto& value) {
			hash ^= std::hash<std::decay_t<decltype(value)>>{}(value) +
				size_t{0x9e3779b9} + (hash << 6) + (hash >> 2);
		};
		for (const auto value : key.values) add(value);
		for (const auto declaration : key.declarations) add(declaration);
		for (const auto& name : key.names) add(name);
		for (const auto child : key.children) add(child);
		return hash;
	}
};

NodeKey nodeKey(const Node& node, std::vector<size_t> children) {
	NodeKey key{node.payload.index(), {}, {}, {}, std::move(children)};
	key.values.push_back(node.valueType.has_value());
	if (node.valueType) {
		key.values.push_back(static_cast<uint64_t>(node.valueType->sourceType));
		key.values.push_back(node.valueType->representation.index());
		key.declarations.push_back(node.valueType->enumInfo);
		if (const auto* target = std::get_if<IntegralTarget>(&node.valueType->representation)) {
			key.values.push_back(static_cast<uint64_t>(*target));
		}
	}
	std::visit([&](const auto& payload) {
		using T = std::decay_t<decltype(payload)>;
		if constexpr (std::is_same_v<T, Value>) {
			key.values.push_back(payload.value.index());
			std::visit([&](const auto& value) {
				using V = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<V, bool> || std::is_same_v<V, int32_t>) {
					key.values.push_back(static_cast<uint64_t>(value));
				} else if constexpr (std::is_same_v<V, EnumConstant>) {
					key.declarations.push_back(value.declaration);
					key.names.push_back(value.member->text);
				} else if constexpr (std::is_same_v<V, KeyConstant>) {
					key.names.push_back(value.name->text);
				} else if constexpr (std::is_same_v<V, ParameterRead>) {
					// Evaluation resolves the current scope by name, not declaration address.
					key.names.push_back(value.declaration->name.text);
				} else if constexpr (std::is_same_v<V, HereValue>) {
					key.declarations.push_back(value.region);
				} else {
					static_assert(std::is_same_v<V, void>, "Unhandled tracker value");
				}
			}, payload.value);
		} else if constexpr (std::is_same_v<T, Logical> || std::is_same_v<T, Arithmetic> ||
			std::is_same_v<T, Comparison>) {
			key.values.push_back(static_cast<uint64_t>(payload.operation));
		} else if constexpr (std::is_same_v<T, HelperCall>) {
			key.values.push_back(payload.definition.slot);
		} else if constexpr (std::is_same_v<T, ExternCall>) {
			key.values.push_back(payload.adapter.slot);
		} else if constexpr (std::is_same_v<T, Parameter>) {
			key.values.push_back(payload.position);
			key.names.push_back(payload.declaration->name.text);
		} else if constexpr (std::is_same_v<T, Conversion> || std::is_same_v<T, Match>) {
			key.values.push_back(static_cast<uint64_t>(payload.target));
		} else if constexpr (std::is_same_v<T, MatchArm>) {
			key.values.push_back(payload.fallThrough);
		} else {
			static_assert(std::is_same_v<T, Not> || std::is_same_v<T, Ternary> ||
				std::is_same_v<T, Condition> || std::is_same_v<T, Invoke> ||
				std::is_same_v<T, MatchPatterns>, "Unhandled tracker payload");
		}
	}, node.payload);
	return key;
}

} // namespace

HelperHandle Graph::RegisterHelper(const ast::DefineDecl& declaration) {
	// Pointer identity deduplicates declarations but never determines emitted IDs.
	const auto existing = std::find_if(helpers.begin(), helpers.end(),
		[&](const HelperDefinition& helper) { return helper.declaration == &declaration; });
	if (existing != helpers.end()) {
		return {static_cast<size_t>(existing - helpers.begin())};
	}
	const HelperHandle handle{helpers.size()};
	// Publish the handle before lowering the body so recursive calls can reuse it.
	helpers.push_back({&declaration, std::nullopt});
	return handle;
}

std::variant<NodeCount, ast::Diagnostic> CheckNodeCount(uint64_t count, const ast::Span& span) {
	constexpr uint64_t maximum = uint64_t{std::numeric_limits<uint32_t>::max()} + 1;
	if (count > maximum) {
		return error(span, "Tracker descriptor count exceeds the uint32_t index ABI (4294967296 nodes).");
	}
	return NodeCount{count};
}

std::variant<NodeId, ast::Diagnostic> CheckNodeId(uint64_t index, const ast::Span& span) {
	if (index > std::numeric_limits<uint32_t>::max()) {
		return error(span, "Tracker node index exceeds the uint32_t index ABI (4294967295).");
	}
	return NodeId{static_cast<uint32_t>(index)};
}

std::variant<uint8_t, ast::Diagnostic> CheckParameterPosition(uint64_t position, const ast::Span& span) {
	if (position > std::numeric_limits<uint8_t>::max()) {
		return error(span, "Tracker parameter position exceeds the uint8_t ABI (indices 0 through 255).");
	}
	return static_cast<uint8_t>(position);
}

std::variant<std::string, ast::Diagnostic> NormalizeSourcePath(
	const ast::Span& span, const std::filesystem::path& sourceRoot)
{
	if (!sourceRoot.is_absolute()) {
		return error(span, "Tracker source ordering requires an absolute project source root.");
	}
	if (span.file.empty()) {
		return error(span, "Tracker source ordering requires a source file path.");
	}
	const auto root = sourceRoot.lexically_normal();
	const std::filesystem::path source{span.file};
	// Lexical paths also work for in-memory sources; external sources retain "..".
	const auto absolute = (source.is_absolute() ? source : root / source).lexically_normal();
	const auto relative = absolute.lexically_relative(root);
	if (relative.empty() || relative == "." || relative.is_absolute()) {
		return error(span, "Tracker source path cannot be made project-relative: '" + span.file + "'.");
	}
	const auto utf8 = relative.generic_u8string();
	return std::string(utf8.begin(), utf8.end());
}

std::variant<std::vector<OrderedSource>, std::vector<ast::Diagnostic>> OrderSources(
	const ast::Project& project, const std::filesystem::path& sourceRoot)
{
	if (!sourceRoot.is_absolute()) {
		return std::vector<ast::Diagnostic>{
			error({}, "Tracker source ordering requires an absolute project source root.")};
	}
	std::vector<OrderedSource> ordered;
	std::vector<ast::Diagnostic> diagnostics;
	for (const auto& file : project.files) {
		auto normalized = NormalizeSourcePath(ast::Span{file.path}, sourceRoot);
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&normalized)) {
			diagnostics.push_back(*diagnostic);
		} else {
			ordered.push_back({&file, std::get<std::string>(std::move(normalized))});
		}
	}
	std::sort(ordered.begin(), ordered.end(),
		[](const OrderedSource& a, const OrderedSource& b) { return a.path < b.path; });
	for (size_t i = 1; i < ordered.size(); ++i) {
		if (ordered[i - 1].path == ordered[i].path) {
			diagnostics.push_back(error(ast::Span{ordered[i].file->path},
				"Tracker source ordering found duplicate normalized path '" + ordered[i].path + "'."));
		}
	}
	if (!diagnostics.empty()) {
		return diagnostics;
	}
	return ordered;
}

std::variant<GraphStructure, std::vector<ast::Diagnostic>> InspectGraph(
	const Graph& graph, const std::filesystem::path& sourceRoot)
{
	if (!sourceRoot.is_absolute()) {
		return std::vector<ast::Diagnostic>{
			error({}, "Tracker source ordering requires an absolute project source root.")};
	}
	std::vector<ast::Diagnostic> diagnostics;
	const auto count = CheckNodeCount(graph.nodes.size(), {});
	if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&count)) {
		diagnostics.push_back(*diagnostic);
	}
	const auto checkReference = [&](NodeHandle handle, const ast::Span& span) {
		if (handle.slot >= graph.nodes.size()) {
			diagnostics.push_back(error(span, "Tracker graph references an invalid node handle."));
		}
	};
	std::unordered_set<const ast::DefineDecl*> declarations;
	for (const auto& helper : graph.helpers) {
		const auto span = helper.declaration ? helper.declaration->span : ast::Span{};
		if (!helper.declaration) {
			diagnostics.push_back(error(span, "Tracker helper is missing its declaration."));
		} else if (!declarations.insert(helper.declaration).second) {
			diagnostics.push_back(error(span, "Tracker helper '" +
				helper.declaration->name.text + "' has more than one registered handle."));
		}
		if (!helper.body) {
			diagnostics.push_back(error(span, "Tracker helper definition is incomplete."));
		} else {
			checkReference(*helper.body, span);
		}
	}
	for (const auto& adapter : graph.adapters) {
		if (!adapter.declaration) {
			diagnostics.push_back(error({}, "Tracker host adapter is missing its declaration."));
		}
	}

	// Check every stored edge, including unreachable nodes, before indexing by handle.
	std::vector<std::vector<NodeHandle>> edges;
	edges.reserve(graph.nodes.size());
	for (const auto& node : graph.nodes) {
		auto children = references(graph, node, diagnostics);
		for (const auto child : children) {
			checkReference(child, node.provenance.span);
		}
		edges.push_back(std::move(children));
	}

	std::vector<std::string> rootPaths(graph.roots.size());
	std::set<std::tuple<ast::SectionKind, std::string, std::string, uint32_t>> rootKeys;
	for (size_t i = 0; i < graph.roots.size(); ++i) {
		const auto& root = graph.roots[i];
		checkReference(root.expression, root.provenance.span);
		if (!root.owner || !root.entry) {
			diagnostics.push_back(error(root.provenance.span,
				"Tracker root is missing its owner or entry declaration."));
		} else if (!rootKeys.emplace(root.kind, root.owner->key.text,
			root.entry->name.text, root.occurrence).second) {
			diagnostics.push_back(error(root.provenance.span,
				"Tracker graph contains a duplicate root identity for '" + root.entry->name.text + "'."));
		}
		auto normalized = NormalizeSourcePath(root.provenance.span, sourceRoot);
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&normalized)) {
			diagnostics.push_back(*diagnostic);
		} else {
			rootPaths[i] = std::get<std::string>(std::move(normalized));
		}
	}
	if (!diagnostics.empty()) {
		return diagnostics;
	}
	return GraphStructure{std::move(edges), std::move(rootPaths)};
}

std::variant<Numbering, std::vector<ast::Diagnostic>> NumberGraph(
	const Graph& graph, const GraphStructure& structure)
{
	const auto& [edges, rootPaths] = structure;

	Numbering numbering{NodeCount{0}, {}, std::vector<std::optional<NodeId>>(graph.nodes.size()), {}};
	numbering.rootsInOrder.resize(graph.roots.size());
	std::iota(numbering.rootsInOrder.begin(), numbering.rootsInOrder.end(), size_t{0});
	// Source positions preserve entry order; symbolic identity breaks ties without
	// depending on allocation addresses or the graph's storage order.
	const auto rootOrder = [&](size_t i) {
		const auto& root = graph.roots[i];
		const auto& span = root.provenance.span;
		return std::tie(rootPaths[i], span.start.line, span.start.column,
			span.end.line, span.end.column, root.owner->key.text, root.kind,
			root.entry->name.text, root.occurrence);
	};
	std::sort(numbering.rootsInOrder.begin(), numbering.rootsInOrder.end(),
		[&](size_t a, size_t b) { return rootOrder(a) < rootOrder(b); });

	// Reverse pushes onto the LIFO stack produce source/child-order preorder.
	// Only root-reachable nodes receive IDs; other slots remain unassigned.
	std::vector<NodeHandle> pending;
	for (auto it = numbering.rootsInOrder.rbegin(); it != numbering.rootsInOrder.rend(); ++it) {
		pending.push_back(graph.roots[*it].expression);
	}
	while (!pending.empty()) {
		const auto handle = pending.back();
		pending.pop_back();
		if (numbering.idsBySlot[handle.slot]) {
			continue;
		}
		auto id = CheckNodeId(numbering.nodesById.size(), graph.nodes[handle.slot].provenance.span);
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&id)) {
			return std::vector<ast::Diagnostic>{*diagnostic};
		}
		// Mark before following edges so self/mutual cycles terminate without recursion.
		numbering.idsBySlot[handle.slot] = std::get<NodeId>(id);
		numbering.nodesById.push_back(handle);
		const auto& children = edges[handle.slot];
		pending.insert(pending.end(), children.rbegin(), children.rend());
	}
	// Each assigned ID was checked before narrowing; count may be one larger than the last ID.
	numbering.count = NodeCount{static_cast<uint64_t>(numbering.nodesById.size())};
	return numbering;
}

std::variant<Numbering, std::vector<ast::Diagnostic>> NumberDistinctGraph(
	const Graph& graph, const GraphStructure& structure)
{
	auto numbered = NumberGraph(graph, structure);
	if (const auto* diagnostics = std::get_if<std::vector<ast::Diagnostic>>(&numbered)) {
		return *diagnostics;
	}
	const auto& occurrences = std::get<Numbering>(numbered);
	const auto dependencies = [&](NodeHandle handle) {
		std::span<const NodeHandle> children = structure.edges[handle.slot];
		// A helper's identity implies its body; exclude that edge from structural keys.
		if (std::holds_alternative<HelperCall>(graph.nodes[handle.slot].payload)) {
			children = children.first(children.size() - 1);
		}
		return children;
	};
	std::unordered_map<NodeKey, size_t, NodeKeyHash> interned;
	std::vector<std::optional<size_t>> classes(graph.nodes.size());
	std::vector<bool> active(graph.nodes.size(), false);
	struct Frame {
		NodeHandle handle;
		size_t nextChild;
	};
	std::vector<Frame> pending;
	for (const auto handle : occurrences.nodesById) {
		if (classes[handle.slot]) continue;
		pending.push_back({handle, 0});
		active[handle.slot] = true;
		while (!pending.empty()) {
			auto& frame = pending.back();
			const auto current = frame.handle;
			const auto children = dependencies(current);
			if (frame.nextChild < children.size()) {
				const auto child = children[frame.nextChild++];
				if (classes[child.slot]) continue;
				if (active[child.slot]) {
					return std::vector<ast::Diagnostic>{error(graph.nodes[child.slot].provenance.span,
						"Tracker structural cycle cannot be deduplicated.")};
				}
				active[child.slot] = true;
				pending.push_back({child, 0});
				continue;
			}
			std::vector<size_t> childClasses;
			childClasses.reserve(children.size());
			for (const auto child : children) childClasses.push_back(classes[child.slot].value());
			const auto [found, inserted] = interned.emplace(
				nodeKey(graph.nodes[current.slot], std::move(childClasses)), interned.size());
			classes[current.slot] = found->second;
			active[current.slot] = false;
			pending.pop_back();
		}
	}

	Numbering distinct{NodeCount{0}, {}, std::vector<std::optional<NodeId>>(graph.nodes.size()),
		occurrences.rootsInOrder};
	std::vector<std::optional<NodeId>> idsByClass(interned.size());
	// Intern-table order and declaration addresses never determine emitted IDs.
	for (const auto handle : occurrences.nodesById) {
		auto& id = idsByClass[classes[handle.slot].value()];
		if (!id) {
			auto checked = CheckNodeId(distinct.nodesById.size(), graph.nodes[handle.slot].provenance.span);
			if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&checked)) {
				return std::vector<ast::Diagnostic>{*diagnostic};
			}
			id = std::get<NodeId>(checked);
			distinct.nodesById.push_back(handle);
		}
		distinct.idsBySlot[handle.slot] = id;
	}
	distinct.count = NodeCount{static_cast<uint64_t>(distinct.nodesById.size())};
	return distinct;
}

} // namespace rls::transpilers::soh_tracker::ir
