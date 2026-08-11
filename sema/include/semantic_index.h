#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ast.h"

namespace rls::sema {

class SymbolId {
public:
	SymbolId() = default;
	bool operator==(const SymbolId&) const = default;

private:
	uint64_t value_ = 0;

	explicit SymbolId(uint64_t value) : value_(value) {}
	friend class SemanticIndex;
};

enum class SymbolCategory {
	Region,
	RegionExtension,
	Define,
	ExternDefine,
	Enum,
	EnumMember,
	ExternEnumPattern,
	Parameter,
	RegionDataEntry,
	SectionEntry,
};

enum class SymbolProvenance {
	Source,
	Extern,
	Pattern,
};

enum class OccurrenceKind {
	Declaration,
	Reference,
	Call,
	TypeReference,
	MemberAccess,
	ExtensionTarget,
	Unresolved,
};

struct SymbolRecord {
	SymbolId id;
	SymbolCategory category;
	SymbolProvenance provenance;
	std::string displayName;
	ast::Span declaration;
	ast::Span selection;
	std::optional<std::string> signature;
	std::optional<ast::Type> type;
	std::optional<std::string> enumName;
	std::optional<SymbolId> container;
};

struct OccurrenceRecord {
	std::optional<SymbolId> symbol;
	ast::Span span;
	OccurrenceKind kind;
};

struct TypeRecord {
	ast::Span span;
	ast::Type type;
	std::optional<std::string> enumName;
};

struct CallRecord {
	ast::Span span;
	std::optional<SymbolId> target;
	std::vector<ast::Span> argumentRanges;
	std::vector<std::optional<size_t>> normalizedBindings;
};

/// Snapshot-local semantic records that retain no AST pointers.
class SemanticIndex {
public:
	const std::vector<SymbolRecord>& symbols() const { return symbols_; }
	const std::vector<OccurrenceRecord>& occurrences() const { return occurrences_; }
	const std::vector<TypeRecord>& types() const { return types_; }
	const std::vector<CallRecord>& calls() const { return calls_; }
	std::optional<SymbolRecord> declaration(SymbolId id) const;
	std::vector<OccurrenceRecord> occurrencesFor(SymbolId id) const;
	std::optional<OccurrenceRecord> occurrenceAt(std::string_view file, ast::Position position) const;
	std::optional<TypeRecord> typeAt(std::string_view file, ast::Position position) const;
	std::optional<CallRecord> callAt(std::string_view file, ast::Position position) const;
	std::vector<SymbolId> visibleSymbolsAt(std::string_view file, ast::Position position) const;

private:
	std::vector<SymbolRecord> symbols_;
	std::vector<OccurrenceRecord> occurrences_;
	std::vector<TypeRecord> types_;
	std::vector<CallRecord> calls_;

	SymbolId addSymbol(SymbolCategory category, SymbolProvenance provenance,
		std::string displayName, ast::Span declaration, ast::Span selection,
		std::optional<SymbolId> container = std::nullopt,
		std::optional<std::string> signature = std::nullopt,
		std::optional<ast::Type> type = std::nullopt,
		std::optional<std::string> enumName = std::nullopt);

	friend SemanticIndex buildSemanticIndex(const ast::Project& project);
};

SemanticIndex buildSemanticIndex(const ast::Project& project);

} // namespace rls::sema