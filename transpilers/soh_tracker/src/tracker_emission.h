#pragma once

#include "tracker_validation.h"

namespace rls::transpilers::soh_tracker::ir {

// Validates, numbers, and normalizes provenance before generating into output streams.
std::vector<ast::Diagnostic> EmitArtifacts(const ast::Project& project, const Graph& graph,
	rls::OutputWriter& out, const std::filesystem::path& sourceRoot,
	std::span<const HostEnumBinding> customEnums = {});

} // namespace rls::transpilers::soh_tracker::ir
