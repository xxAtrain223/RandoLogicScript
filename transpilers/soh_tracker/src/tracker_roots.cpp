#include "tracker_roots.h"

namespace rls::transpilers::soh_tracker::ir {

std::variant<std::vector<RootSource>, std::vector<ast::Diagnostic>> CollectRoots(
	const ast::Project& project, const std::filesystem::path& sourceRoot)
{
	auto sources = OrderSources(project, sourceRoot);
	if (const auto* diagnostics = std::get_if<std::vector<ast::Diagnostic>>(&sources)) {
		return *diagnostics;
	}

	std::vector<RootSource> roots;
	std::vector<ast::Diagnostic> diagnostics;
	for (const auto& source : std::get<std::vector<OrderedSource>>(sources)) {
		for (const auto& declaration : source.file->declarations) {
			const auto* region = std::get_if<ast::RegionDecl>(&declaration);
			const auto* extension = std::get_if<ast::ExtendRegionDecl>(&declaration);
			if (!region && !extension) {
				continue;
			}

			const auto& key = region ? region->key : extension->name;
			const auto resolved = project.RegionDecls.find(key.text);
			if (resolved == project.RegionDecls.end() || !resolved->second ||
				(region && resolved->second != region)) {
				diagnostics.push_back({"", key.span, ast::DiagnosticLevel::Error,
					"Tracker root collection requires an analyzed region declaration for '" +
					key.text + "'."});
				continue;
			}

			const auto& sections = region ? region->body.sections : extension->sections;
			for (const auto& section : sections) {
				for (const auto& entry : section.entries) {
					roots.push_back({section.kind, resolved->second, &entry, 0});
				}
			}
		}
	}
	if (!diagnostics.empty()) {
		return diagnostics;
	}
	return roots;
}

} // namespace rls::transpilers::soh_tracker::ir
