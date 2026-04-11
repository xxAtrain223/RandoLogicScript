#include "soh.h"

#include "generate_functions.h"

namespace rls::transpilers::soh {

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out) {
	GenerateFunctionDefinitionsHeader(project, out);
	GenerateFunctionDefinitionsSource(project, out);
}

} // namespace rls::transpilers::soh
