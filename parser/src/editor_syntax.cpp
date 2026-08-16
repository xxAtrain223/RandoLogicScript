#include "editor_syntax.h"

#include "grammar.h"

#include <tao/pegtl.hpp>

#include <optional>
#include <type_traits>

namespace rls::parser {

namespace {

namespace editor_grammar {

using namespace tao::pegtl;

struct string_escape : seq<one<'\\'>, opt<any>> {};
struct string_character : sor<string_escape, not_one<'"'>> {};
struct string_literal : seq<one<'"'>, star<string_character>, opt<one<'"'>>> {};

struct enum_name : grammar::ident {};
struct enum_head : seq<grammar::kw<grammar::kw_enum>, grammar::_, enum_name> {};

struct member_object : grammar::ident {};
struct member_name : grammar::ident {};
struct member_access : seq<member_object, grammar::dot, opt<member_name>> {};

struct file : seq<
	star<sor<
		grammar::line_comment,
		string_literal,
		enum_head,
		member_access,
		grammar::ident,
		any
	>>,
	eof
> {};

} // namespace editor_grammar

struct EditorSyntaxBuilder {
	const ast::SourceText& source;
	std::string_view filename;
	EditorSyntax result;
	std::optional<ast::Name> memberObject;
	std::optional<ast::Span> memberNameSpan;

	template<typename Input>
	std::optional<ast::Span> spanFor(const Input& input) const {
		const auto* sourceBegin = source.content().data();
		const auto* inputBegin = input.begin();
		if (inputBegin < sourceBegin || inputBegin > sourceBegin + source.content().size()) {
			return std::nullopt;
		}
		const size_t startOffset = static_cast<size_t>(inputBegin - sourceBegin);
		const auto start = source.utf8PositionAtByteOffset(startOffset);
		const auto end = source.utf8PositionAtByteOffset(startOffset + input.size());
		if (!start || !end) return std::nullopt;
		return ast::Span{std::string(filename), *start, *end};
	}
};

template<typename Rule>
struct editor_action : tao::pegtl::nothing<Rule> {};

template<>
struct editor_action<editor_grammar::enum_name> {
	template<typename Input>
	static void apply(const Input& input, EditorSyntaxBuilder& builder) {
		if (const auto span = builder.spanFor(input)) {
			builder.result.enumDeclarations.push_back({
				ast::Name(input.string(), *span), *span,
				SyntaxRecoveryStatus::Recovered});
		}
	}
};

template<>
struct editor_action<editor_grammar::enum_head> {
	template<typename Input>
	static void apply(const Input& input, EditorSyntaxBuilder& builder) {
		if (!builder.result.enumDeclarations.empty()) {
			if (const auto span = builder.spanFor(input)) {
				builder.result.enumDeclarations.back().span = *span;
			}
		}
	}
};

template<>
struct editor_action<editor_grammar::member_object> {
	template<typename Input>
	static void apply(const Input& input, EditorSyntaxBuilder& builder) {
		builder.memberNameSpan.reset();
		if (const auto span = builder.spanFor(input)) {
			builder.memberObject = ast::Name(input.string(), *span);
		} else {
			builder.memberObject.reset();
		}
	}
};

template<>
struct editor_action<editor_grammar::member_name> {
	template<typename Input>
	static void apply(const Input& input, EditorSyntaxBuilder& builder) {
		builder.memberNameSpan = builder.spanFor(input);
	}
};

template<>
struct editor_action<editor_grammar::member_access> {
	template<typename Input>
	static void apply(const Input& input, EditorSyntaxBuilder& builder) {
		const auto span = builder.spanFor(input);
		if (!span || !builder.memberObject) return;
		const ast::Span memberSpan = builder.memberNameSpan.value_or(
			ast::Span{std::string(builder.filename), span->end, span->end});
		builder.result.memberAccesses.push_back({
			std::move(*builder.memberObject), *span, memberSpan,
			builder.memberNameSpan
				? SyntaxRecoveryStatus::Complete
				: SyntaxRecoveryStatus::Recovered});
		builder.memberObject.reset();
		builder.memberNameSpan.reset();
	}
};

bool sameSpan(const ast::Span& left, const ast::Span& right) {
	return left.file == right.file
		&& left.start.line == right.start.line
		&& left.start.column == right.start.column
		&& left.end.line == right.end.line
		&& left.end.column == right.end.column;
}

void classifyCompleteDeclarations(EditorSyntax& syntax, const ast::File& file) {
	for (auto& candidate : syntax.enumDeclarations) {
		for (const auto& declaration : file.declarations) {
			const bool complete = std::visit([&](const auto& node) {
				using T = std::decay_t<decltype(node)>;
				if constexpr (std::is_same_v<T, ast::EnumDecl>
					|| std::is_same_v<T, ast::ExternEnumDecl>) {
					return candidate.name.text == node.name.text
						&& sameSpan(candidate.name.span, node.name.span);
				}
				return false;
			}, declaration);
			if (complete) {
				candidate.status = SyntaxRecoveryStatus::Complete;
				break;
			}
		}
	}
}

} // namespace

EditorSyntax ParseEditorSyntax(
	const ast::SourceText& source, std::string_view filename,
	const ast::File& parsedFile) {
	EditorSyntaxBuilder builder{source, filename, {}, std::nullopt, std::nullopt};
	tao::pegtl::memory_input input(source.content(), filename);
	tao::pegtl::parse<editor_grammar::file, editor_action>(input, builder);
	classifyCompleteDeclarations(builder.result, parsedFile);
	return std::move(builder.result);
}

} // namespace rls::parser