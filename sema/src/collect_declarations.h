#pragma once

#include <vector>

#include "ast.h"

namespace rls::sema {

/// Pass 1: Collect all top-level declarations from every file in the project
/// into the Project's lookup maps (RegionDecls, ExtendRegionDecls, DefineDecls,
/// ExternDefineDecls, EnemyDecls).
///
/// Reports errors for duplicate region, define, extern define, or enemy names.
/// Define and extern define share a single function namespace, so a define and
/// extern define with the same name are also reported as duplicates.
/// ExtendRegionDecls are accumulated in a multimap (duplicates are valid).
///
/// The Project's maps are cleared before collection begins, so this function
/// is idempotent.
std::vector<ast::Diagnostic> collectDeclarations(ast::Project& project);

} // namespace rls::sema
