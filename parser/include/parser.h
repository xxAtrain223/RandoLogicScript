#pragma once

#include <string>
#include <filesystem>

#include "ast.h"
#include "source_index.h"

namespace rls::parser {

enum class ParseMode {
	Strict,
	Editor,
};

rls::ast::File ParseString(
	const std::string& source, const std::string& filename = "in_memory",
	ParseMode mode = ParseMode::Strict);

rls::ast::File ParseFile(
	const std::filesystem::path& filepath, ParseMode mode = ParseMode::Strict);

rls::ast::Project ParseProject(
	const std::filesystem::path& directory, ParseMode mode = ParseMode::Strict);

struct IndexedFile {
	rls::ast::File file;
	SourceIndex sourceIndex;
};

IndexedFile ParseStringWithIndex(
	const std::string& source, const std::string& filename = "in_memory",
	ParseMode mode = ParseMode::Strict);

IndexedFile ParseFileWithIndex(
	const std::filesystem::path& filepath, ParseMode mode = ParseMode::Strict);

} // namespace rls::parser
