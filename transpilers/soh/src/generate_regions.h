#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh {

void GenerateRegionsHeader(const rls::ast::Project& project, rls::OutputWriter& out);
void GenerateRegionsSource(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh