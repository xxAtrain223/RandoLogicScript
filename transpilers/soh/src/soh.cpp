#include "soh.h"

namespace rls::transpilers::soh {

namespace {

bool hasErrors(const std::vector<rls::ast::Diagnostic>& diagnostics) {
	for (const auto& diagnostic : diagnostics) {
		if (diagnostic.level == rls::ast::DiagnosticLevel::Error) {
			return true;
		}
	}
	return false;
}

} // namespace

SohTranspiler::SohTranspiler(const rls::ast::Project& project)
	: project(project) {}

std::vector<rls::ast::Diagnostic> SohTranspiler::Transpile(rls::OutputWriter& out) const {
	auto diagnostics = Validate();
	if (hasErrors(diagnostics)) {
		return diagnostics;
	}

	GenerateFunctionDefinitionsHeader(out);
	GenerateFunctionDefinitionsSource(out);
	GenerateRegionsHeader(out);
	WriteRegionsSource(out);
	return diagnostics;
}

std::vector<rls::ast::Diagnostic> Transpile(
	const rls::ast::Project& project, rls::OutputWriter& out)
{
	return SohTranspiler(project).Transpile(out);
}

} // namespace rls::transpilers::soh
