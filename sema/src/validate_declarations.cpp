#include "validate_declarations.h"

#include <algorithm>
#include <format>
#include <unordered_set>

namespace rls::sema {

static const char* sectionKindName(ast::SectionKind kind) {
	switch (kind) {
	case ast::SectionKind::Events:    return "event";
	case ast::SectionKind::Locations: return "location";
	case ast::SectionKind::Exits:     return "exit";
	}
	return "entry";
}

std::vector<ast::Diagnostic> validateDeclarations(ast::Project& project) {
	std::vector<ast::Diagnostic> diagnostics;

	// Check 1: Every extend-region must target a declared region.
	for (auto& [name, decl] : project.ExtendRegionDecls) {
		if (!project.RegionDecls.contains(name)) {
			diagnostics.push_back({
				ast::DiagnosticLevel::Error,
				std::format("extend region targets unknown region '{}'", name),
				decl->span
			});
		}
	}

	// Check 2: Every enemy must have a 'kill' field.
	for (auto& [name, decl] : project.EnemyDecls) {
		bool hasKill = std::any_of(
			decl->fields.begin(), decl->fields.end(),
			[](const ast::EnemyField& f) { return f.kind == ast::EnemyFieldKind::Kill; });
		if (!hasKill) {
			diagnostics.push_back({
				ast::DiagnosticLevel::Error,
				std::format("enemy '{}' must have a 'kill' field", name),
				decl->span
			});
		}
	}

	// Check 3: No duplicate entries across base region + all its extensions
	//           within the same SectionKind.
	for (auto& [regionName, regionDecl] : project.RegionDecls) {
		// Collect all sections: from the base region and from extensions.
		std::unordered_map<ast::SectionKind, std::unordered_set<std::string>> seen;

		auto checkEntries = [&](const std::vector<ast::Section>& sections) {
			for (const auto& section : sections) {
				auto& set = seen[section.kind];
				for (const auto& entry : section.entries) {
					if (!set.insert(entry.name).second) {
						diagnostics.push_back({
							ast::DiagnosticLevel::Error,
							std::format("duplicate {} '{}' in region '{}'",
								sectionKindName(section.kind), entry.name, regionName),
							entry.span
						});
					}
				}
			}
		};

		checkEntries(regionDecl->body.sections);

		auto range = project.ExtendRegionDecls.equal_range(regionName);
		for (auto it = range.first; it != range.second; ++it) {
			checkEntries(it->second->sections);
		}
	}

	return diagnostics;
}

} // namespace rls::sema
