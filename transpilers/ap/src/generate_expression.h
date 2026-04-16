#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::ap {

std::string GenerateExpression(const rls::ast::ExprPtr& expr);

}

