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

struct EditorSyntax {
	std::vector<EditorEnumDeclaration> enumDeclarations;
	std::vector<EditorMemberAccess> memberAccesses;
	std::vector<EditorTypePosition> typePositions;
	std::vector<EditorCall> calls;
};

EditorSyntax ParseEditorSyntax(
	const ast::SourceText& source, std::string_view filename,
	const ast::File& parsedFile);

} // namespace rls::parser