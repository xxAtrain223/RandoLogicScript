#pragma once

#include <vector>

#include "ast.h"

namespace rls::sema {

/// Pass 3: Validate declarations after types are resolved.
///
/// Checks:
///   - extend region targets must reference a declared region
///   - every enemy must have a 'kill' field
///   - no duplicate entries within the same region and section kind
///   - entry conditions must be Bool-compatible
///   - every region must be reachable from RR_ROOT via exits
///   - every define should be referenced somewhere (info)
///   - define/extern define signatures must have valid parameter/default shapes
std::vector<ast::Diagnostic> validateDeclarations(ast::Project& project);

} // namespace rls::sema
