#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ast.h"

namespace rls::parser {

enum class SyntaxKind {
	Declaration,
	Name,
	Expression,
	Call,
	Argument,
	RegionData,
	Section,
	Entry,
};

enum class SourceNameKind {
	Declaration,
	Parameter,
	Type,
	Identifier,
	MemberObject,
	Member,
	CallCallee,
	ArgumentLabel,
	RegionDataKey,
	Entry,
	EnumMember,
};

struct SyntaxContext {
	SyntaxKind kind;
	ast::Span span;
};

struct SourceNameContext {
	SourceNameKind kind;
	std::string text;
	ast::Span span;
};

struct LogicalOperatorContext {
	ast::Span span;
};

struct CallContext {
	std::string calleeName;
	ast::Span span;
	ast::Span callee;
	std::vector<ast::Span> argumentRanges;
	std::vector<std::optional<ast::Span>> argumentLabels;
	std::vector<std::optional<std::string>> argumentLabelNames;
	std::optional<size_t> activeArgument;
};

struct RegionSectionContext {
	ast::SectionKind kind;
	ast::Span span;
	std::vector<std::string> entryNames;
};

struct RegionContext {
	ast::Span span;
	std::string name;
	bool extension = false;
	std::vector<std::string> dataKeys;
	std::vector<ast::SectionKind> sectionKinds;
	std::optional<ast::SectionKind> activeSection;
	std::vector<std::string> activeSectionEntries;
};

struct ExtensionTargetContext {
	ast::Span targetSpan;
};

struct SectionEntryContext {
	ast::SectionKind kind;
	ast::Span labelSpan;
};

struct TypePositionContext {
	ast::Span typeSpan;
};

struct MemberAccessContext {
	std::string object;
	ast::Span memberSpan;
};

struct NamedArgumentContext {
	std::string callee;
	std::vector<std::optional<std::string>> argumentLabels;
	size_t activeArgument = 0;
	ast::Span labelSpan;
};

struct CallArgumentContext {
	std::string callee;
	std::vector<std::optional<std::string>> argumentLabels;
	size_t activeArgument = 0;
	ast::Span valueSpan;
};

/// A value-only cursor index built from trustworthy parser spans.
class SourceIndex {
public:
	std::optional<SyntaxContext> syntaxAt(ast::Position position) const;
	std::optional<SourceNameContext> nameAt(ast::Position position) const;
	std::optional<SyntaxContext> enclosingExpression(ast::Position position) const;
	std::optional<CallContext> enclosingCall(ast::Position position) const;
	std::optional<RegionContext> regionContextAt(ast::Position position) const;
	std::optional<ExtensionTargetContext> extensionTargetAt(ast::Position position) const;
	std::optional<MemberAccessContext> memberAccessAt(ast::Position position) const;
	std::optional<NamedArgumentContext> namedArgumentAt(ast::Position position) const;
	std::optional<CallArgumentContext> callArgumentAt(ast::Position position) const;
	std::optional<SectionEntryContext> sectionEntryAt(ast::Position position) const;
	std::optional<TypePositionContext> typePositionAt(ast::Position position) const;
	std::vector<std::string> sectionEntryNames(
		ast::SectionKind kind,
		std::optional<std::string_view> regionName = std::nullopt) const;
	std::vector<std::string> regionNames() const;
	const std::vector<std::string>& enumNames() const { return enumNames_; }
	const std::vector<LogicalOperatorContext>& logicalOperators() const { return logicalOperators_; }
	const std::vector<ast::Span>& booleanLiterals() const { return booleanLiterals_; }
	const std::vector<SyntaxContext>& declarations() const { return declarations_; }
	std::vector<SyntaxContext> declarationsIn(std::string_view file) const;

	// Internal parser construction operations.
	void addSyntax(SyntaxKind kind, const ast::Span& span);
	void addName(SourceNameKind kind, const ast::Name& name);
	void addExpression(const ast::Span& span);
	void addLogicalOperator(LogicalOperatorContext context);
	void addBooleanLiteral(const ast::Span& span);
	void addCall(CallContext call);
	void addDeclaration(const ast::Span& span);
	void addRegionContext(RegionContext context, std::vector<RegionSectionContext> sections);
	void addExtensionTarget(ExtensionTargetContext context);
	void addMemberAccess(MemberAccessContext context);
	void addNamedArgument(NamedArgumentContext context);
	void addCallArgument(CallArgumentContext context);
	void addSectionEntry(SectionEntryContext context);
	void addTypePosition(TypePositionContext context);
	void addEnumName(std::string name);

private:
	std::vector<SyntaxContext> syntax_;
	std::vector<SourceNameContext> names_;
	std::vector<SyntaxContext> expressions_;
	std::vector<LogicalOperatorContext> logicalOperators_;
	std::vector<ast::Span> booleanLiterals_;
	std::vector<CallContext> calls_;
	std::vector<SyntaxContext> declarations_;
	struct IndexedRegionContext {
		RegionContext context;
		std::vector<RegionSectionContext> sections;
	};
	std::vector<IndexedRegionContext> regionContexts_;
	std::vector<ExtensionTargetContext> extensionTargets_;
	std::vector<MemberAccessContext> memberAccesses_;
	std::vector<NamedArgumentContext> namedArguments_;
	std::vector<CallArgumentContext> callArguments_;
	std::vector<SectionEntryContext> sectionEntries_;
	std::vector<TypePositionContext> typePositions_;
	std::vector<std::string> enumNames_;
};

SourceIndex BuildSourceIndex(const ast::File& file, const ast::SourceText* source = nullptr);

} // namespace rls::parser