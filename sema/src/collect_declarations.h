#pragma once

#include <vector>

#include "ast.h"
#include "diagnostic.h"

namespace rls::sema {

/// Pass 1: Collect all top-level declarations from every file in the project
/// into the Project's lookup maps (RegionDecls, ExtendRegionDecls, DefineDecls,
/// EnemyDecls).
///
/// Reports errors for duplicate region, define, or enemy names.
/// ExtendRegionDecls are accumulated in a multimap (duplicates are valid).
///
/// The Project's maps are cleared before collection begins, so this function
/// is idempotent.
std::vector<ast::Diagnostic> collectDeclarations(ast::Project& project);

} // namespace rls::sema
