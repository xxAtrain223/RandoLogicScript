#include "source_index.h"

#include <algorithm>
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

bool containsInclusive(const ast::Span& span, ast::Position position) {
	return span.start.line != 0 && isBeforeOrEqual(span.start, position)
		&& isBeforeOrEqual(position, span.end);
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
	if (param.type) {
		index.addName(SourceNameKind::Type, param.type->name);
		index.addTypePosition({param.type->name.span});
	}
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

void SourceIndex::addNamedArgument(NamedArgumentContext context) {
	if (context.labelSpan.start.line == 0) return;
	namedArguments_.push_back(std::move(context));
}

void SourceIndex::addCallArgument(CallArgumentContext context) {
	if (context.valueSpan.start.line == 0) return;
	callArguments_.push_back(std::move(context));
}

void SourceIndex::addSectionEntry(SectionEntryContext context) {
	if (context.labelSpan.start.line == 0) return;
	sectionEntries_.push_back(std::move(context));
}

void SourceIndex::addTypePosition(TypePositionContext context) {
	if (context.typeSpan.start.line == 0) return;
	typePositions_.push_back(std::move(context));
}

void SourceIndex::addEnumName(std::string name) {
	if (std::find(enumNames_.begin(), enumNames_.end(), name) == enumNames_.end()) {
		enumNames_.push_back(std::move(name));
		std::sort(enumNames_.begin(), enumNames_.end());
	}
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
			if (containsInclusive(call.span, position)
				|| containsInclusive(call.callee, position)) {
				result = call;
				break;
			}
			for (size_t index = 0; !result && index < call.argumentRanges.size(); ++index) {
				if (containsInclusive(call.argumentRanges[index], position) ||
					(call.argumentLabels[index]
						&& containsInclusive(*call.argumentLabels[index], position))) {
					result = call;
					break;
				}
			}
		}
	}
	if (!result) return std::nullopt;
	for (size_t index = 0; index < result->argumentRanges.size(); ++index) {
		if (containsInclusive(result->argumentRanges[index], position) ||
			(result->argumentLabels[index]
				&& containsInclusive(*result->argumentLabels[index], position))) {
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
			context.activeSectionEntries = section.entryNames;
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

std::optional<NamedArgumentContext> SourceIndex::namedArgumentAt(ast::Position position) const {
	const NamedArgumentContext* result = nullptr;
	for (const auto& context : namedArguments_) {
		const bool atLabel = isBeforeOrEqual(context.labelSpan.start, position)
			&& isBeforeOrEqual(position, context.labelSpan.end);
		if (atLabel && (!result
			|| spanSize(context.labelSpan) < spanSize(result->labelSpan))) {
			result = &context;
		}
	}
	return result ? std::optional<NamedArgumentContext>(*result) : std::nullopt;
}

std::optional<CallArgumentContext> SourceIndex::callArgumentAt(ast::Position position) const {
	const CallArgumentContext* result = nullptr;
	for (const auto& context : callArguments_) {
		const bool atValue = isBeforeOrEqual(context.valueSpan.start, position)
			&& isBeforeOrEqual(position, context.valueSpan.end);
		if (atValue && (!result
			|| spanSize(context.valueSpan) < spanSize(result->valueSpan))) {
			result = &context;
		}
	}
	return result ? std::optional<CallArgumentContext>(*result) : std::nullopt;
}

std::optional<SectionEntryContext> SourceIndex::sectionEntryAt(ast::Position position) const {
	const SectionEntryContext* result = nullptr;
	for (const auto& context : sectionEntries_) {
		const bool atLabel = isBeforeOrEqual(context.labelSpan.start, position)
			&& isBeforeOrEqual(position, context.labelSpan.end);
		if (atLabel && (!result
			|| spanSize(context.labelSpan) < spanSize(result->labelSpan))) {
			result = &context;
		}
	}
	return result ? std::optional<SectionEntryContext>(*result) : std::nullopt;
}

std::optional<TypePositionContext> SourceIndex::typePositionAt(ast::Position position) const {
	const TypePositionContext* result = nullptr;
	for (const auto& context : typePositions_) {
		const bool atType = isBeforeOrEqual(context.typeSpan.start, position)
			&& isBeforeOrEqual(position, context.typeSpan.end);
		if (atType && (!result
			|| spanSize(context.typeSpan) < spanSize(result->typeSpan))) {
			result = &context;
		}
	}
	return result ? std::optional<TypePositionContext>(*result) : std::nullopt;
}

std::vector<std::string> SourceIndex::sectionEntryNames(
	ast::SectionKind kind, std::optional<std::string_view> regionName) const {
	std::vector<std::string> result;
	for (const auto& indexed : regionContexts_) {
		if (regionName && indexed.context.name != *regionName) continue;
		for (const auto& section : indexed.sections) {
			if (section.kind != kind) continue;
			result.insert(result.end(), section.entryNames.begin(), section.entryNames.end());
		}
	}
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

std::vector<std::string> SourceIndex::regionNames() const {
	std::vector<std::string> result;
	for (const auto& indexed : regionContexts_) {
		if (!indexed.context.extension) result.push_back(indexed.context.name);
	}
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
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
	for (const auto& declaration : file.declarations) {
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			index.addDeclaration(node.span);
			if constexpr (std::is_same_v<T, ast::RegionDecl>) {
				RegionContext context{
					.span = {node.span.file, node.key.span.end, node.span.end},
					.name = node.key.text,
				};
				std::vector<RegionSectionContext> sections;
				for (const auto& data : node.body.data) context.dataKeys.push_back(data.key.text);
				for (const auto& section : node.body.sections) {
					context.sectionKinds.push_back(section.kind);
					RegionSectionContext sectionContext{section.kind, section.span, {}};
					for (const auto& entry : section.entries) {
						sectionContext.entryNames.push_back(entry.name.text);
					}
					sections.push_back(std::move(sectionContext));
				}
				index.addRegionContext(std::move(context), std::move(sections));
				index.addName(SourceNameKind::Declaration, node.key);
				for (const auto& data : node.body.data) {
					index.addSyntax(SyntaxKind::RegionData, data.span);
					index.addName(SourceNameKind::RegionDataKey, data.key);
					indexExpr(index, *data.value);
				}
				indexSections(index, node.body.sections);
			} else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
				RegionContext context{
					.span = {node.span.file, node.name.span.end, node.span.end},
					.name = node.name.text,
					.extension = true,
				};
				std::vector<RegionSectionContext> sections;
				for (const auto& section : node.sections) {
					context.sectionKinds.push_back(section.kind);
					RegionSectionContext sectionContext{section.kind, section.span, {}};
					for (const auto& entry : section.entries) {
						sectionContext.entryNames.push_back(entry.name.text);
					}
					sections.push_back(std::move(sectionContext));
				}
				index.addRegionContext(std::move(context), std::move(sections));
				index.addName(SourceNameKind::Declaration, node.name);
				indexSections(index, node.sections);
			} else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
				index.addName(SourceNameKind::Declaration, node.name);
				for (const auto& parameter : node.params) indexParam(index, parameter);
				indexExpr(index, *node.body);
			} else if constexpr (std::is_same_v<T, ast::ExternDefineDecl>) {
				index.addName(SourceNameKind::Declaration, node.name);
				for (const auto& parameter : node.params) indexParam(index, parameter);
				if (node.returnType) {
					index.addName(SourceNameKind::Type, node.returnType->name);
					index.addTypePosition({node.returnType->name.span});
				}
			} else if constexpr (std::is_same_v<T, ast::EnumDecl>) {
				index.addEnumName(node.name.text);
				index.addName(SourceNameKind::Declaration, node.name);
				for (const auto& member : node.members) index.addName(SourceNameKind::EnumMember, member.name);
			} else if constexpr (std::is_same_v<T, ast::ExternEnumDecl>) {
				index.addEnumName(node.name.text);
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