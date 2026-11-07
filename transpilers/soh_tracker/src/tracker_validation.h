#pragma once

#include <filesystem>

#include "tracker_adapters.h"
#include "tracker_graph.h"

namespace rls::transpilers::soh_tracker::ir {

// Validates every stored descriptor and returns the inspected structure for numbering.
std::variant<GraphStructure, std::vector<ast::Diagnostic>> PreflightGraph(
	const ast::Project& project, const Graph& graph, const std::filesystem::path& sourceRoot,
	std::span<const HostEnumBinding> customEnums = {});

} // namespace rls::transpilers::soh_tracker::ir
