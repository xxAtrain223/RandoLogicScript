#include "editor_syntax.h"

#include "grammar.h"

#include <tao/pegtl.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <type_traits>

namespace rls::parser {

namespace {

struct EditorSyntaxBuilder {
	struct CallFrame {
		ast::Name callee;
		size_t argumentStart = 0;
		std::optional<ast::Name> pendingLabel;
		std::optional<ast::Name> label;
		std::optional<size_t> labelDelimiterEnd;
		std::vector<EditorCallArgument> arguments;
		bool closed = false;
	};

	const ast::SourceText& source;
	std::string_view filename;
	EditorSyntax result;
	std::optional<ast::Name> memberObject;
	std::optional<size_t> memberAccessIndex;
	std::optional<size_t> typePositionIndex;
	std::optional<ast::Name> callCallee;
	std::vector<CallFrame> callFrames;

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

	std::optional<size_t> offsetFor(ast::Position position) const {
		return source.byteOffsetFromUtf8Position(position);
	}

	std::optional<ast::Span> spanFromOffsets(size_t start, size_t end) const {
		const auto startPosition = source.utf8PositionAtByteOffset(start);
		const auto endPosition = source.utf8PositionAtByteOffset(end);
		if (!startPosition || !endPosition) return std::nullopt;
		return ast::Span{std::string(filename), *startPosition, *endPosition};
	}

	void finishArgument(CallFrame& frame, size_t end, bool allowEmpty) {
		if (end < frame.argumentStart) return;
		size_t contentStart = frame.argumentStart;
		while (contentStart < end
			&& std::isspace(static_cast<unsigned char>(
				source.content()[contentStart]))) {
			++contentStart;
		}
		size_t contentEnd = end;
		while (contentEnd > contentStart
			&& std::isspace(static_cast<unsigned char>(
				source.content()[contentEnd - 1]))) {
			--contentEnd;
		}
		if (!allowEmpty && contentStart == contentEnd && !frame.label) return;

		size_t valueStart = frame.labelDelimiterEnd.value_or(contentStart);
		while (valueStart < contentEnd
			&& std::isspace(static_cast<unsigned char>(
				source.content()[valueStart]))) {
			++valueStart;
		}
		const auto valueSpan = spanFromOffsets(valueStart, contentEnd);
		if (!valueSpan) return;

		ast::Span labelSpan;
		bool labelCandidate = false;
		if (frame.label) {
			labelSpan = frame.label->span;
		} else {
			size_t candidateEnd = contentStart;
			if (candidateEnd < contentEnd
				&& (std::isalpha(static_cast<unsigned char>(
					source.content()[candidateEnd]))
					|| source.content()[candidateEnd] == '_')) {
				++candidateEnd;
				while (candidateEnd < contentEnd
					&& (std::isalnum(static_cast<unsigned char>(
						source.content()[candidateEnd]))
						|| source.content()[candidateEnd] == '_')) {
					++candidateEnd;
				}
			}
			labelCandidate = candidateEnd == contentEnd;
			if (labelCandidate) {
				if (const auto candidate = spanFromOffsets(contentStart, candidateEnd)) {
					labelSpan = *candidate;
				}
			}
		}

		frame.arguments.push_back({
			frame.label, labelSpan, *valueSpan, labelCandidate});
		frame.pendingLabel.reset();
		frame.label.reset();
		frame.labelDelimiterEnd.reset();
	}

	void finishCall(const ast::Span& span) {
		if (callFrames.empty()) return;
		auto frame = std::move(callFrames.back());
		callFrames.pop_back();
		if (!frame.closed) {
			if (const auto end = offsetFor(span.end)) {
				finishArgument(frame, *end, true);
			}
		}
		result.calls.push_back({
			std::move(frame.callee), span, std::move(frame.arguments),
			frame.closed
				? SyntaxRecoveryStatus::Complete
				: SyntaxRecoveryStatus::Recovered});
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

template<>
struct editor_action<grammar::call_callee> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.callCallee = ast::Name(input.string(), *span);
		}
	}
};

template<>
struct editor_action<grammar::call_open_paren> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		const auto span = builder.spanFor(input);
		if (!span || !builder.callCallee) return;
		const auto start = builder.offsetFor(span->end);
		if (!start) return;
		builder.callFrames.push_back({
			std::move(*builder.callCallee), *start});
		builder.callCallee.reset();
	}
};

template<>
struct editor_action<grammar::named_argument_label> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.callFrames.empty()) return;
		if (const auto span = builder.spanFor(input)) {
			builder.callFrames.back().pendingLabel = ast::Name(input.string(), *span);
		}
	}
};

template<>
struct editor_action<grammar::named_argument_delimiter> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.callFrames.empty()) return;
		if (const auto span = builder.spanFor(input)) {
			auto& frame = builder.callFrames.back();
			frame.label = std::move(frame.pendingLabel);
			frame.pendingLabel.reset();
			frame.labelDelimiterEnd = builder.offsetFor(span->end);
		}
	}
};

template<>
struct editor_action<grammar::call_argument_separator> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.callFrames.empty()) return;
		const auto span = builder.spanFor(input);
		if (!span) return;
		const auto end = builder.offsetFor(span->start);
		const auto next = builder.offsetFor(span->end);
		if (!end || !next) return;
		auto& frame = builder.callFrames.back();
		builder.finishArgument(frame, *end, true);
		frame.argumentStart = *next;
	}
};

template<>
struct editor_action<grammar::call_close_paren> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.callFrames.empty()) return;
		const auto span = builder.spanFor(input);
		if (!span) return;
		const auto end = builder.offsetFor(span->start);
		if (!end) return;
		auto& frame = builder.callFrames.back();
		builder.finishArgument(frame, *end, false);
		frame.closed = true;
	}
};

template<>
struct editor_action<grammar::call> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.finishCall(*span);
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
		source, filename, {}, std::nullopt, std::nullopt, std::nullopt,
		std::nullopt, {}};
	tao::pegtl::memory_input input(source.content(), filename);
	grammar::ParseState state{true};
	tao::pegtl::parse<grammar::rls_file, editor_action>(
		input, builder, state);
	classifyCompleteDeclarations(builder.result, parsedFile);
	std::sort(builder.result.calls.begin(), builder.result.calls.end(),
		[](const EditorCall& left, const EditorCall& right) {
			if (left.span.start.line != right.span.start.line) {
				return left.span.start.line < right.span.start.line;
			}
			if (left.span.start.column != right.span.start.column) {
				return left.span.start.column < right.span.start.column;
			}
			if (left.span.end.line != right.span.end.line) {
				return left.span.end.line < right.span.end.line;
			}
			return left.span.end.column < right.span.end.column;
		});
	builder.result.calls.erase(std::unique(
		builder.result.calls.begin(), builder.result.calls.end(),
		[](const EditorCall& left, const EditorCall& right) {
			return sameSpan(left.span, right.span)
				&& sameSpan(left.callee.span, right.callee.span);
		}), builder.result.calls.end());
	return std::move(builder.result);
}

} // namespace rls::parser