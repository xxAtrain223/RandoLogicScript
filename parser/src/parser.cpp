#include "parser.h"

#include "builder.h"
#include "grammar.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>

#include <stdexcept>

namespace rls::parser {

template <typename T>
rls::ast::File Parse(T&& in) {
	rls::ast::File file;
	file.path = std::string(in.source());

	try {
		auto root = tao::pegtl::parse_tree::parse<
			grammar::rls_file, selector
		>(in);

		if (!root) {
			file.diagnostics.push_back({
				ast::DiagnosticLevel::Error,
				"parse failed",
				ast::Span{file.path, {}, {}}
			});
			return file;
		}

		std::vector<ast::Diagnostic> buildDiags;
		file = buildFile(*root, buildDiags);
		file.diagnostics.insert(file.diagnostics.end(),
			std::make_move_iterator(buildDiags.begin()),
			std::make_move_iterator(buildDiags.end()));
	} catch (const tao::pegtl::parse_error& e) {
		ast::Span span;
		span.file = file.path;
		if (!e.positions().empty()) {
			const auto& pos = e.positions().front();
			span.start = {
				static_cast<uint32_t>(pos.line),
				static_cast<uint32_t>(pos.column)
			};
			span.end = span.start;
		}
		file.diagnostics.push_back({
			ast::DiagnosticLevel::Error,
			std::string(e.message()),
			span
		});
	}

	return file;
}

rls::ast::File ParseString(const std::string& source, const std::string& filename) {
	return Parse(tao::pegtl::memory_input(source, filename));
}

rls::ast::File ParseFile(const std::filesystem::path& filepath) {
	if (!std::filesystem::is_regular_file(filepath)) {
		throw std::runtime_error("Not a regular file: " + filepath.string());
	}

	return Parse(tao::pegtl::file_input(filepath));
}

rls::ast::Project ParseProject(const std::filesystem::path& directory) {
	if (!std::filesystem::is_directory(directory)) {
		throw std::runtime_error("Not a directory: " + directory.string());
	}

	rls::ast::Project project;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
		if (entry.is_regular_file() && entry.path().extension() == ".rls") {
			project.files.emplace_back(ParseFile(entry.path()));
		}
	}

	return project;
}

} // namespace rls::parser
