#include "collect_declarations.h"

#include <format>

namespace rls::sema {

std::vector<ast::Diagnostic> collectDeclarations(ast::Project& project) {
	std::vector<ast::Diagnostic> diagnostics;

	// Clear any previous state so the function is idempotent.
	project.RegionDecls.clear();
	project.ExtendRegionDecls.clear();
	project.DefineDecls.clear();
	project.EnemyDecls.clear();

	for (auto& file : project.files) {
		for (auto& decl : file.declarations) {
			std::visit([&](auto& d) {
				using T = std::decay_t<decltype(d)>;

				if constexpr (std::is_same_v<T, ast::RegionDecl>) {
					auto [it, inserted] = project.RegionDecls.try_emplace(d.name, &d);
					if (!inserted) {
						diagnostics.push_back({
							ast::DiagnosticLevel::Error,
							std::format("duplicate region '{}' (first declared at {}:{})",
								d.name,
								it->second->span.file,
								it->second->span.start.line),
							d.span
						});
					}
				}
				else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
					project.ExtendRegionDecls.emplace(d.name, &d);
				}
				else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
					auto [it, inserted] = project.DefineDecls.try_emplace(d.name, &d);
					if (!inserted) {
						diagnostics.push_back({
							ast::DiagnosticLevel::Error,
							std::format("duplicate define '{}' (first declared at {}:{})",
								d.name,
								it->second->span.file,
								it->second->span.start.line),
							d.span
						});
					}
				}
				else if constexpr (std::is_same_v<T, ast::EnemyDecl>) {
					auto [it, inserted] = project.EnemyDecls.try_emplace(d.name, &d);
					if (!inserted) {
						diagnostics.push_back({
							ast::DiagnosticLevel::Error,
							std::format("duplicate enemy '{}' (first declared at {}:{})",
								d.name,
								it->second->span.file,
								it->second->span.start.line),
							d.span
						});
					}
				}
			}, decl);
		}
	}

	return diagnostics;
}

} // namespace rls::sema