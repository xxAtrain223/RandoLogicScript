#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh {

void GenerateFunctionDefinitionsHeader(const rls::ast::Project& project, rls::OutputWriter& out);
void GenerateFunctionDefinitionsSource(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh