#include "ap_transpiler.h"

namespace rls::transpilers::ap {

ApTranspiler::ApTranspiler(const rls::ast::Project& project)
	: project(project) {}

void ApTranspiler::GenerateEnumsSource(rls::OutputWriter& out) const {
	writeEnums(out);
}

void ApTranspiler::SetCurrentLocation(std::optional<std::string> location) const {
	currentLocationName = std::move(location);
}

// == Default hook implementations =============================================
// Generic AP behavior; SoH (and other games) override as needed.

std::string ApTranspiler::ruleContextParam() const {
	return "";
}

std::string ApTranspiler::renderEnumValue(rls::ast::Type, const std::string& name) const {
	return name;
}

std::optional<std::string> ApTranspiler::renderHostCall(const rls::ast::CallExpr&) const {
	return std::nullopt;
}

std::optional<std::string> ApTranspiler::renderBinarySpecialCase(const rls::ast::BinaryExpr&) const {
	return std::nullopt;
}

std::string ApTranspiler::renderSharedBlock(const rls::ast::SharedBlock&) const {
	return "";
}

} // namespace rls::transpilers::ap
