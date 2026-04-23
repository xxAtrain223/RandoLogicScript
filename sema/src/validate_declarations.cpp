#include "validate_declarations.h"
#include "type_helpers.h"

#include <algorithm>
#include <format>
#include <queue>
#include <unordered_map>
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

/// Check 1: Every extend-region must target a declared region.
static void checkExtendRegionTargets(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	for (auto& [name, decl] : project.ExtendRegionDecls) {
		if (!project.RegionDecls.contains(name)) {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("extend region targets unknown region '{}'", name),
				decl->span
			});
		}
	}
}

/// Check 2: Every enemy must have a 'kill' field.
static void checkEnemyKillFields(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	for (auto& [name, decl] : project.EnemyDecls) {
		bool hasKill = std::any_of(
			decl->fields.begin(), decl->fields.end(),
			[](const ast::EnemyField& f) { return f.kind == ast::EnemyFieldKind::Kill; });
		if (!hasKill) {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("enemy '{}' must have a 'kill' field", name),
				decl->span
			});
		}
	}
}

/// Check 3: No duplicate entries across base region + all its extensions
///           within the same SectionKind.
static void checkDuplicateEntries(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	for (auto& [regionName, regionDecl] : project.RegionDecls) {
		std::unordered_map<ast::SectionKind, std::unordered_set<std::string>> seen;

		auto checkEntries = [&](const std::vector<ast::Section>& sections) {
			for (const auto& section : sections) {
				auto& set = seen[section.kind];
				for (const auto& entry : section.entries) {
					if (!set.insert(entry.name).second) {
						diags.push_back({
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
}

/// Check 4: Entry conditions must be Bool-compatible.
static void checkEntryConditionTypes(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	auto check = [&](const std::vector<ast::Section>& sections,
		const std::string& regionName) {
		for (const auto& section : sections) {
			for (const auto& entry : section.entries) {
				auto condType = project.getType(entry.condition.get());
				if (!condType || *condType == ast::Type::Error) continue;
				if (!isBoolCompatible(*condType)) {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format("{} condition for '{}' in region '{}' must be Bool, got {}",
							sectionKindName(section.kind), entry.name, regionName,
							typeName(*condType)),
						entry.span
					});
				}
			}
		}
	};

	for (auto& [name, decl] : project.RegionDecls) {
		check(decl->body.sections, name);
	}
	for (auto& [name, decl] : project.ExtendRegionDecls) {
		check(decl->sections, name);
	}
}

/// Check 5: Every region must be reachable from RR_ROOT via exits.
static void checkRegionReachability(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	if (!project.RegionDecls.contains("RR_ROOT")) return;

	// Build directed graph: region → set of regions reachable via exits.
	std::unordered_map<std::string, std::unordered_set<std::string>> graph;

	for (auto& [regionName, decl] : project.RegionDecls) {
		auto& targets = graph[regionName];
		for (const auto& section : decl->body.sections) {
			if (section.kind == ast::SectionKind::Exits) {
				for (const auto& entry : section.entries) {
					targets.insert(entry.name);
				}
			}
		}
	}
	for (auto& [regionName, decl] : project.ExtendRegionDecls) {
		if (!project.RegionDecls.contains(regionName)) continue;
		auto& targets = graph[regionName];
		for (const auto& section : decl->sections) {
			if (section.kind == ast::SectionKind::Exits) {
				for (const auto& entry : section.entries) {
					targets.insert(entry.name);
				}
			}
		}
	}

	// BFS from RR_ROOT.
	std::unordered_set<std::string> visited;
	std::queue<std::string> frontier;
	frontier.push("RR_ROOT");
	visited.insert("RR_ROOT");

	while (!frontier.empty()) {
		auto current = frontier.front();
		frontier.pop();
		if (auto it = graph.find(current); it != graph.end()) {
			for (const auto& target : it->second) {
				if (visited.insert(target).second) {
					frontier.push(target);
				}
			}
		}
	}

	for (auto& [regionName, decl] : project.RegionDecls) {
		if (!visited.contains(regionName)) {
			diags.push_back({
				ast::DiagnosticLevel::Warning,
				std::format("region '{}' is not reachable from 'RR_ROOT'", regionName),
				decl->span
			});
		}
	}
}

/// Check 6: Every define should be referenced somewhere.
static void checkUnusedDefines(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	if (project.DefineDecls.empty()) return;

	std::unordered_set<std::string> usedFunctions;

	// Walk all expression trees to collect function call names.
	auto collectFromSections = [&](const std::vector<ast::Section>& sections) {
		for (const auto& section : sections) {
			for (const auto& entry : section.entries) {
				collectCallNames(*entry.condition, usedFunctions);
			}
		}
	};

	for (auto& [name, decl] : project.RegionDecls) {
		collectFromSections(decl->body.sections);
	}
	for (auto& [name, decl] : project.ExtendRegionDecls) {
		collectFromSections(decl->sections);
	}
	for (auto& [name, decl] : project.DefineDecls) {
		collectCallNames(*decl->body, usedFunctions);
		for (const auto& param : decl->params) {
			if (param.defaultValue) {
				collectCallNames(*param.defaultValue, usedFunctions);
			}
		}
	}
	for (auto& [name, decl] : project.EnemyDecls) {
		for (const auto& field : decl->fields) {
			collectCallNames(*field.body, usedFunctions);
		}
	}

	for (auto& [name, decl] : project.DefineDecls) {
		if (!usedFunctions.contains(name)) {
			diags.push_back({
				ast::DiagnosticLevel::Info,
				std::format("'{}' is defined but never used", name),
				decl->span
			});
		}
	}
}

/// Check 7: Define/extern signatures must have valid parameter shapes and
/// typed defaults must match annotated parameter types.
static void checkFunctionSignatures(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	auto validateParams = [&](const std::string& kind,
		const std::string& name,
		const std::vector<ast::Param>& params,
		const ast::Span& declSpan) {
		std::unordered_set<std::string> seenNames;
		bool seenDefault = false;

		for (const auto& param : params) {
			if (!seenNames.insert(param.name).second) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format(
						"duplicate parameter '{}' in {} '{}'",
						param.name, kind, name),
					declSpan
				});
			}

			if (param.defaultValue) {
				seenDefault = true;
			} else if (seenDefault) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format(
						"required parameter '{}' cannot follow optional parameters in {} '{}'",
						param.name, kind, name),
					declSpan
				});
			}

			if (!param.type || !param.defaultValue) continue;

			auto paramType = project.getType(&param);
			auto defaultType = project.getType(param.defaultValue.get());
			if (!paramType || !defaultType) continue;
			if (*paramType == ast::Type::Error || *defaultType == ast::Type::Error) {
				continue;
			}
			if (*paramType != *defaultType) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format(
						"default value for parameter '{}' in {} '{}' has type {}, expected {}",
						param.name,
						kind,
						name,
						typeName(*defaultType),
						typeName(*paramType)),
					param.defaultValue->span
				});
			}
		}
	};

	for (const auto& [name, decl] : project.DefineDecls) {
		validateParams("define", name, decl->params, decl->span);
	}
	for (const auto& [name, decl] : project.ExternDefineDecls) {
		validateParams("extern define", name, decl->params, decl->span);
	}
}

std::vector<ast::Diagnostic> validateDeclarations(ast::Project& project) {
	std::vector<ast::Diagnostic> diags;

	checkExtendRegionTargets(project, diags);
	checkEnemyKillFields(project, diags);
	checkDuplicateEntries(project, diags);
	checkEntryConditionTypes(project, diags);
	checkRegionReachability(project, diags);
	checkUnusedDefines(project, diags);
	checkFunctionSignatures(project, diags);

	return diags;
}

} // namespace rls::sema
