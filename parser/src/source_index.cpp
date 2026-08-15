#include "source_index.h"

#include <algorithm>
#include <cctype>
#include <type_traits>

namespace rls::parser {

namespace {

bool isBeforeOrEqual(ast::Position left, ast::Position right) {
	return left.line < right.line || (left.line == right.line && left.column <= right.column);
}

bool contains(const ast::Span& span, ast::Position position) {
	return span.start.line != 0 && isBeforeOrEqual(span.start, position) &&
		isBeforeOrEqual(position, span.end) && !(position.line == span.end.line && position.column == span.end.column);
}

size_t spanSize(const ast::Span& span) {
	return (static_cast<size_t>(span.end.line - span.start.line) << 32) +
		span.end.column - span.start.column;
}

template<typename Context>
std::optional<Context> narrowestAt(const std::vector<Context>& contexts, ast::Position position) {
	const Context* result = nullptr;
	for (const auto& context : contexts) {
		if (contains(context.span, position) && (!result || spanSize(context.span) < spanSize(result->span))) {
			result = &context;
		}
	}
	return result ? std::optional<Context>(*result) : std::nullopt;
}

void indexExpr(SourceIndex& index, const ast::Expr& expr);

void indexParam(SourceIndex& index, const ast::Param& param) {
	index.addName(SourceNameKind::Parameter, param.name);
	if (param.type) index.addName(SourceNameKind::Type, param.type->name);
	if (param.defaultValue) indexExpr(index, *param.defaultValue);
}

void indexExpr(SourceIndex& index, const ast::Expr& expr) {
	index.addExpression(expr.span);
	std::visit([&](const auto& node) {
		using T = std::decay_t<decltype(node)>;
		if constexpr (std::is_same_v<T, ast::Identifier>) {
			index.addName(SourceNameKind::Identifier, node.name);
		} else if constexpr (std::is_same_v<T, ast::MemberExpr>) {
			index.addName(SourceNameKind::MemberObject, node.object);
			index.addName(SourceNameKind::Member, node.member);
			index.addMemberAccess({node.object.text, node.member.span});
		} else if constexpr (std::is_same_v<T, ast::UnaryExpr>) {
			indexExpr(index, *node.operand);
		} else if constexpr (std::is_same_v<T, ast::BinaryExpr>) {
			indexExpr(index, *node.left);
			indexExpr(index, *node.right);
		} else if constexpr (std::is_same_v<T, ast::TernaryExpr>) {
			indexExpr(index, *node.condition);
			indexExpr(index, *node.thenBranch);
			indexExpr(index, *node.elseBranch);
		} else if constexpr (std::is_same_v<T, ast::CallExpr>) {
			index.addName(SourceNameKind::CallCallee, node.callee);
			CallContext call{expr.span, node.callee.span, {}, {}, std::nullopt};
			for (const auto& argument : node.args) {
				call.argumentRanges.push_back(argument.value->span);
				call.argumentLabels.push_back(argument.name ? std::optional<ast::Span>(argument.name->span) : std::nullopt);
				if (argument.name) index.addName(SourceNameKind::ArgumentLabel, *argument.name);
				index.addSyntax(SyntaxKind::Argument, argument.value->span);
				indexExpr(index, *argument.value);
			}
			index.addCall(std::move(call));
			index.addSyntax(SyntaxKind::Call, expr.span);
		} else if constexpr (std::is_same_v<T, ast::InvokeExpr>) {
			indexExpr(index, *node.callee);
		} else if constexpr (std::is_same_v<T, ast::MatchExpr>) {
			indexExpr(index, *node.discriminant);
			for (const auto& arm : node.arms) {
				for (const auto& pattern : arm.patterns) indexExpr(index, *pattern);
				indexExpr(index, *arm.body);
			}
		} else if constexpr (std::is_same_v<T, ast::ListExpr>) {
			for (const auto& element : node.elements) indexExpr(index, *element);
		}
	}, expr.node);
}

void indexEntry(SourceIndex& index, const ast::Entry& entry) {
	index.addSyntax(SyntaxKind::Entry, entry.span);
	index.addName(SourceNameKind::Entry, entry.name);
	indexExpr(index, *entry.condition);
}

void indexSections(SourceIndex& index, const std::vector<ast::Section>& sections) {
	for (const auto& section : sections) {
		index.addSyntax(SyntaxKind::Section, section.span);
		for (const auto& entry : section.entries) indexEntry(index, entry);
	}
}

std::optional<ast::SectionKind> sectionKind(std::string_view token) {
	if (token == "events") return ast::SectionKind::Events;
	if (token == "locations") return ast::SectionKind::Locations;
	if (token == "exits") return ast::SectionKind::Exits;
	return std::nullopt;
}

struct RecoveryToken {
	std::string_view text;
	size_t end = 0;
	char punctuation = 0;
};

std::vector<RecoveryToken> recoveryTokens(std::string_view source) {
	std::vector<RecoveryToken> result;
	for (size_t offset = 0; offset < source.size();) {
		const char character = source[offset];
		if (character == '#') {
			while (offset < source.size() && source[offset] != '\n') ++offset;
			continue;
		}
		if (character == '"') {
			++offset;
			while (offset < source.size()) {
				if (source[offset] == '\\' && offset + 1 < source.size()) {
					offset += 2;
				} else if (source[offset++] == '"') {
					break;
				}
			}
			continue;
		}
		if (std::isalpha(static_cast<unsigned char>(character)) || character == '_') {
			const size_t start = offset++;
			while (offset < source.size()
				&& (std::isalnum(static_cast<unsigned char>(source[offset]))
					|| source[offset] == '_')) {
				++offset;
			}
			result.push_back({source.substr(start, offset - start), offset, 0});
			continue;
		}
		if (character == '{' || character == '}' || character == ':' || character == '.') {
			result.push_back({source.substr(offset, 1), offset + 1, character});
		}
		++offset;
	}
	return result;
}

std::optional<ast::Span> spanFromOffsets(
	const ast::SourceText& source, std::string_view file, size_t start, size_t end);

void addRecoveredMemberAccesses(
	SourceIndex& index, const ast::File& file, const ast::SourceText& source) {
	const auto tokens = recoveryTokens(source.content());
	for (size_t tokenIndex = 0; tokenIndex + 1 < tokens.size(); ++tokenIndex) {
		const auto& object = tokens[tokenIndex];
		const auto& dot = tokens[tokenIndex + 1];
		if (object.punctuation != 0 || dot.punctuation != '.'
			|| object.end != dot.end - 1) {
			continue;
		}

		size_t memberEnd = dot.end;
		if (tokenIndex + 2 < tokens.size()) {
			const auto& member = tokens[tokenIndex + 2];
			const size_t memberStart = member.end - member.text.size();
			if (member.punctuation == 0 && memberStart == dot.end) {
				memberEnd = member.end;
			}
		}
		const auto memberSpan = spanFromOffsets(
			source, file.path, dot.end, memberEnd);
		if (memberSpan) {
			index.addMemberAccess({std::string(object.text), *memberSpan});
		}
	}
}

std::optional<ast::Span> spanFromOffsets(
	const ast::SourceText& source, std::string_view file, size_t start, size_t end) {
	const auto startPosition = source.utf8PositionAtByteOffset(start);
	const auto endPosition = source.utf8PositionAtByteOffset(end);
	if (!startPosition || !endPosition) return std::nullopt;
	return ast::Span{std::string(file), *startPosition, *endPosition};
}

void addRecoveredRegionContexts(
	SourceIndex& index, const ast::File& file, const ast::SourceText& source) {
	const auto tokens = recoveryTokens(source.content());
	for (size_t tokenIndex = 0; tokenIndex < tokens.size(); ++tokenIndex) {
		bool extension = false;
		size_t regionIndex = tokenIndex;
		if (tokens[tokenIndex].text == "extend") {
			extension = true;
			if (++regionIndex >= tokens.size() || tokens[regionIndex].text != "region") continue;
		} else if (tokens[tokenIndex].text != "region") {
			continue;
		}
		if (regionIndex + 2 >= tokens.size()
			|| tokens[regionIndex + 1].punctuation != 0
			|| tokens[regionIndex + 2].punctuation != '{') {
			continue;
		}

		const size_t openIndex = regionIndex + 2;
		size_t closeIndex = tokens.size();
		size_t depth = 1;
		for (size_t cursor = openIndex + 1; cursor < tokens.size(); ++cursor) {
			if (tokens[cursor].punctuation == '{') ++depth;
			if (tokens[cursor].punctuation == '}' && --depth == 0) {
				closeIndex = cursor;
				break;
			}
		}
		const size_t bodyEnd = closeIndex < tokens.size()
			? tokens[closeIndex].end : source.content().size();
		const auto bodySpan = spanFromOffsets(
			source, file.path, tokens[openIndex].end, bodyEnd);
		if (!bodySpan) continue;

		RegionContext context{.span = *bodySpan, .extension = extension};
		std::vector<RegionSectionContext> sections;
		depth = 1;
		for (size_t cursor = openIndex + 1; cursor < closeIndex && cursor < tokens.size(); ++cursor) {
			if (tokens[cursor].punctuation == '{') {
				++depth;
				continue;
			}
			if (tokens[cursor].punctuation == '}') {
				if (depth > 1) --depth;
				continue;
			}
			if (depth != 1 || tokens[cursor].punctuation != 0 || cursor + 1 >= tokens.size()) {
				continue;
			}
			if (tokens[cursor + 1].punctuation == ':' && !extension) {
				context.dataKeys.emplace_back(tokens[cursor].text);
				continue;
			}
			const auto kind = sectionKind(tokens[cursor].text);
			if (!kind || tokens[cursor + 1].punctuation != '{') continue;
			context.sectionKinds.push_back(*kind);
			size_t sectionDepth = 1;
			size_t sectionClose = closeIndex;
			for (size_t sectionCursor = cursor + 2;
				 sectionCursor < closeIndex && sectionCursor < tokens.size(); ++sectionCursor) {
				if (tokens[sectionCursor].punctuation == '{') ++sectionDepth;
				if (tokens[sectionCursor].punctuation == '}' && --sectionDepth == 0) {
					sectionClose = sectionCursor;
					break;
				}
			}
			const size_t sectionEnd = sectionClose < tokens.size()
				? tokens[sectionClose].end : bodyEnd;
			if (const auto sectionSpan = spanFromOffsets(
					source, file.path, tokens[cursor + 1].end, sectionEnd)) {
				sections.push_back({*kind, *sectionSpan});
			}
		}
		index.addRegionContext(std::move(context), std::move(sections));
		tokenIndex = closeIndex < tokens.size() ? closeIndex : tokens.size();
	}
}

} // namespace

void SourceIndex::addSyntax(SyntaxKind kind, const ast::Span& span) {
	if (span.start.line != 0) syntax_.push_back({kind, span});
}

void SourceIndex::addName(SourceNameKind kind, const ast::Name& name) {
	if (name.span.start.line != 0) names_.push_back({kind, name.text, name.span});
}

void SourceIndex::addExpression(const ast::Span& span) {
	if (span.start.line != 0) expressions_.push_back({SyntaxKind::Expression, span});
	addSyntax(SyntaxKind::Expression, span);
}

void SourceIndex::addCall(CallContext call) {
	calls_.push_back(std::move(call));
}

void SourceIndex::addDeclaration(const ast::Span& span) {
	if (span.start.line == 0) return;
	declarations_.push_back({SyntaxKind::Declaration, span});
	addSyntax(SyntaxKind::Declaration, span);
}

void SourceIndex::addRegionContext(
	RegionContext context, std::vector<RegionSectionContext> sections) {
	if (context.span.start.line == 0) return;
	regionContexts_.push_back({std::move(context), std::move(sections)});
}

void SourceIndex::addMemberAccess(MemberAccessContext context) {
	if (context.memberSpan.start.line == 0) return;
	memberAccesses_.push_back(std::move(context));
}

std::optional<SyntaxContext> SourceIndex::syntaxAt(ast::Position position) const {
	if (const auto name = narrowestAt(names_, position)) return SyntaxContext{SyntaxKind::Name, name->span};
	return narrowestAt(syntax_, position);
}

std::optional<SourceNameContext> SourceIndex::nameAt(ast::Position position) const {
	return narrowestAt(names_, position);
}

std::optional<SyntaxContext> SourceIndex::enclosingExpression(ast::Position position) const {
	return narrowestAt(expressions_, position);
}

std::optional<CallContext> SourceIndex::enclosingCall(ast::Position position) const {
	auto result = narrowestAt(calls_, position);
	if (!result) {
		for (const auto& call : calls_) {
			if (contains(call.callee, position)) {
				result = call;
				break;
			}
			for (size_t index = 0; !result && index < call.argumentRanges.size(); ++index) {
				if (contains(call.argumentRanges[index], position) ||
					(call.argumentLabels[index] && contains(*call.argumentLabels[index], position))) {
					result = call;
					break;
				}
			}
		}
	}
	if (!result) return std::nullopt;
	for (size_t index = 0; index < result->argumentRanges.size(); ++index) {
		if (contains(result->argumentRanges[index], position) ||
			(result->argumentLabels[index] && contains(*result->argumentLabels[index], position))) {
			result->activeArgument = index;
			break;
		}
	}
	return result;
}

std::optional<RegionContext> SourceIndex::regionContextAt(ast::Position position) const {
	const IndexedRegionContext* result = nullptr;
	for (const auto& indexed : regionContexts_) {
		if (contains(indexed.context.span, position)
			&& (!result || spanSize(indexed.context.span) < spanSize(result->context.span))) {
			result = &indexed;
		}
	}
	if (!result) return std::nullopt;
	RegionContext context = result->context;
	for (const auto& section : result->sections) {
		if (contains(section.span, position)) {
			context.activeSection = section.kind;
			break;
		}
	}
	return context;
}

std::optional<MemberAccessContext> SourceIndex::memberAccessAt(ast::Position position) const {
	const MemberAccessContext* result = nullptr;
	for (const auto& context : memberAccesses_) {
		const bool atMember = isBeforeOrEqual(context.memberSpan.start, position)
			&& isBeforeOrEqual(position, context.memberSpan.end);
		if (atMember && (!result
			|| spanSize(context.memberSpan) < spanSize(result->memberSpan))) {
			result = &context;
		}
	}
	return result ? std::optional<MemberAccessContext>(*result) : std::nullopt;
}

std::vector<SyntaxContext> SourceIndex::declarationsIn(std::string_view file) const {
	std::vector<SyntaxContext> result;
	for (const auto& declaration : declarations_) {
		if (declaration.span.file == file) result.push_back(declaration);
	}
	return result;
}

SourceIndex BuildSourceIndex(const ast::File& file, const ast::SourceText* source) {
	SourceIndex index;
	if (source) {
		addRecoveredRegionContexts(index, file, *source);
		addRecoveredMemberAccesses(index, file, *source);
	}
	for (const auto& declaration : file.declarations) {
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			index.addDeclaration(node.span);
			if constexpr (std::is_same_v<T, ast::RegionDecl>) {
				if (!source) {
					RegionContext context{
						.span = {node.span.file, node.key.span.end, node.span.end},
					};
					std::vector<RegionSectionContext> sections;
					for (const auto& data : node.body.data) context.dataKeys.push_back(data.key.text);
					for (const auto& section : node.body.sections) {
						context.sectionKinds.push_back(section.kind);
						sections.push_back({section.kind, section.span});
					}
					index.addRegionContext(std::move(context), std::move(sections));
				}
				index.addName(SourceNameKind::Declaration, node.key);
				for (const auto& data : node.body.data) {
					index.addSyntax(SyntaxKind::RegionData, data.span);
					index.addName(SourceNameKind::RegionDataKey, data.key);
					indexExpr(index, *data.value);
				}
				indexSections(index, node.body.sections);
			} else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
				if (!source) {
					RegionContext context{
						.span = {node.span.file, node.name.span.end, node.span.end},
						.extension = true,
					};
					std::vector<RegionSectionContext> sections;
					for (const auto& section : node.sections) {
						context.sectionKinds.push_back(section.kind);
						sections.push_back({section.kind, section.span});
					}
					index.addRegionContext(std::move(context), std::move(sections));
				}
				index.addName(SourceNameKind::Declaration, node.name);
				indexSections(index, node.sections);
			} else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
				index.addName(SourceNameKind::Declaration, node.name);
				for (const auto& parameter : node.params) indexParam(index, parameter);
				indexExpr(index, *node.body);
			} else if constexpr (std::is_same_v<T, ast::ExternDefineDecl>) {
				index.addName(SourceNameKind::Declaration, node.name);
				for (const auto& parameter : node.params) indexParam(index, parameter);
				if (node.returnType) index.addName(SourceNameKind::Type, node.returnType->name);
			} else if constexpr (std::is_same_v<T, ast::EnumDecl>) {
				index.addName(SourceNameKind::Declaration, node.name);
				for (const auto& member : node.members) index.addName(SourceNameKind::EnumMember, member.name);
			} else if constexpr (std::is_same_v<T, ast::ExternEnumDecl>) {
				index.addName(SourceNameKind::Declaration, node.name);
				for (const auto& entry : node.entries) {
					if (const auto* member = std::get_if<ast::EnumMemberDecl>(&entry)) {
						index.addName(SourceNameKind::EnumMember, member->name);
					}
				}
			}
		}, declaration);
	}
	return index;
}

} // namespace rls::parser