#pragma once

#include <string>
#include <filesystem>

#include "ast.h"

namespace rls::parser {

rls::ast::File ParseString(const std::string& source, const std::string& filename = "in_memory");

rls::ast::File ParseFile(const std::filesystem::path& filepath);

rls::ast::Project ParseProject(const std::filesystem::path& directory);

} // namespace rls::parser
