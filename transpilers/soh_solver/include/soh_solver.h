#pragma once

#include <string>

#include "ast.h"

namespace rls::transpilers::soh_solver {

std::string Transpile(const rls::ast::File& file);

} // namespace rls::transpilers::soh_solver
