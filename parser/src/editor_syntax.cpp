#include "editor_syntax.h"

#include "grammar.h"

#include <tao/pegtl.hpp>

#include <optional>
#include <type_traits>

namespace rls::parser {

namespace {

struct EditorSyntaxBuilder {
	const ast::SourceText& source;
	std::string_view filename;
	EditorSyntax result;
	std::optional<ast::Name> memberObject;
	std::optional<size_t> memberAccessIndex;
	std::optional<size_t> typePositionIndex;

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

	void beginTypePosition(
		const ast::Span& delimiter, EditorTypePositionKind kind) {
		size_t endOffset = source.content().size();
		if (const auto offset = source.byteOffsetFromUtf8Position(delimiter.end)) {
			endOffset = *offset;
			while (endOffset < source.content().size()
				&& (source.content()[endOffset] == ' '
					|| source.content()[endOffset] == '\t')) {
				++endOffset;
			}
		}
		const auto end = source.utf8PositionAtByteOffset(endOffset);
		if (!end) return;
		result.typePositions.push_back({
			kind,
			ast::Span{std::string(filename), delimiter.end, *end},
			SyntaxRecoveryStatus::Recovered});
		typePositionIndex = result.typePositions.size() - 1;
	}

	void completeTypePosition(const ast::Span& name) {
		if (!typePositionIndex) return;
		auto& position = result.typePositions[*typePositionIndex];
		position.span.end = name.end;
		position.status = SyntaxRecoveryStatus::Complete;
		typePositionIndex.reset();
	}
};

template<typename Rule>
struct editor_action : tao::pegtl::nothing<Rule> {};

template<>
struct editor_action<grammar::enum_name> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.result.enumDeclarations.push_back({
				ast::Name(input.string(), *span), *span,
				SyntaxRecoveryStatus::Recovered});
		}
	}
};

template<>
struct editor_action<grammar::enum_head> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (!builder.result.enumDeclarations.empty()) {
			if (const auto span = builder.spanFor(input)) {
				builder.result.enumDeclarations.back().span = *span;
			}
		}
	}
};

template<>
struct editor_action<grammar::member_object> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		builder.memberAccessIndex.reset();
		if (const auto span = builder.spanFor(input)) {
			builder.memberObject = ast::Name(input.string(), *span);
		} else {
			builder.memberObject.reset();
		}
	}
};

template<>
struct editor_action<grammar::member_delimiter> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		const auto delimiter = builder.spanFor(input);
		if (!delimiter || !builder.memberObject) return;
		builder.result.memberAccesses.push_back({
			*builder.memberObject,
			ast::Span{std::string(builder.filename),
				builder.memberObject->span.start, delimiter->end},
			ast::Span{std::string(builder.filename), delimiter->end, delimiter->end},
			SyntaxRecoveryStatus::Recovered});
		builder.memberAccessIndex = builder.result.memberAccesses.size() - 1;
	}
};

template<>
struct editor_action<grammar::member_name> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		const auto span = builder.spanFor(input);
		if (!span || !builder.memberAccessIndex) return;
		auto& access = builder.result.memberAccesses[*builder.memberAccessIndex];
		access.span.end = span->end;
		access.memberSpan = *span;
		access.status = SyntaxRecoveryStatus::Complete;
		builder.memberObject.reset();
		builder.memberAccessIndex.reset();
	}
};

template<>
struct editor_action<grammar::parameter_type_delimiter> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.beginTypePosition(
				*span, EditorTypePositionKind::Parameter);
		}
	}
};

template<>
struct editor_action<grammar::parameter_type_name> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.completeTypePosition(*span);
		}
	}
};

template<>
struct editor_action<grammar::return_type_delimiter> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.beginTypePosition(
				*span, EditorTypePositionKind::Return);
		}
	}
};

template<>
struct editor_action<grammar::return_type_name> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.completeTypePosition(*span);
		}
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
	EditorSyntaxBuilder builder{
		source, filename, {}, std::nullopt, std::nullopt, std::nullopt};
	tao::pegtl::memory_input input(source.content(), filename);
	grammar::ParseState state{true};
	tao::pegtl::parse<grammar::rls_file, editor_action>(
		input, builder, state);
	classifyCompleteDeclarations(builder.result, parsedFile);
	return std::move(builder.result);
}

} // namespace rls::parser