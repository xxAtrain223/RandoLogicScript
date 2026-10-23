#pragma once

#include <filesystem>
#include <string>

#include "tracker_ir.h"

namespace rls::transpilers::soh_tracker::ir {

std::variant<NodeCount, ast::Diagnostic> CheckNodeCount(uint64_t count, const ast::Span& span);
std::variant<NodeId, ast::Diagnostic> CheckNodeId(uint64_t index, const ast::Span& span);
std::variant<uint8_t, ast::Diagnostic> CheckParameterPosition(uint64_t position, const ast::Span& span);

// Lexical normalization only: no filesystem access or dependency on the process CWD.
std::variant<std::string, ast::Diagnostic> NormalizeSourcePath(
	const ast::Span& span, const std::filesystem::path& sourceRoot);

struct OrderedSource {
	const ast::File* file;
	std::string path;
};

// Traverse each file's declarations, sections, entries and arguments in source order.
std::variant<std::vector<OrderedSource>, std::vector<ast::Diagnostic>> OrderSources(
	const ast::Project& project, const std::filesystem::path& sourceRoot);

struct Numbering {
	NodeCount count;
	std::vector<NodeHandle> nodesById;
	std::vector<std::optional<NodeId>> idsBySlot;
	std::vector<size_t> rootsInOrder;
};

// Checks reference integrity throughout the graph; numbers only root-reachable nodes.
// This is not the full target type/ABI validator.
std::variant<Numbering, std::vector<ast::Diagnostic>> NumberGraph(
	const Graph& graph, const std::filesystem::path& sourceRoot);

} // namespace rls::transpilers::soh_tracker::ir
