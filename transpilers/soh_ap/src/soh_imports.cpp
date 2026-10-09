#include "soh_ap.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <vector>

namespace rls::transpilers::soh_ap {

namespace {

std::string lowered(const std::string& s) {
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
	return out;
}

// isort's default member order, which the project's linter enforces: CONSTANTS, then Classes, then
// everything else, each group case-insensitively.
int memberRank(const std::string& name) {
	const bool allUpper = std::none_of(name.begin(), name.end(), [](unsigned char c) { return std::islower(c) != 0; });
	if (allUpper && name.size() > 1) {
		return 0;
	}
	return std::isupper(static_cast<unsigned char>(name[0])) != 0 ? 1 : 2;
}

// One `from <module> import ...` statement: a single name on the line, several one per line.
std::string importStatement(const std::string& module, const std::set<std::string>& names) {
	std::vector<std::string> sorted(names.begin(), names.end());
	std::sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
		return std::make_pair(memberRank(a), lowered(a)) < std::make_pair(memberRank(b), lowered(b));
	});
	std::ostringstream out;
	out << "from " << module << " import ";
	if (sorted.size() == 1) {
		out << sorted.front() << "\n";
		return out.str();
	}
	out << "(\n";
	for (const auto& name : sorted) {
		out << "    " << name << ",\n";
	}
	out << ")\n";
	return out.str();
}

} // namespace

std::set<std::string> SohApTranspiler::generatedEnumNames() const {
	// writeEnums: the classes built from the region walk, then every enum declared in RLS.
	std::set<std::string> names{"Regions", "Events", "EventLocations"};
	for (const auto& [enumName, info] : project.EnumInfos) {
		if (info.kind == rls::ast::EnumKind::Normal) {
			names.insert(enumName);
		}
	}
	return names;
}

std::set<std::string> SohApTranspiler::generatedDefineNames() const {
	std::set<std::string> names;
	for (const auto& [name, decl] : project.DefineDecls) {
		if (!isHostProvidedDefine(name)) {
			names.insert(name);
		}
	}
	return names;
}

std::string SohApTranspiler::renderImports(const std::set<std::string>& usedNames) const {
	const std::set<std::string> enumNames = generatedEnumNames();
	const std::set<std::string> defineNames = generatedDefineNames();

	// Everything not provided by one of this transpiler's own files or the runtime support modules
	// is the world's: the host rules, option classes and Ship enums behind LogicHelpers.
	std::map<std::string, std::set<std::string>> byModule;
	for (const auto& name : usedNames) {
		if (name == "Callable") {
			continue;  // typing, below
		}
		if (name == "rls_match_rule" || name == "rls_match_value") {
			byModule[".rls_match"].insert(name);
		} else if (name == "rls_conditional") {
			byModule[".rls_conditional"].insert(name);
		} else if (enumNames.count(name) != 0) {
			byModule[".enums_gen"].insert(name);
		} else if (defineNames.count(name) != 0) {
			byModule[".functions_gen"].insert(name);
		} else {
			byModule[".LogicHelpers"].insert(name);
		}
	}

	std::ostringstream out;
	out << "from typing import TYPE_CHECKING" << (usedNames.count("Callable") != 0 ? ", Callable" : "") << "\n\n";
	for (const char* module : {".enums_gen", ".functions_gen", ".LogicHelpers", ".rls_conditional", ".rls_match"}) {
		if (const auto it = byModule.find(module); it != byModule.end()) {
			out << importStatement(module, it->second);
		}
	}
	return out.str();
}

} // namespace rls::transpilers::soh_ap
