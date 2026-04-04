#pragma once

#include <vector>

#include "ast.h"
#include "diagnostic.h"

namespace rls::sema {

/// Run all semantic analysis passes on the project.
///
/// Currently performs:
///   Pass 1 - Collect all top-level declarations into the Project's lookup maps
///            (RegionDecls, ExtendRegionDecls, DefineDecls, EnemyDecls).
///
/// Returns all diagnostics (errors and warnings) accumulated across passes.
/// The Project is modified in-place to populate its side tables.
std::vector<ast::Diagnostic> analyze(ast::Project& project);

} // namespace rls::sema
