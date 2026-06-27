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

const std::vector<rls::ast::Diagnostic>& ApTranspiler::Diagnostics() const {
	return diagnostics;
}

void ApTranspiler::Diagnose(const rls::ast::Span& span, std::string message) const {
	diagnostics.push_back({rls::ast::DiagnosticLevel::Error, std::move(message), span});
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

bool ApTranspiler::isHostProvidedDefine(const std::string&) const {
	return false;
}

} // namespace rls::transpilers::ap
