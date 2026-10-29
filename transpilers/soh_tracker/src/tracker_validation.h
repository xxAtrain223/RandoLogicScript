#pragma once

#include <filesystem>

#include "tracker_adapters.h"

namespace rls::transpilers::soh_tracker::ir {

// Pure preflight over every stored descriptor, not just evaluated/reachable branches.
// Returns diagnostics only; numbering and code generation are separate steps.
std::vector<ast::Diagnostic> ValidateGraph(const ast::Project& project, const Graph& graph,
	const std::filesystem::path& sourceRoot, std::span<const HostEnumBinding> customEnums = {});

} // namespace rls::transpilers::soh_tracker::ir
