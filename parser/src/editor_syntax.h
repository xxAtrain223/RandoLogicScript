#pragma once

#include "ast.h"

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

struct EditorSyntax {
	std::vector<EditorEnumDeclaration> enumDeclarations;
	std::vector<EditorMemberAccess> memberAccesses;
};

EditorSyntax ParseEditorSyntax(
	const ast::SourceText& source, std::string_view filename,
	const ast::File& parsedFile);

} // namespace rls::parser