#pragma once

#include <string>

#include "ast.h"

namespace rls::parser {

rls::ast::File Parse(const std::string& source);

} // namespace rls::parser
