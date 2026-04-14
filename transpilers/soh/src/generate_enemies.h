#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh {

void GenerateEnemiesHeader(const rls::ast::Project& project, rls::OutputWriter& out);
void GenerateEnemiesSource(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh
