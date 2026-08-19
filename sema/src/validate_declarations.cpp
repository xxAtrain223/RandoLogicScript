#include "validate_declarations.h"
#include "diagnostics.h"
#include "type_helpers.h"

#include <algorithm>
#include <queue>
#include <set>
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

static void checkEnumDeclarations(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	std::unordered_map<std::string, std::set<std::string>> valueNameToEnums;

	for (auto& [enumName, info] : project.EnumInfos) {
		if (info.kind == ast::EnumKind::Normal) {
			std::unordered_set<std::string> seenNames;
			std::unordered_set<int> seenValues;
			std::vector<ast::EnumEntryInfo> normalized;
			normalized.reserve(info.entries.size());

			int nextValue = 0;
			for (const auto& entry : info.entries) {
				if (std::holds_alternative<ast::EnumPatternInfo>(entry)) {
					const auto& pattern = std::get<ast::EnumPatternInfo>(entry);
					diags.push_back(diagnostics::EnumWildcardPattern(pattern.span, enumName, pattern.pattern));
					continue;
				}

				auto member = std::get<ast::EnumMemberInfo>(entry);
				if (!seenNames.insert(member.name.text).second) {
					diags.push_back(diagnostics::EnumDuplicateMember(member.span, member.name.text, enumName));
					continue;
				}

				if (member.value.has_value()) {
					nextValue = *member.value;
				} else {
					member.value = nextValue;
				}

				if (!seenValues.insert(*member.value).second) {
					diags.push_back(diagnostics::EnumDuplicateValue(member.span, *member.value, enumName));
				}

				nextValue = *member.value + 1;
				normalized.emplace_back(std::move(member));
			}

			info.entries = std::move(normalized);

			for (const auto& entry : info.entries) {
				if (std::holds_alternative<ast::EnumMemberInfo>(entry)) {
					const auto& member = std::get<ast::EnumMemberInfo>(entry);
					valueNameToEnums[member.name.text].insert(enumName);
				}
			}
			continue;
		}

		// Extern enums: explicit members and wildcard patterns are allowed.
		if (info.entries.empty()) {
			diags.push_back(diagnostics::ExternEnumEmpty(info.span, enumName));
			continue;
		}

		std::unordered_set<std::string> explicitNames;
		std::vector<std::string> patterns;
		for (const auto& entry : info.entries) {
			if (std::holds_alternative<ast::EnumMemberInfo>(entry)) {
				const auto& member = std::get<ast::EnumMemberInfo>(entry);
				if (!explicitNames.insert(member.name.text).second) {
					diags.push_back(diagnostics::ExternEnumDuplicateMember(member.span, member.name.text, enumName));
				}
				valueNameToEnums[member.name.text].insert(enumName);
			} else {
				const auto& pattern = std::get<ast::EnumPatternInfo>(entry);
				patterns.push_back(pattern.pattern);
			}
		}

		// Check wildcard overlap against explicit sibling members.
		// Full host-registry matching is deferred to Phase 4: the identifier
		// resolver calls globMatches(pattern, identifierName) at use-site rather
		// than materialising a concrete member list here.
		for (const auto& pattern : patterns) {
			for (const auto& explicitName : explicitNames) {
				if (globMatches(pattern, explicitName)) {
					diags.push_back(diagnostics::ExternEnumWildcardOverlap(info.span, enumName, pattern, explicitName));
				}
			}
		}
	}

	for (const auto& [valueName, enums] : valueNameToEnums) {
		if (enums.size() <= 1) {
			continue;
		}
		std::string enumList;
		bool first = true;
		for (const auto& enumName : enums) {
			if (!first) {
				enumList += ", ";
			}
			enumList += enumName;
			first = false;
		}

		diags.push_back(diagnostics::EnumValueNameCollision({}, valueName, enumList));
	}
}

/// Check 1: Every extend-region must target a declared region.
static void checkExtendRegionTargets(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	for (auto& [name, decls] : project.ExtendRegionDecls) {
		if (!project.RegionDecls.contains(name)) {
			for (const auto* decl : decls) {
				diags.push_back(diagnostics::UnknownExtensionTarget(decl->span, name));
			}
		}
	}
}

/// Check 2: Region data keys are unique within each region.
static void checkDuplicateRegionData(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	for (const auto& [regionName, regionDecl] : project.RegionDecls) {
		std::unordered_set<std::string> seen;
		for (const auto& entry : regionDecl->body.data) {
			if (!seen.insert(entry.key.text).second) {
				diags.push_back(diagnostics::DuplicateRegionData(entry.key.span, entry.key.text, regionName));
			}
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
					if (!set.insert(entry.name.text).second) {
						diags.push_back(diagnostics::DuplicateRegionEntry(
							entry.span, sectionKindName(section.kind), entry.name.text, regionName));
					}
				}
			}
		};

		checkEntries(regionDecl->body.sections);

		if (auto it = project.ExtendRegionDecls.find(regionName);
			it != project.ExtendRegionDecls.end()) {
			for (const auto* decl : it->second) {
				checkEntries(decl->sections);
			}
		}
	}
}

/// Check 3: Entry conditions must be Bool-compatible.
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
					diags.push_back(diagnostics::EntryConditionType(
						entry.span, sectionKindName(section.kind), entry.name.text, regionName, typeName(*condType)));
				}
			}
		}
	};

	for (auto& [name, decl] : project.RegionDecls) {
		check(decl->body.sections, name);
	}
	for (auto& [name, decls] : project.ExtendRegionDecls) {
		for (const auto* decl : decls) {
			check(decl->sections, name);
		}
	}
}

/// Check 4: Every exit must target a declared region.
static void checkExitTargets(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	auto check = [&](const std::vector<ast::Section>& sections) {
		for (const auto& section : sections) {
			if (section.kind != ast::SectionKind::Exits) continue;
			for (const auto& entry : section.entries) {
				if (!project.RegionDecls.contains(entry.name.text)) {
					diags.push_back(diagnostics::ExitTargetMissingRegion(
						entry.name.span, entry.name.text));
				}
			}
		}
	};

	for (const auto& [_, decl] : project.RegionDecls) {
		check(decl->body.sections);
	}
	for (const auto& [_, decls] : project.ExtendRegionDecls) {
		for (const auto* decl : decls) {
			check(decl->sections);
		}
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
					targets.insert(entry.name.text);
				}
			}
		}
	}
	for (auto& [regionName, decls] : project.ExtendRegionDecls) {
		if (!project.RegionDecls.contains(regionName)) continue;
		auto& targets = graph[regionName];
		for (const auto* decl : decls) {
			for (const auto& section : decl->sections) {
				if (section.kind == ast::SectionKind::Exits) {
					for (const auto& entry : section.entries) {
						targets.insert(entry.name.text);
					}
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
			diags.push_back(diagnostics::UnreachableRegion(decl->span, regionName));
		}
	}
}

/// Check 5: Every define should be referenced somewhere.
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
	for (auto& [name, decls] : project.ExtendRegionDecls) {
		for (const auto* decl : decls) {
			collectFromSections(decl->sections);
		}
	}
	for (auto& [name, decl] : project.DefineDecls) {
		collectCallNames(*decl->body, usedFunctions);
		for (const auto& param : decl->params) {
			if (param.defaultValue) {
				collectCallNames(*param.defaultValue, usedFunctions);
			}
		}
	}
	for (auto& [name, decl] : project.DefineDecls) {
		if (!usedFunctions.contains(name)) {
			diags.push_back(diagnostics::UnusedDefine(decl->span, name));
		}
	}
}

/// Check 6: Define/extern signatures must have valid parameter shapes and
/// typed defaults must match annotated parameter types.
static void checkFunctionSignatures(
	ast::Project& project, std::vector<ast::Diagnostic>& diags)
{
	auto validateParams = [&](const std::string& kind,
		bool isExtern,
		const std::string& name,
		const std::vector<ast::Param>& params,
		const ast::Span& declSpan) {
		std::unordered_set<std::string> seenNames;
		bool seenDefault = false;

		for (const auto& param : params) {
			if (!seenNames.insert(param.name.text).second) {
				diags.push_back(diagnostics::DuplicateParameter(declSpan, param.name.text, kind, name));
			}

			if (param.defaultValue) {
				seenDefault = true;
			} else if (seenDefault) {
				diags.push_back(diagnostics::RequiredAfterOptionalParameter(declSpan, param.name.text, kind, name));
			}

			if (isExtern && !param.type && !param.defaultValue) {
				diags.push_back(diagnostics::ExternParameterMissingType(declSpan, name, param.name.text));
				continue;
			}

			if (isExtern && !param.type && param.defaultValue) {
				auto defaultType = project.getType(param.defaultValue.get());
				if (!defaultType || *defaultType == ast::Type::Error) {
					diags.push_back(diagnostics::ExternParameterCannotInfer(param.defaultValue->span, name, param.name.text));
				}
				continue;
			}

			if (!param.type || !param.defaultValue) continue;

			auto paramType = project.getType(&param);
			auto defaultType = project.getType(param.defaultValue.get());
			if (!paramType || !defaultType) continue;
			if (*paramType == ast::Type::Error || *defaultType == ast::Type::Error) {
				continue;
			}
			auto isDefaultCompatible = [&](ast::Type expected, ast::Type actual) {
				if (expected == ast::Type::Condition) {
					return actual == ast::Type::Condition || isBoolCompatible(actual);
				}
				if (expected == ast::Type::Callable) {
					return actual == ast::Type::Callable || actual == ast::Type::Condition || isBoolCompatible(actual);
				}
				return actual == expected;
			};

			bool defaultCompatible = isDefaultCompatible(*paramType, *defaultType);
			const auto parameterEnum = *paramType == ast::Type::Enum
				? project.getEnumType(&param) : std::optional<std::string_view>{};
			const auto defaultEnum = *defaultType == ast::Type::Enum
				? project.getEnumType(param.defaultValue.get()) : std::optional<std::string_view>{};
			defaultCompatible = defaultCompatible || areDomainAndEnumCompatible(
				*paramType, parameterEnum, *defaultType, defaultEnum);
			if (defaultCompatible && *paramType == ast::Type::Enum
				&& *defaultType == ast::Type::Enum) {
				defaultCompatible = !parameterEnum.has_value() || !defaultEnum.has_value()
					|| *parameterEnum == *defaultEnum;
			}

			if (!defaultCompatible) {
				auto typeDisplayName = [&](ast::Type type, const auto* node) {
					if (type == ast::Type::Enum) {
						if (auto enumName = project.getEnumType(node); enumName.has_value()) {
							return std::format("enum '{}'", *enumName);
						}
					}
					return std::string(typeName(type));
				};
				diags.push_back(diagnostics::DefaultValueTypeMismatch(
					param.defaultValue->span, param.name.text, kind, name,
					typeDisplayName(*defaultType, param.defaultValue.get()),
					typeDisplayName(*paramType, &param)));
			}
		}
	};

	for (const auto& [name, decl] : project.DefineDecls) {
		validateParams("define", false, name, decl->params, decl->span);
	}
	for (const auto& [name, decl] : project.ExternDefineDecls) {
		validateParams("extern define", true, name, decl->params, decl->span);

		if (!decl->returnType) {
			diags.push_back(diagnostics::ExternMissingReturnType(decl->span, name));
			continue;
		}

		if (!resolveTypeAnnotation(project, decl->returnType->name.text)) {
			diags.push_back(diagnostics::ExternUnknownReturnType(
				decl->span, decl->returnType->name.text, name));
		}
	}
}

std::vector<ast::Diagnostic> validateDeclarations(ast::Project& project) {
	std::vector<ast::Diagnostic> diags;

	checkExtendRegionTargets(project, diags);
	checkDuplicateRegionData(project, diags);
	checkDuplicateEntries(project, diags);
	checkEntryConditionTypes(project, diags);
	checkExitTargets(project, diags);
	checkRegionReachability(project, diags);
	checkUnusedDefines(project, diags);
	checkFunctionSignatures(project, diags);
	checkEnumDeclarations(project, diags);

	return diags;
}

std::vector<CompilerDiagnostic> structureValidationDiagnostics(
	const ast::Project& project,
	const std::vector<ast::Diagnostic>& diagnostics) {
	std::vector<CompilerDiagnostic> result;
	for (const auto& diagnostic : diagnostics) {
		if (!diagnostic.code.empty()) {
			std::vector<DiagnosticRelatedLocation> related;
			if (diagnostics::IsDuplicateRegionData(diagnostic)) {
				for (const auto& [_, region] : project.RegionDecls) {
					for (size_t index = 0; index < region->body.data.size(); ++index) {
						const auto& duplicate = region->body.data[index];
						if (duplicate.key.span.file != diagnostic.span.file ||
							duplicate.key.span.start.line != diagnostic.span.start.line ||
							duplicate.key.span.start.column != diagnostic.span.start.column) {
							continue;
						}
						for (size_t prior = 0; prior < index; ++prior) {
							const auto& first = region->body.data[prior];
							if (first.key.text == duplicate.key.text) {
								related.push_back({"first definition", first.key.span});
								break;
							}
						}
					}
				}
			}
			result.push_back({diagnostic.code, diagnostic.level, diagnostic.message, diagnostic.span, std::move(related)});
		}
	}
	return result;
}

} // namespace rls::sema
