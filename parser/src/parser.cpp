#include "parser.h"

#include "builder.h"
#include "editor_syntax.h"
#include "grammar.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/must_if.hpp>

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace rls::parser {

// == Custom error messages for must<> failures ================================

struct parse_errors {
	template<typename>
	static constexpr const char* message = nullptr;

	// Never auto-raise on normal failure — only raise inside must<>.
	template<typename>
	static constexpr bool raise_on_failure = false;
};

// -- Punctuation --------------------------------------------------------------
template<> constexpr const char* parse_errors::message<grammar::open_paren>  = "expected '('";
template<> constexpr const char* parse_errors::message<grammar::close_paren> = "expected ')'";
template<> constexpr const char* parse_errors::message<grammar::call_close_paren> = "expected ')'";
template<> constexpr const char* parse_errors::message<grammar::open_brace>  = "expected '{'";
template<> constexpr const char* parse_errors::message<grammar::close_brace> = "expected '}'";
template<> constexpr const char* parse_errors::message<grammar::section_open_brace> = "expected '{'";
template<> constexpr const char* parse_errors::message<grammar::section_close_brace> = "expected '}'";
template<> constexpr const char* parse_errors::message<grammar::region_open_brace> = "expected '{'";
template<> constexpr const char* parse_errors::message<grammar::region_close_brace> = "expected '}'";
template<> constexpr const char* parse_errors::message<grammar::colon>       = "expected ':'";
template<> constexpr const char* parse_errors::message<grammar::entry_delimiter> = "expected ':'";
template<> constexpr const char* parse_errors::message<grammar::region_data_delimiter> = "expected ':'";

// -- Tokens -------------------------------------------------------------------
template<> constexpr const char* parse_errors::message<grammar::ident>          = "expected identifier";
template<> constexpr const char* parse_errors::message<grammar::enum_name>      = "expected identifier";
template<> constexpr const char* parse_errors::message<grammar::region_name>    = "expected identifier";
template<> constexpr const char* parse_errors::message<grammar::expr>           = "expected expression";
template<> constexpr const char* parse_errors::message<grammar::ternary>        = "expected expression";
template<> constexpr const char* parse_errors::message<grammar::match_ternary>  = "expected expression";

// -- Keywords -----------------------------------------------------------------
template<> constexpr const char* parse_errors::message<grammar::kw<grammar::kw_region>> = "expected 'region'";
template<> constexpr const char* parse_errors::message<grammar::kw<grammar::kw_enum>> = "expected 'enum'";

// -- Structural ---------------------------------------------------------------
template<> constexpr const char* parse_errors::message<grammar::region_body> = "expected region data or section";
template<> constexpr const char* parse_errors::message<grammar::no_trailing_or> = "trailing 'or' without a following match arm";

// -- Top-level ----------------------------------------------------------------
template<> constexpr const char* parse_errors::message<tao::pegtl::eof> = "expected declaration or end of file";

/// Control class: uses custom messages when available, falls back to default.
template<typename Rule>
using rls_control = tao::pegtl::must_if<parse_errors, tao::pegtl::normal, false>::control<Rule>;

// =============================================================================

template <typename T>
rls::ast::File Parse(T&& in, [[maybe_unused]] ParseMode mode) {
	rls::ast::File file;
	file.path = std::string(in.source());

	try {
		auto root = tao::pegtl::parse_tree::parse<
			grammar::rls_file, selector, tao::pegtl::nothing, rls_control
		>(in);

		if (!root) {
			file.diagnostics.push_back(ast::Diagnostic{
				"", ast::Span{file.path, {}, {}}, ast::DiagnosticLevel::Error, "parse failed"});
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
		file.diagnostics.push_back(ast::Diagnostic{
			"", span, ast::DiagnosticLevel::Error, std::string(e.message())});
	}

	return file;
}

rls::ast::File ParseString(
	const std::string& source, const std::string& filename, ParseMode mode) {
	return Parse(tao::pegtl::memory_input(source, filename), mode);
}

rls::ast::File ParseFile(const std::filesystem::path& filepath, ParseMode mode) {
	if (!std::filesystem::is_regular_file(filepath)) {
		throw std::runtime_error("Not a regular file: " + filepath.string());
	}

	return Parse(tao::pegtl::file_input(filepath), mode);
}

rls::ast::Project ParseProject(
	const std::filesystem::path& directory, ParseMode mode) {
	if (!std::filesystem::is_directory(directory)) {
		throw std::runtime_error("Not a directory: " + directory.string());
	}

	rls::ast::Project project;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
		if (entry.is_regular_file() && entry.path().extension() == ".rls") {
			project.files.emplace_back(ParseFile(entry.path(), mode));
		}
	}

	return project;
}

void RecoverCompleteDeclarations(
	ast::File& file, const std::string& source,
	const ast::SourceText& sourceText, const EditorSyntax& syntax) {
	if (file.diagnostics.empty()) return;

	for (const auto& candidate : syntax.declarations) {
		const auto start = sourceText.byteOffsetFromUtf8Position(candidate.span.start);
		const auto end = sourceText.byteOffsetFromUtf8Position(candidate.span.end);
		if (!start || !end || *start >= *end) continue;

		try {
			tao::pegtl::memory_input input(
				source.data() + *start, source.data() + *end, file.path,
				*start, candidate.span.start.line, candidate.span.start.column);
			auto root = tao::pegtl::parse_tree::parse<
				grammar::rls_file, selector, tao::pegtl::nothing, rls_control
			>(input);
			if (!root) continue;

			std::vector<ast::Diagnostic> diagnostics;
			auto candidateFile = buildFile(*root, diagnostics);
			if (!diagnostics.empty() || candidateFile.declarations.size() != 1) continue;
			file.declarations.push_back(std::move(candidateFile.declarations.front()));
		} catch (const tao::pegtl::parse_error&) {
			// Recovered syntax is not semantic syntax unless strict parsing succeeds.
		}
	}

	std::sort(file.declarations.begin(), file.declarations.end(),
		[](const ast::Decl& left, const ast::Decl& right) {
			const auto leftSpan = std::visit(
				[](const auto& node) { return node.span; }, left);
			const auto rightSpan = std::visit(
				[](const auto& node) { return node.span; }, right);
			if (leftSpan.start.line != rightSpan.start.line) {
				return leftSpan.start.line < rightSpan.start.line;
			}
			return leftSpan.start.column < rightSpan.start.column;
		});
}

IndexedFile ParseStringWithIndex(
	const std::string& source, const std::string& filename, ParseMode mode) {
	auto file = ParseString(source, filename, mode);
	const auto sourceText = ast::SourceText::FromUtf8(source);
	std::optional<EditorSyntax> editorSyntax;
	if (mode == ParseMode::Editor && sourceText) {
		editorSyntax = ParseEditorSyntax(*sourceText, filename, file);
		RecoverCompleteDeclarations(file, source, *sourceText, *editorSyntax);
		ClassifyEditorSyntax(*editorSyntax, file);
	}
	auto sourceIndex = BuildSourceIndex(file, sourceText ? &*sourceText : nullptr);
	if (editorSyntax) {
		for (const auto& declaration : editorSyntax->enumDeclarations) {
			sourceIndex.addEnumName(declaration.name.text);
		}
		for (const auto& memberAccess : editorSyntax->memberAccesses) {
			sourceIndex.addMemberAccess({
				memberAccess.object.text, memberAccess.memberSpan});
		}
		for (const auto& typePosition : editorSyntax->typePositions) {
			sourceIndex.addTypePosition({typePosition.span});
		}
		for (const auto& target : editorSyntax->extensionTargets) {
			sourceIndex.addExtensionTarget({target});
		}
		for (const auto& call : editorSyntax->calls) {
			std::vector<std::optional<std::string>> labels;
			labels.reserve(call.arguments.size());
			for (const auto& argument : call.arguments) {
				labels.push_back(argument.label
					? std::optional<std::string>(argument.label->text)
					: std::nullopt);
			}
			if (call.status == SyntaxRecoveryStatus::Recovered) {
				CallContext context{
					call.callee.text, call.span, call.callee.span,
					{}, {}, {}, std::nullopt};
				for (const auto& argument : call.arguments) {
					context.argumentRanges.push_back(argument.valueSpan);
					context.argumentLabels.push_back(argument.label
						? std::optional<ast::Span>(argument.label->span)
						: std::nullopt);
					context.argumentLabelNames.push_back(argument.label
						? std::optional<std::string>(argument.label->text)
						: std::nullopt);
				}
				sourceIndex.addCall(std::move(context));
			}
			for (size_t index = 0; index < call.arguments.size(); ++index) {
				const auto& argument = call.arguments[index];
				sourceIndex.addCallArgument({
					call.callee.text, labels, index, argument.valueSpan});
				if (argument.label || argument.labelCandidate) {
					sourceIndex.addNamedArgument({
						call.callee.text, labels, index, argument.labelSpan});
				}
			}
		}
		for (const auto& region : editorSyntax->regions) {
			for (const auto& section : region.sections) {
				for (const auto& entry : section.entries) {
					sourceIndex.addSectionEntry({section.kind, entry.labelSpan});
				}
			}
			if (region.status == SyntaxRecoveryStatus::Complete) continue;
			RegionContext context{
				.span = region.span,
				.name = region.name.text,
				.extension = region.extension,
				.dataKeys = region.dataKeys,
			};
			std::vector<RegionSectionContext> sections;
			for (const auto& section : region.sections) {
				context.sectionKinds.push_back(section.kind);
				RegionSectionContext sectionContext{
					section.kind, section.span, {}};
				for (const auto& entry : section.entries) {
					if (entry.name) {
						sectionContext.entryNames.push_back(entry.name->text);
					}
				}
				sections.push_back(std::move(sectionContext));
			}
			sourceIndex.addRegionContext(
				std::move(context), std::move(sections));
		}
	}
	return {std::move(file), std::move(sourceIndex)};
}

IndexedFile ParseFileWithIndex(
	const std::filesystem::path& filepath, ParseMode mode) {
	auto file = ParseFile(filepath, mode);
	auto sourceIndex = BuildSourceIndex(file);
	return {std::move(file), std::move(sourceIndex)};
}

} // namespace rls::parser
