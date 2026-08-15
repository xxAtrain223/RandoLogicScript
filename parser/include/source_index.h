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

struct CallContext {
	ast::Span span;
	ast::Span callee;
	std::vector<ast::Span> argumentRanges;
	std::vector<std::optional<ast::Span>> argumentLabels;
	std::optional<size_t> activeArgument;
};

struct RegionSectionContext {
	ast::SectionKind kind;
	ast::Span span;
};

struct RegionContext {
	ast::Span span;
	bool extension = false;
	std::vector<std::string> dataKeys;
	std::vector<ast::SectionKind> sectionKinds;
	std::optional<ast::SectionKind> activeSection;
};

/// A value-only cursor index built from trustworthy parser spans.
class SourceIndex {
public:
	std::optional<SyntaxContext> syntaxAt(ast::Position position) const;
	std::optional<SourceNameContext> nameAt(ast::Position position) const;
	std::optional<SyntaxContext> enclosingExpression(ast::Position position) const;
	std::optional<CallContext> enclosingCall(ast::Position position) const;
	std::optional<RegionContext> regionContextAt(ast::Position position) const;
	const std::vector<SyntaxContext>& declarations() const { return declarations_; }
	std::vector<SyntaxContext> declarationsIn(std::string_view file) const;

	// Internal parser construction operations.
	void addSyntax(SyntaxKind kind, const ast::Span& span);
	void addName(SourceNameKind kind, const ast::Name& name);
	void addExpression(const ast::Span& span);
	void addCall(CallContext call);
	void addDeclaration(const ast::Span& span);
	void addRegionContext(RegionContext context, std::vector<RegionSectionContext> sections);

private:
	std::vector<SyntaxContext> syntax_;
	std::vector<SourceNameContext> names_;
	std::vector<SyntaxContext> expressions_;
	std::vector<CallContext> calls_;
	std::vector<SyntaxContext> declarations_;
	struct IndexedRegionContext {
		RegionContext context;
		std::vector<RegionSectionContext> sections;
	};
	std::vector<IndexedRegionContext> regionContexts_;
};

SourceIndex BuildSourceIndex(const ast::File& file, const ast::SourceText* source = nullptr);

} // namespace rls::parser