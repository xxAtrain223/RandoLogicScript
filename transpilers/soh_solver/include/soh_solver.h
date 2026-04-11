#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh_solver {

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh_solver
