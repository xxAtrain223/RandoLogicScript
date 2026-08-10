#pragma once

#include <string>
#include <filesystem>

#include "ast.h"
#include "source_index.h"

namespace rls::parser {

rls::ast::File ParseString(const std::string& source, const std::string& filename = "in_memory");

rls::ast::File ParseFile(const std::filesystem::path& filepath);

rls::ast::Project ParseProject(const std::filesystem::path& directory);

struct IndexedFile {
	rls::ast::File file;
	SourceIndex sourceIndex;
};

IndexedFile ParseStringWithIndex(const std::string& source, const std::string& filename = "in_memory");

IndexedFile ParseFileWithIndex(const std::filesystem::path& filepath);

} // namespace rls::parser
