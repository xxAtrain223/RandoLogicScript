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
		bool expectsArgument = false;
		bool recovered = false;
		bool closed = false;
	};
	struct SectionFrame {
		ast::SectionKind kind;
		size_t bodyStart = 0;
		std::vector<EditorSectionEntry> entries;
		std::optional<ast::Name> pendingEntry;
		bool entryHasDelimiter = false;
		std::optional<size_t> closeStart;
	};
	struct RegionFrame {
		ast::Name name;
		bool extension = false;
		size_t bodyStart = 0;
		std::vector<std::string> dataKeys;
		std::vector<EditorRegionSection> sections;
		std::optional<ast::Name> pendingDataKey;
		std::optional<size_t> closeEnd;
	};

	const ast::SourceText& source;
	std::string_view filename;
	EditorSyntax result;
	std::optional<ast::Name> memberObject;
	std::optional<size_t> memberAccessIndex;
	std::optional<size_t> typePositionIndex;
	std::optional<ast::Name> callCallee;
	std::vector<CallFrame> callFrames;
	bool nextRegionExtension = false;
	std::optional<ast::Name> regionName;
	std::vector<RegionFrame> regionFrames;
	std::optional<ast::SectionKind> sectionKind;
	std::vector<SectionFrame> sectionFrames;

	EditorSyntaxBuilder(
		const ast::SourceText& source, std::string_view filename)
		: source(source), filename(filename) {}

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
		if (frame.labelDelimiterEnd && valueStart == contentEnd) {
			frame.recovered = true;
		}

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
			frame.closed && !frame.recovered
				? SyntaxRecoveryStatus::Complete
				: SyntaxRecoveryStatus::Recovered});
	}

	void addBlankSectionEntries(SectionFrame& frame, size_t bodyEnd) {
		size_t lineStart = frame.bodyStart;
		while (lineStart <= bodyEnd) {
			size_t lineEnd = source.content().find('\n', lineStart);
			if (lineEnd == std::string::npos || lineEnd > bodyEnd) lineEnd = bodyEnd;
			if (lineEnd > lineStart && source.content()[lineEnd - 1] == '\r') {
				--lineEnd;
			}
			size_t contentStart = lineStart;
			while (contentStart < lineEnd
				&& (source.content()[contentStart] == ' '
					|| source.content()[contentStart] == '\t')) {
				++contentStart;
			}
			if (contentStart == lineEnd) {
				if (const auto span = spanFromOffsets(contentStart, contentStart)) {
					const bool duplicate = std::any_of(
						frame.entries.begin(), frame.entries.end(),
						[&](const EditorSectionEntry& entry) {
							return entry.labelSpan.start.line == span->start.line
								&& entry.labelSpan.start.column == span->start.column;
						});
					if (!duplicate) frame.entries.push_back({std::nullopt, *span});
				}
			}
			if (lineEnd >= bodyEnd) break;
			lineStart = lineEnd + 1;
		}
	}

	void finishSection(const ast::Span& span) {
		if (sectionFrames.empty() || regionFrames.empty()) return;
		auto frame = std::move(sectionFrames.back());
		sectionFrames.pop_back();
		const size_t bodyEnd = frame.closeStart.value_or(
			offsetFor(span.end).value_or(source.content().size()));
		addBlankSectionEntries(frame, bodyEnd);
		const auto bodySpan = spanFromOffsets(frame.bodyStart, bodyEnd);
		if (!bodySpan) return;
		regionFrames.back().sections.push_back({
			frame.kind, *bodySpan, std::move(frame.entries)});
	}

	void finishRegion(const ast::Span& span) {
		if (regionFrames.empty()) return;
		auto frame = std::move(regionFrames.back());
		regionFrames.pop_back();
		const size_t bodyEnd = frame.closeEnd.value_or(
			offsetFor(span.end).value_or(source.content().size()));
		const auto bodySpan = spanFromOffsets(frame.bodyStart, bodyEnd);
		if (!bodySpan) return;
		result.regions.push_back({
			std::move(frame.name), *bodySpan, frame.extension,
			std::move(frame.dataKeys), std::move(frame.sections),
			SyntaxRecoveryStatus::Recovered});
		nextRegionExtension = false;
		regionName.reset();
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
		frame.expectsArgument = true;
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
		size_t contentStart = frame.argumentStart;
		while (contentStart < *end
			&& std::isspace(static_cast<unsigned char>(
				builder.source.content()[contentStart]))) {
			++contentStart;
		}
		if (contentStart < *end || frame.label) {
			builder.finishArgument(frame, *end, false);
		} else if (frame.expectsArgument) {
			builder.finishArgument(frame, *end, true);
			frame.recovered = true;
		}
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

template<>
struct editor_action<grammar::kw_extend> {
	template<typename Input>
	static void apply(
		const Input&, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		builder.nextRegionExtension = true;
	}
};

template<>
struct editor_action<grammar::region_name> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.regionName = ast::Name(input.string(), *span);
		}
	}
};

template<>
struct editor_action<grammar::region_open_brace> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		const auto span = builder.spanFor(input);
		if (!span || !builder.regionName) return;
		const auto start = builder.offsetFor(span->end);
		if (!start) return;
		builder.regionFrames.push_back({
			std::move(*builder.regionName), builder.nextRegionExtension, *start});
		builder.regionName.reset();
	}
};

template<>
struct editor_action<grammar::region_data_key> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.regionFrames.empty()) return;
		if (const auto span = builder.spanFor(input)) {
			builder.regionFrames.back().pendingDataKey =
				ast::Name(input.string(), *span);
		}
	}
};

template<>
struct editor_action<grammar::region_data_delimiter> {
	template<typename Input>
	static void apply(
		const Input&, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.regionFrames.empty()) return;
		auto& frame = builder.regionFrames.back();
		if (frame.pendingDataKey && !frame.extension) {
			frame.dataKeys.push_back(frame.pendingDataKey->text);
		}
		frame.pendingDataKey.reset();
	}
};

template<>
struct editor_action<grammar::section_kind> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		const auto text = input.string_view();
		if (text == "events") builder.sectionKind = ast::SectionKind::Events;
		else if (text == "locations") builder.sectionKind = ast::SectionKind::Locations;
		else if (text == "exits") builder.sectionKind = ast::SectionKind::Exits;
	}
};

template<>
struct editor_action<grammar::section_open_brace> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		const auto span = builder.spanFor(input);
		if (!span || !builder.sectionKind) return;
		const auto start = builder.offsetFor(span->end);
		if (!start) return;
		builder.sectionFrames.push_back({*builder.sectionKind, *start});
		builder.sectionKind.reset();
	}
};

template<>
struct editor_action<grammar::entry_label> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.sectionFrames.empty()) return;
		if (const auto span = builder.spanFor(input)) {
			auto& frame = builder.sectionFrames.back();
			frame.pendingEntry = ast::Name(input.string(), *span);
			frame.entryHasDelimiter = false;
		}
	}
};

template<>
struct editor_action<grammar::entry_delimiter> {
	template<typename Input>
	static void apply(
		const Input&, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (!builder.sectionFrames.empty()) {
			builder.sectionFrames.back().entryHasDelimiter = true;
		}
	}
};

template<>
struct editor_action<grammar::entry> {
	template<typename Input>
	static void apply(
		const Input&, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.sectionFrames.empty()) return;
		auto& frame = builder.sectionFrames.back();
		if (!frame.pendingEntry) return;
		frame.entries.push_back({
			frame.entryHasDelimiter
				? frame.pendingEntry
				: std::nullopt,
			frame.pendingEntry->span});
		frame.pendingEntry.reset();
		frame.entryHasDelimiter = false;
	}
};

template<>
struct editor_action<grammar::section_close_brace> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.sectionFrames.empty()) return;
		if (const auto span = builder.spanFor(input)) {
			builder.sectionFrames.back().closeStart =
				builder.offsetFor(span->start);
		}
	}
};

template<>
struct editor_action<grammar::section> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.finishSection(*span);
		}
	}
};

template<>
struct editor_action<grammar::region_close_brace> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (builder.regionFrames.empty()) return;
		if (const auto span = builder.spanFor(input)) {
			builder.regionFrames.back().closeEnd =
				builder.offsetFor(span->end);
		}
	}
};

template<>
struct editor_action<grammar::region_decl> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) builder.finishRegion(*span);
	}
};

template<>
struct editor_action<grammar::extend_decl> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) builder.finishRegion(*span);
	}
};

template<>
struct editor_action<grammar::declaration> {
	template<typename Input>
	static void apply(
		const Input& input, EditorSyntaxBuilder& builder,
		grammar::ParseState&) {
		if (const auto span = builder.spanFor(input)) {
			builder.result.declarations.push_back({*span});
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

void classifyCompleteDeclarationsImpl(EditorSyntax& syntax, const ast::File& file) {
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
	for (auto& candidate : syntax.regions) {
		for (const auto& declaration : file.declarations) {
			const bool complete = std::visit([&](const auto& node) {
				using T = std::decay_t<decltype(node)>;
				if constexpr (std::is_same_v<T, ast::RegionDecl>) {
					return !candidate.extension
						&& candidate.name.text == node.key.text
						&& sameSpan(candidate.name.span, node.key.span);
				} else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
					return candidate.extension
						&& candidate.name.text == node.name.text
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
	EditorSyntaxBuilder builder(source, filename);
	tao::pegtl::memory_input input(source.content(), filename);
	grammar::ParseState state{true};
	tao::pegtl::parse<grammar::rls_file, editor_action>(
		input, builder, state);
	ClassifyEditorSyntax(builder.result, parsedFile);
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

void ClassifyEditorSyntax(EditorSyntax& syntax, const ast::File& parsedFile) {
	classifyCompleteDeclarationsImpl(syntax, parsedFile);
}

} // namespace rls::parser