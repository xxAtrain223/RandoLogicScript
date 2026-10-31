#pragma once

#include "tracker_validation.h"

namespace rls::transpilers::soh_tracker::ir {

struct RenderedArtifacts {
	std::string header;
	std::string source;
	std::string manifest;
	bool operator==(const RenderedArtifacts&) const = default;
};

// Validates the entire graph, then numbers and renders without opening outputs.
std::variant<RenderedArtifacts, std::vector<ast::Diagnostic>> RenderArtifacts(
	const ast::Project& project, const Graph& graph, const std::filesystem::path& sourceRoot,
	std::span<const HostEnumBinding> customEnums = {});

std::vector<ast::Diagnostic> EmitArtifacts(const ast::Project& project, const Graph& graph,
	rls::OutputWriter& out, const std::filesystem::path& sourceRoot,
	std::span<const HostEnumBinding> customEnums = {});

} // namespace rls::transpilers::soh_tracker::ir
