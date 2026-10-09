#pragma once

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "ast.h"
#include "output.h"

#include "ap_transpiler.h"

namespace rls::transpilers::soh_ap {

// Ship of Harkinian (SoH) AP transpiler. Supplies the SoH/oot_soh world bindings
// on top of the generic ApTranspiler: enum-class prefixes, host-call rewrites
// (has/flag/trick/check_price), world special cases (wallet capacity, triforce
// hunt, spirit-shared blocks), the region/enum file scaffolding, and Python type
// names. The generic AP behavior lives entirely in the base class.
class SohApTranspiler : public ap::ApTranspiler {
public:
	explicit SohApTranspiler(const rls::ast::Project& project);

	// SoH emits region rules and the supporting enums.
	void Transpile(rls::OutputWriter& out) const override;

protected:
	std::string ruleContextParam() const override;
	std::string ruleContextOptions() const override;
	std::string renderEnumValue(std::string_view enumName, const std::string& value) const override;
	std::optional<std::string> renderHostCall(const rls::ast::CallExpr& node,
		size_t overrideIdx = std::string::npos, const rls::ast::Expr* overrideExpr = nullptr) const override;
	std::optional<std::string> renderBinarySpecialCase(const rls::ast::BinaryExpr& node) const override;
	bool isHostProvidedDefine(const std::string& name) const override;

	std::string regionsPreamble(const std::set<std::string>& usedNames) const override;
	std::set<std::string> regionsBoundNames() const override;
	std::string regionCreationArgs(const std::string& regionKey) const override;
	std::string addEventsFn() const override;
	std::string addLocationsFn() const override;
	std::string connectRegionsFn() const override;
	std::string eventEntryLine(
		const std::string& regionKey, const std::string& entryName, const std::string& rule) const override;
	std::string locationEntryLine(const std::string& entryName, const std::string& rule) const override;
	std::string exitEntryLine(const std::string& entryName, const std::string& rule) const override;
	void writeEnums(rls::OutputWriter& out) const override;
	std::string functionsPreamble(const std::set<std::string>& usedNames) const override;
	std::string pythonTypeName(
		rls::ast::Type type, std::optional<std::string_view> enumName) const override;

private:
	// The import statements a generated file needs for `usedNames`, from "from typing import" down
	// to the last module. Each name comes from the file that defines it: the enums and functions
	// this transpiler writes, the two runtime support modules, and otherwise the hard-coded host
	// module `LogicHelpers` -- so which names a file imports is derived from what it uses, and only
	// that module name is fixed here.
	std::string renderImports(const std::set<std::string>& usedNames) const;

	// Enum classes this transpiler writes into enums_gen.py, and the defines it writes into
	// functions_gen.py.
	std::set<std::string> generatedEnumNames() const;
	std::set<std::string> generatedDefineNames() const;

	// The Python enum class that values of an RLS enum belong to (e.g. Item -> "Items",
	// Check -> "Locations"), keyed by the RLS enum's name. std::nullopt for enums with no
	// dedicated class in the reference world (Scene/Dungeon/Area, which render bare). This
	// is the single source of truth shared by renderEnumValue (the value prefix) and
	// pythonTypeName (the parameter annotation), so the two cannot drift: a value
	// `Items.RG_HOOKSHOT` always annotates as `Items`.
	std::optional<std::string> enumClassName(std::string_view enumName) const;
};

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh_ap
