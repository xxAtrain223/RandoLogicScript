#include "soh.h"

#include "generate_enemies.h"
#include "generate_functions.h"
#include "generate_regions.h"

namespace rls::transpilers::soh {

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out) {
	GenerateFunctionDefinitionsHeader(project, out);
	GenerateFunctionDefinitionsSource(project, out);
	GenerateEnemiesHeader(project, out);
	GenerateEnemiesSource(project, out);
	GenerateRegionsHeader(project, out);
	GenerateRegionsSource(project, out);
}

} // namespace rls::transpilers::soh
