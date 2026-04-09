#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "ast.h"

namespace rls::sema {

/// Map an identifier name to its type based on its enum prefix.
/// Returns nullopt if the name has no recognized prefix (e.g. a parameter name).
std::optional<ast::Type> typeFromIdentifier(std::string_view name);

/// Parse a type annotation string (e.g. "Distance") to a Type enum value.
/// Returns nullopt if the annotation is not a recognized type name.
std::optional<ast::Type> typeFromAnnotation(std::string_view annotation);

/// Pass 2: Resolve and check types for all expressions in the project.
/// Populates Project::TypeTable via setType().
///
/// Currently handles:
///   - Enum-prefixed identifiers (Step 1)
///   - Host function call validation (Step 2)
///   - Bottom-up expression typing for all node types (Step 3)
///   - Parameter scope for define and enemy field bodies (Step 4)
std::vector<ast::Diagnostic> resolveTypes(ast::Project& project);

} // namespace rls::sema
