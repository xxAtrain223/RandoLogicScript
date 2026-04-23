#include "collect_declarations.h"

#include <format>

namespace rls::sema {

std::vector<ast::Diagnostic> collectDeclarations(ast::Project& project) {
	std::vector<ast::Diagnostic> diagnostics;

	auto emitDuplicate = [&](std::string_view kind, std::string_view name,
	                        const ast::Span& first, const ast::Span& duplicate) {
		diagnostics.push_back({
			ast::DiagnosticLevel::Error,
			std::format("duplicate {} '{}' (first declared at {}:{})",
				kind, name, first.file, first.start.line),
			duplicate
		});
	};

	// Clear any previous state so the function is idempotent.
	project.RegionDecls.clear();
	project.ExtendRegionDecls.clear();
	project.DefineDecls.clear();
	project.ExternDefineDecls.clear();
	project.EnemyDecls.clear();

	for (auto& file : project.files) {
		for (auto& decl : file.declarations) {
			std::visit([&](auto& d) {
				using T = std::decay_t<decltype(d)>;

				if constexpr (std::is_same_v<T, ast::RegionDecl>) {
					auto [it, inserted] = project.RegionDecls.try_emplace(d.key, &d);
					if (!inserted) {
						emitDuplicate("region", d.key, it->second->span, d.span);
					}
				}
				else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
					project.ExtendRegionDecls.emplace(d.name, &d);
				}
				else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
					if (auto it = project.DefineDecls.find(d.name);
						it != project.DefineDecls.end()) {
						emitDuplicate("define", d.name, it->second->span, d.span);
					}
					else if (auto it = project.ExternDefineDecls.find(d.name);
					         it != project.ExternDefineDecls.end()) {
						emitDuplicate("function", d.name, it->second->span, d.span);
					}
					else {
						project.DefineDecls.emplace(d.name, &d);
					}
				}
				else if constexpr (std::is_same_v<T, ast::ExternDefineDecl>) {
					if (auto it = project.ExternDefineDecls.find(d.name);
						it != project.ExternDefineDecls.end()) {
						emitDuplicate("extern define", d.name, it->second->span, d.span);
					}
					else if (auto it = project.DefineDecls.find(d.name);
					         it != project.DefineDecls.end()) {
						emitDuplicate("function", d.name, it->second->span, d.span);
					}
					else {
						project.ExternDefineDecls.emplace(d.name, &d);
					}
				}
				else if constexpr (std::is_same_v<T, ast::EnemyDecl>) {
					auto [it, inserted] = project.EnemyDecls.try_emplace(d.name, &d);
					if (!inserted) {
						emitDuplicate("enemy", d.name, it->second->span, d.span);
					}
				}
			}, decl);
		}
	}

	return diagnostics;
}

} // namespace rls::sema