#include "semantic_index.h"

#include <algorithm>
#include <format>
#include <functional>
#include <type_traits>
#include <unordered_map>

namespace rls::sema {

SymbolId SemanticIndex::addSymbol(SymbolCategory category, SymbolProvenance provenance,
	std::string displayName, ast::Span declaration, ast::Span selection,
		std::optional<SymbolId> container, std::optional<std::string> signature,
		std::optional<ast::Type> type, std::optional<std::string> enumName) {
	const SymbolId id{symbols_.size() + 1};
	symbols_.push_back({id, category, provenance, std::move(displayName),
		std::move(declaration), std::move(selection), std::move(signature), type,
		std::move(enumName), container});
	occurrences_.push_back({id, symbols_.back().selection, OccurrenceKind::Declaration});
	return id;
}

std::optional<SymbolRecord> SemanticIndex::declaration(SymbolId id) const {
	for (const auto& symbol : symbols_) {
		if (symbol.id == id) return symbol;
	}
	return std::nullopt;
}

std::vector<OccurrenceRecord> SemanticIndex::occurrencesFor(SymbolId id) const {
	std::vector<OccurrenceRecord> result;
	for (const auto& occurrence : occurrences_) {
		if (occurrence.symbol == id) result.push_back(occurrence);
	}
	std::sort(result.begin(), result.end(), [](const OccurrenceRecord& left, const OccurrenceRecord& right) {
		return std::tie(left.span.file, left.span.start.line, left.span.start.column,
			left.span.end.line, left.span.end.column) <
			std::tie(right.span.file, right.span.start.line, right.span.start.column,
				right.span.end.line, right.span.end.column);
	});
	return result;
}

namespace {

bool isBeforeOrEqual(ast::Position left, ast::Position right) {
	return left.line < right.line || (left.line == right.line && left.column <= right.column);
}

bool contains(const ast::Span& span, std::string_view file, ast::Position position) {
	return span.file == file && span.start.line != 0 && isBeforeOrEqual(span.start, position) &&
		isBeforeOrEqual(position, span.end) &&
		!(position.line == span.end.line && position.column == span.end.column);
}

size_t spanSize(const ast::Span& span) {
	return (static_cast<size_t>(span.end.line - span.start.line) << 32) +
		span.end.column - span.start.column;
}

template<typename Record>
std::optional<Record> narrowestAt(const std::vector<Record>& records, std::string_view file,
	ast::Position position) {
	const Record* result = nullptr;
	for (const auto& record : records) {
		if (contains(record.span, file, position) && (!result || spanSize(record.span) < spanSize(result->span))) {
			result = &record;
		}
	}
	return result ? std::optional<Record>(*result) : std::nullopt;
}

} // namespace

std::optional<OccurrenceRecord> SemanticIndex::occurrenceAt(std::string_view file,
	ast::Position position) const {
	const OccurrenceRecord* result = nullptr;
	for (const auto& occurrence : occurrences_) {
		if (!contains(occurrence.span, file, position)) continue;
		if (!result || spanSize(occurrence.span) < spanSize(result->span) ||
			(spanSize(occurrence.span) == spanSize(result->span) &&
				result->kind == OccurrenceKind::Declaration && occurrence.kind != OccurrenceKind::Declaration)) {
			result = &occurrence;
		}
	}
	return result ? std::optional<OccurrenceRecord>(*result) : std::nullopt;
}

std::optional<TypeRecord> SemanticIndex::typeAt(std::string_view file, ast::Position position) const {
	return narrowestAt(types_, file, position);
}

std::optional<CallRecord> SemanticIndex::callAt(std::string_view file, ast::Position position) const {
	return narrowestAt(calls_, file, position);
}

std::vector<SymbolId> SemanticIndex::visibleSymbolsAt(std::string_view file,
	ast::Position position) const {
	std::vector<SymbolId> result;
	for (const auto& symbol : symbols_) {
		if (!symbol.container) {
			result.push_back(symbol.id);
			continue;
		}
		const auto container = declaration(*symbol.container);
		if (symbol.category == SymbolCategory::Parameter && container &&
			container->category == SymbolCategory::Define && contains(container->declaration, file, position)) {
			result.push_back(symbol.id);
		}
	}
	return result;
}

SemanticIndex buildSemanticIndex(const ast::Project& project) {
	SemanticIndex index;
	auto addParameters = [&](const std::vector<ast::Param>& parameters, SymbolId container) {
		for (const auto& parameter : parameters) {
			index.addSymbol(SymbolCategory::Parameter, SymbolProvenance::Source,
				parameter.name.text, parameter.name.span, parameter.name.span,
				container, std::nullopt, std::nullopt,
				parameter.type ? std::optional<std::string>(parameter.type->name.text) : std::nullopt);
		}
	};
	auto addSections = [&](const std::vector<ast::Section>& sections, SymbolId container) {
		for (const auto& section : sections) {
			for (const auto& entry : section.entries) {
				index.addSymbol(SymbolCategory::SectionEntry, SymbolProvenance::Source,
					entry.name.text, entry.span, entry.name.span, container);
			}
		}
	};

	for (const auto& file : project.files) {
		for (const auto& declaration : file.declarations) {
			std::visit([&](const auto& node) {
				using T = std::decay_t<decltype(node)>;
				if constexpr (std::is_same_v<T, ast::RegionDecl>) {
					const auto id = index.addSymbol(SymbolCategory::Region, SymbolProvenance::Source,
						node.key.text, node.span, node.key.span);
					for (const auto& data : node.body.data) {
						index.addSymbol(SymbolCategory::RegionDataEntry, SymbolProvenance::Source,
							data.key.text, data.span, data.key.span, id);
					}
					addSections(node.body.sections, id);
				} else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
					const auto id = index.addSymbol(SymbolCategory::RegionExtension, SymbolProvenance::Source,
						node.name.text, node.span, node.name.span);
					addSections(node.sections, id);
				} else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
					const auto id = index.addSymbol(SymbolCategory::Define, SymbolProvenance::Source,
						node.name.text, node.span, node.name.span, std::nullopt,
						"define " + node.name.text);
					addParameters(node.params, id);
				} else if constexpr (std::is_same_v<T, ast::ExternDefineDecl>) {
					const auto id = index.addSymbol(SymbolCategory::ExternDefine, SymbolProvenance::Extern,
						node.name.text, node.span, node.name.span, std::nullopt,
						"extern define " + node.name.text);
					addParameters(node.params, id);
				} else if constexpr (std::is_same_v<T, ast::EnumDecl>) {
					const auto id = index.addSymbol(SymbolCategory::Enum, SymbolProvenance::Source,
						node.name.text, node.span, node.name.span, std::nullopt, std::nullopt,
						ast::Type::Enum, node.name.text);
					for (const auto& member : node.members) {
						index.addSymbol(SymbolCategory::EnumMember, SymbolProvenance::Source,
							member.name.text, member.span, member.name.span, id, std::nullopt,
							ast::Type::Enum, node.name.text);
					}
				} else if constexpr (std::is_same_v<T, ast::ExternEnumDecl>) {
					const auto id = index.addSymbol(SymbolCategory::Enum, SymbolProvenance::Extern,
						node.name.text, node.span, node.name.span, std::nullopt, std::nullopt,
						ast::Type::Enum, node.name.text);
					for (const auto& entry : node.entries) {
						if (const auto* member = std::get_if<ast::EnumMemberDecl>(&entry)) {
							index.addSymbol(SymbolCategory::EnumMember, SymbolProvenance::Extern,
								member->name.text, member->span, member->name.span, id, std::nullopt,
								ast::Type::Enum, node.name.text);
						} else {
							const auto& pattern = std::get<ast::EnumPatternDecl>(entry);
							index.addSymbol(SymbolCategory::ExternEnumPattern, SymbolProvenance::Pattern,
								pattern.pattern, pattern.span, pattern.span, id, std::nullopt,
								ast::Type::Enum, node.name.text);
						}
					}
				}
			}, declaration);
		}
	}

	auto findSymbol = [&](SymbolCategory category, std::string_view name) -> std::optional<SymbolId> {
		for (const auto& symbol : index.symbols_) {
			if (symbol.category == category && symbol.displayName == name) return symbol.id;
		}
		return std::nullopt;
	};
	auto addDuplicateDiagnostic = [&](std::string_view code, std::string_view kind,
		std::string_view name, const ast::Span& first, const ast::Span& duplicate) {
		index.diagnostics_.push_back({
			std::string(code),
			ast::DiagnosticLevel::Error,
			std::format("duplicate {} '{}'", kind, name),
			duplicate,
			{{"first declaration", first}},
		});
	};
	std::unordered_map<std::string, ast::Span> regions;
	std::unordered_map<std::string, ast::Span> functions;
	std::unordered_map<std::string, ast::Span> enums;
	for (const auto& file : project.files) {
		for (const auto& declaration : file.declarations) {
			std::visit([&](const auto& node) {
				using T = std::decay_t<decltype(node)>;
				if constexpr (std::is_same_v<T, ast::RegionDecl>) {
					if (const auto [it, inserted] = regions.try_emplace(node.key.text, node.span); !inserted) {
						addDuplicateDiagnostic("RLS-S001", "region", node.key.text, it->second, node.span);
					}
				} else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
					if (const auto [it, inserted] = functions.try_emplace(node.name.text, node.span); !inserted) {
						addDuplicateDiagnostic("RLS-S002", "function", node.name.text, it->second, node.span);
					}
				} else if constexpr (std::is_same_v<T, ast::ExternDefineDecl>) {
					if (const auto [it, inserted] = functions.try_emplace(node.name.text, node.span); !inserted) {
						addDuplicateDiagnostic("RLS-S002", "function", node.name.text, it->second, node.span);
					}
				} else if constexpr (std::is_same_v<T, ast::EnumDecl> || std::is_same_v<T, ast::ExternEnumDecl>) {
					if (const auto [it, inserted] = enums.try_emplace(node.name.text, node.span); !inserted) {
						addDuplicateDiagnostic("RLS-S003", "enum", node.name.text, it->second, node.span);
					}
				}
			}, declaration);
		}
	}
	for (const auto& file : project.files) {
		for (const auto& declaration : file.declarations) {
			if (const auto* extension = std::get_if<ast::ExtendRegionDecl>(&declaration)) {
				const auto extensionId = findSymbol(SymbolCategory::RegionExtension, extension->name.text);
				const auto targetId = findSymbol(SymbolCategory::Region, extension->name.text);
				if (extensionId) {
					for (auto& symbol : index.symbols_) {
						if (symbol.id == *extensionId) {
							symbol.container = targetId;
							break;
						}
					}
				}
				index.occurrences_.push_back({targetId, extension->name.span,
					targetId ? OccurrenceKind::ExtensionTarget : OccurrenceKind::Unresolved});
			}
		}
	}
	auto addType = [&](const ast::Expr& expression) {
		if (const auto type = project.getType(&expression)) {
			const auto enumName = project.getEnumType(&expression);
			index.types_.push_back({expression.span, *type,
				enumName ? std::optional<std::string>(*enumName) : std::nullopt});
		}
	};
	std::function<void(const ast::Expr&)> indexExpression;
	indexExpression = [&](const ast::Expr& expression) {
		addType(expression);
		std::visit([&](const auto& node) {
			using T = std::decay_t<decltype(node)>;
			if constexpr (std::is_same_v<T, ast::Identifier>) {
				std::optional<SymbolId> target;
				OccurrenceKind kind = OccurrenceKind::Unresolved;
				if (node.kind == ast::IdentifierKind::FunctionRef) {
					target = findSymbol(SymbolCategory::Define, node.name.text);
					if (!target) target = findSymbol(SymbolCategory::ExternDefine, node.name.text);
					kind = target ? OccurrenceKind::Reference : OccurrenceKind::Unresolved;
				} else if (node.kind == ast::IdentifierKind::EnumValue) {
					const auto enumName = project.getEnumType(&expression);
					if (enumName) {
						const auto enumId = findSymbol(SymbolCategory::Enum, *enumName);
						if (enumId) {
							for (const auto& symbol : index.symbols_) {
								if (symbol.container == enumId && symbol.displayName == node.name.text) {
									target = symbol.id;
									break;
								}
							}
						}
					}
					kind = target ? OccurrenceKind::Reference : OccurrenceKind::Unresolved;
				}
				index.occurrences_.push_back({target, node.name.span, kind});
			} else if constexpr (std::is_same_v<T, ast::MemberExpr>) {
				const auto enumId = findSymbol(SymbolCategory::Enum, node.object.text);
				index.occurrences_.push_back({enumId, node.object.span,
					enumId ? OccurrenceKind::Reference : OccurrenceKind::Unresolved});
				std::optional<SymbolId> memberId;
				if (enumId) {
					for (const auto& symbol : index.symbols_) {
						if (symbol.container == enumId && symbol.displayName == node.member.text) {
							memberId = symbol.id;
							break;
						}
					}
				}
				index.occurrences_.push_back({memberId, node.member.span,
					memberId ? OccurrenceKind::MemberAccess : OccurrenceKind::Unresolved});
			} else if constexpr (std::is_same_v<T, ast::UnaryExpr>) {
				indexExpression(*node.operand);
			} else if constexpr (std::is_same_v<T, ast::BinaryExpr>) {
				indexExpression(*node.left);
				indexExpression(*node.right);
			} else if constexpr (std::is_same_v<T, ast::TernaryExpr>) {
				indexExpression(*node.condition);
				indexExpression(*node.thenBranch);
				indexExpression(*node.elseBranch);
			} else if constexpr (std::is_same_v<T, ast::CallExpr>) {
				auto target = findSymbol(SymbolCategory::Define, node.callee.text);
				if (!target) target = findSymbol(SymbolCategory::ExternDefine, node.callee.text);
				index.occurrences_.push_back({target, node.callee.span,
					target ? OccurrenceKind::Call : OccurrenceKind::Unresolved});
				CallRecord call{expression.span, target, {}, {}};
				const auto* normalized = project.getResolvedCallArgs(&node);
				for (const auto& argument : node.args) {
					call.argumentRanges.push_back(argument.value->span);
					std::optional<size_t> binding;
					if (normalized) {
						for (size_t indexValue = 0; indexValue < normalized->size(); ++indexValue) {
							if ((*normalized)[indexValue] == argument.value.get()) binding = indexValue;
						}
					}
					call.normalizedBindings.push_back(binding);
					indexExpression(*argument.value);
				}
				index.calls_.push_back(std::move(call));
			} else if constexpr (std::is_same_v<T, ast::InvokeExpr>) {
				indexExpression(*node.callee);
			} else if constexpr (std::is_same_v<T, ast::MatchExpr>) {
				indexExpression(*node.discriminant);
				for (const auto& arm : node.arms) {
					for (const auto& pattern : arm.patterns) indexExpression(*pattern);
					indexExpression(*arm.body);
				}
			} else if constexpr (std::is_same_v<T, ast::ListExpr>) {
				for (const auto& element : node.elements) indexExpression(*element);
			}
		}, expression.node);
	};

	for (const auto& file : project.files) {
		for (const auto& declaration : file.declarations) {
			std::visit([&](const auto& node) {
				using T = std::decay_t<decltype(node)>;
				if constexpr (std::is_same_v<T, ast::DefineDecl>) {
					if (node.body) indexExpression(*node.body);
					for (const auto& parameter : node.params) {
						if (parameter.defaultValue) indexExpression(*parameter.defaultValue);
					}
				} else if constexpr (std::is_same_v<T, ast::RegionDecl>) {
					for (const auto& data : node.body.data) indexExpression(*data.value);
					for (const auto& section : node.body.sections) {
						for (const auto& entry : section.entries) indexExpression(*entry.condition);
					}
				} else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
					for (const auto& section : node.sections) {
						for (const auto& entry : section.entries) indexExpression(*entry.condition);
					}
				}
			}, declaration);
		}
	}
	return index;
}

} // namespace rls::sema