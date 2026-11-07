#pragma once

#include "tracker_validation.h"

namespace rls::transpilers::soh_tracker::ir::testing_helpers {

inline std::vector<ast::Diagnostic> ValidateGraphStructure(
	const Graph& graph, const std::filesystem::path& sourceRoot)
{
	auto result = InspectGraph(graph, sourceRoot);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result)) return *errors;
	return {};
}

inline std::vector<ast::Diagnostic> ValidateGraph(const ast::Project& project, const Graph& graph,
	const std::filesystem::path& sourceRoot, std::span<const HostEnumBinding> customEnums = {})
{
	auto result = PreflightGraph(project, graph, sourceRoot, customEnums);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result)) return *errors;
	return {};
}

inline std::variant<Numbering, std::vector<ast::Diagnostic>> NumberGraph(
	const Graph& graph, const std::filesystem::path& sourceRoot)
{
	auto result = InspectGraph(graph, sourceRoot);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result)) return *errors;
	return ir::NumberGraph(graph, std::get<GraphStructure>(result));
}

inline std::variant<Numbering, std::vector<ast::Diagnostic>> NumberDistinctGraph(
	const Graph& graph, const std::filesystem::path& sourceRoot)
{
	auto result = InspectGraph(graph, sourceRoot);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&result)) return *errors;
	return ir::NumberDistinctGraph(graph, std::get<GraphStructure>(result));
}

using AdapterRenderingResult =
	std::variant<std::vector<RenderedAdapter>, std::vector<ast::Diagnostic>>;

inline AdapterRenderingResult RenderHostAdapters(const Graph& graph,
	std::span<const HostEnumBinding> customEnums = {})
{
	auto diagnostics = ValidateHostAdapters(graph, customEnums);
	if (!diagnostics.empty()) return diagnostics;
	return RenderValidatedHostAdapters(graph, customEnums);
}

} // namespace rls::transpilers::soh_tracker::ir::testing_helpers
