#pragma once

#include <string>

#include "ast.h"

namespace rls::parser {

rls::AstNode Parse(const std::string& source);

} // namespace rls::parser
