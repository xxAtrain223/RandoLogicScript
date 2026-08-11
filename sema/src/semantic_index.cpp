#include "semantic_index.h"

#include <type_traits>

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
	return index;
}

} // namespace rls::sema