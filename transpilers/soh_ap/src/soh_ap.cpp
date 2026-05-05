#include "soh_ap.h"

namespace rls::transpilers::soh_ap {

SohApTranspiler::SohApTranspiler(const rls::ast::Project& project)
	: project(project) {}

void SohApTranspiler::Transpile(rls::OutputWriter& out) const {
	GenerateFunctionDefinitionsSource(out);
	GenerateRegionsSource(out);
}

} // namespace rls::transpilers::soh_ap
