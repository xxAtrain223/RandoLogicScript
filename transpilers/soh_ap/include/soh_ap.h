#pragma once

#include <optional>
#include <string>

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
	std::string renderEnumValue(rls::ast::Type type, const std::string& name) const override;
	std::optional<std::string> renderHostCall(const rls::ast::CallExpr& node) const override;
	std::optional<std::string> renderBinarySpecialCase(const rls::ast::BinaryExpr& node) const override;
	std::string renderSharedBlock(const rls::ast::SharedBlock& node) const override;
	std::string renderAnyAgeBlock(const rls::ast::AnyAgeBlock& node) const override;

	std::string regionsPreamble() const override;
	std::string regionCreationArgs(const std::string& regionKey) const override;
	std::string addEventsFn() const override;
	std::string addLocationsFn() const override;
	std::string connectRegionsFn() const override;
	std::string eventEntryLine(
		const std::string& regionKey, const std::string& entryName, const std::string& rule) const override;
	std::string locationEntryLine(const std::string& entryName, const std::string& rule) const override;
	std::string exitEntryLine(const std::string& entryName, const std::string& rule) const override;
	void writeEnums(rls::OutputWriter& out) const override;
	std::string functionsPreamble() const override;
	std::string pythonTypeName(rls::ast::Type type) const override;
};

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh_ap
