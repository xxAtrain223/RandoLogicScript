#pragma once

#include "ast.h"

#include <optional>
#include <string_view>
#include <vector>

namespace rls::parser {

enum class SyntaxRecoveryStatus {
	Complete,
	Recovered,
};

struct EditorEnumDeclaration {
	ast::Name name;
	ast::Span span;
	SyntaxRecoveryStatus status = SyntaxRecoveryStatus::Recovered;
};

struct EditorMemberAccess {
	ast::Name object;
	ast::Span span;
	ast::Span memberSpan;
	SyntaxRecoveryStatus status = SyntaxRecoveryStatus::Recovered;
};

enum class EditorTypePositionKind {
	Parameter,
	Return,
};

struct EditorTypePosition {
	EditorTypePositionKind kind;
	ast::Span span;
	SyntaxRecoveryStatus status = SyntaxRecoveryStatus::Recovered;
};

struct EditorCallArgument {
	std::optional<ast::Name> label;
	ast::Span labelSpan;
	ast::Span valueSpan;
	bool labelCandidate = false;
};

struct EditorCall {
	ast::Name callee;
	ast::Span span;
	std::vector<EditorCallArgument> arguments;
	SyntaxRecoveryStatus status = SyntaxRecoveryStatus::Recovered;
};

struct EditorSectionEntry {
	std::optional<ast::Name> name;
	ast::Span labelSpan;
};

struct EditorRegionSection {
	ast::SectionKind kind;
	ast::Span span;
	std::vector<EditorSectionEntry> entries;
};

struct EditorRegion {
	ast::Name name;
	ast::Span span;
	bool extension = false;
	std::vector<std::string> dataKeys;
	std::vector<EditorRegionSection> sections;
	SyntaxRecoveryStatus status = SyntaxRecoveryStatus::Recovered;
};

struct EditorDeclaration {
	ast::Span span;
};

struct EditorSyntax {
	std::vector<EditorDeclaration> declarations;
	std::vector<EditorEnumDeclaration> enumDeclarations;
	std::vector<EditorMemberAccess> memberAccesses;
	std::vector<EditorTypePosition> typePositions;
	std::vector<EditorCall> calls;
	std::vector<EditorRegion> regions;
};

EditorSyntax ParseEditorSyntax(
	const ast::SourceText& source, std::string_view filename,
	const ast::File& parsedFile);

void ClassifyEditorSyntax(EditorSyntax& syntax, const ast::File& parsedFile);

} // namespace rls::parser