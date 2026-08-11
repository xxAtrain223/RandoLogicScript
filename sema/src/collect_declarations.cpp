#include "collect_declarations.h"

#include <algorithm>
#include <format>

namespace rls::sema {

std::vector<ast::Diagnostic> collectDeclarations(ast::Project& project) {
	std::vector<ast::Diagnostic> diagnostics;

	auto emitDuplicate = [&](std::string_view kind, std::string_view name,
	                        const ast::Span& first, const ast::Span& duplicate) {
		diagnostics.push_back(ast::Diagnostic{
			"", duplicate, ast::DiagnosticLevel::Error,
			std::format("duplicate {} '{}' (first declared at {}:{})",
				kind, name, first.file, first.start.line)});
	};

	// Clear any previous state so the function is idempotent.
	project.RegionDecls.clear();
	project.ExtendRegionDecls.clear();
	project.DefineDecls.clear();
	project.ExternDefineDecls.clear();
	project.EnumInfos.clear();

	for (auto& file : project.files) {
		for (auto& decl : file.declarations) {
			std::visit([&](auto& d) {
				using T = std::decay_t<decltype(d)>;

				if constexpr (std::is_same_v<T, ast::RegionDecl>) {
					auto [it, inserted] = project.RegionDecls.try_emplace(d.key.text, &d);
					if (!inserted) {
						emitDuplicate("region", d.key.text, it->second->span, d.span);
					}
				}
				else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
					project.ExtendRegionDecls[d.name.text].push_back(&d);
				}
				else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
					if (auto it = project.DefineDecls.find(d.name.text);
						it != project.DefineDecls.end()) {
						emitDuplicate("define", d.name.text, it->second->span, d.span);
					}
					else if (auto it = project.ExternDefineDecls.find(d.name.text);
					         it != project.ExternDefineDecls.end()) {
						emitDuplicate("function", d.name.text, it->second->span, d.span);
					}
					else {
						project.DefineDecls.emplace(d.name.text, &d);
					}
				}
				else if constexpr (std::is_same_v<T, ast::ExternDefineDecl>) {
					if (auto it = project.ExternDefineDecls.find(d.name.text);
						it != project.ExternDefineDecls.end()) {
						emitDuplicate("extern define", d.name.text, it->second->span, d.span);
					}
					else if (auto it = project.DefineDecls.find(d.name.text);
					         it != project.DefineDecls.end()) {
						emitDuplicate("function", d.name.text, it->second->span, d.span);
					}
					else {
						project.ExternDefineDecls.emplace(d.name.text, &d);
					}
				}
				else if constexpr (std::is_same_v<T, ast::EnumDecl>) {
					auto [it, inserted] = project.EnumInfos.try_emplace(d.name.text);
					if (!inserted) {
						emitDuplicate("enum", d.name.text, it->second.span, d.span);
					} else {
						std::vector<ast::EnumEntryInfo> entries;
						entries.reserve(d.members.size());
						for (const auto& member : d.members) {
							entries.emplace_back(ast::EnumMemberInfo{
								member.name,
								member.explicitValue,
								member.span,
							});
						}

						it->second = ast::EnumInfo(
							d.name,
							ast::EnumKind::Normal,
							ast::Type::Int,
							std::move(entries),
							d.span);
					}
				}
				else if constexpr (std::is_same_v<T, ast::ExternEnumDecl>) {
					auto [it, inserted] = project.EnumInfos.try_emplace(d.name.text);
					if (!inserted) {
						emitDuplicate("enum", d.name.text, it->second.span, d.span);
					} else {
						std::vector<ast::EnumEntryInfo> entries;
						entries.reserve(d.entries.size());

						for (const auto& entry : d.entries) {
							if (std::holds_alternative<ast::EnumMemberDecl>(entry)) {
								const auto& member = std::get<ast::EnumMemberDecl>(entry);
								entries.emplace_back(ast::EnumMemberInfo{
									member.name,
									member.explicitValue,
									member.span,
								});
							} else {
								const auto& pattern = std::get<ast::EnumPatternDecl>(entry);
								entries.emplace_back(ast::EnumPatternInfo{
									pattern.pattern,
									pattern.span,
								});
							}
						}

						it->second = ast::EnumInfo(
							d.name,
							ast::EnumKind::Extern,
							ast::Type::Int,
							std::move(entries),
							d.span);
					}
				}
			}, decl);
		}
	}

	for (auto& [regionName, decls] : project.ExtendRegionDecls) {
		std::sort(decls.begin(), decls.end(),
		          [](const ast::ExtendRegionDecl* a, const ast::ExtendRegionDecl* b) {
			          return std::tie(a->span.file, a->span.start.line) <
			                 std::tie(b->span.file, b->span.start.line);
		          });
	}

	return diagnostics;
}

} // namespace rls::sema