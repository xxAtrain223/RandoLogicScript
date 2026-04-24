#include "soh.h"

namespace rls::transpilers::soh {

SohTranspiler::SohTranspiler(const rls::ast::Project& project)
	: project(project) {}

void SohTranspiler::Transpile(rls::OutputWriter& out) const {
	GenerateRuntimeHeaders(out);
	GenerateFunctionDefinitionsHeader(out);
	GenerateFunctionDefinitionsSource(out);
	GenerateEnemiesHeader(out);
	GenerateEnemiesSource(out);
	GenerateRegionsHeader(out);
	GenerateRegionsSource(out);
}

} // namespace rls::transpilers::soh
