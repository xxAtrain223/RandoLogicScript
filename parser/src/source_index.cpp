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

std::vector<SyntaxContext> SourceIndex::declarationsIn(std::string_view file) const {
	std::vector<SyntaxContext> result;
	for (const auto& declaration : declarations_) {
		if (declaration.span.file == file) result.push_back(declaration);
	}
	return result;
}

SourceIndex BuildSourceIndex(const ast::File& file) {
	SourceIndex index;
	for (const auto& declaration : file.declarations) {
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			index.addDeclaration(node.span);
			if constexpr (std::is_same_v<T, ast::RegionDecl>) {
				index.addName(SourceNameKind::Declaration, node.key);
				for (const auto& data : node.body.data) {
					index.addSyntax(SyntaxKind::RegionData, data.span);
					index.addName(SourceNameKind::RegionDataKey, data.key);
					indexExpr(index, *data.value);
				}
				indexSections(index, node.body.sections);
			} else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
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