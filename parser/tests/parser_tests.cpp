#include <gtest/gtest.h>
#include <fstream>
#include <stdexcept>

#include "ast.h"
#include "editor_syntax.h"
#include "parser.h"

using namespace rls::ast;

// == Helpers ==================================================================

/// Parse a single-declaration source and return the File.
static File parse(const std::string& src) {
	return rls::parser::ParseString(src);
}

/// Parse a source that should contain exactly one declaration and return it.
static const Decl& parseDecl(const std::string& src) {
	static File holder;
	holder = parse(src);
	EXPECT_EQ(holder.declarations.size(), 1u);
	return holder.declarations[0];
}

/// Convenience: parse a define-wrapped expression and return the body Expr.
/// Wraps the expression in `define _(): <expr>` so the parser produces an AST.
static const Expr& parseExpr(const std::string& exprSrc) {
	static File holder;
	holder = parse("define _(): " + exprSrc);
	const auto& def = std::get<DefineDecl>(holder.declarations[0]);
	return *def.body;
}

static void expectSameSpan(const Span& strict, const Span& editor) {
	EXPECT_EQ(strict.file, editor.file);
	EXPECT_EQ(strict.start.line, editor.start.line);
	EXPECT_EQ(strict.start.column, editor.start.column);
	EXPECT_EQ(strict.end.line, editor.end.line);
	EXPECT_EQ(strict.end.column, editor.end.column);
}

// == Basic parsing ============================================================

TEST(ParserTests, ReturnsEmptyFileForEmptySource) {
	const auto file = rls::parser::ParseString("");

	EXPECT_TRUE(file.declarations.empty());
	EXPECT_TRUE(file.diagnostics.empty());
}

TEST(ParserTests, InvalidSourceReportsDiagnostic) {
	const auto file = rls::parser::ParseString("not valid rls at all ^^^");

	EXPECT_TRUE(file.declarations.empty());
	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected declaration or end of file");
	EXPECT_EQ(file.diagnostics[0].span.start.line, 1u);
	EXPECT_EQ(file.diagnostics[0].span.start.column, 1u);
}

TEST(ParserTests, StrictModeDoesNotSkipTopLevelStrings) {
	const auto file = rls::parser::ParseString("\"enum Fake { VALUE }\"");

	EXPECT_TRUE(file.declarations.empty());
	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].message, "expected declaration or end of file");
}

TEST(ParserTests, MissingIdentifierAfterDefine) {
	const auto file = rls::parser::ParseString("define 123");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected identifier");
}

TEST(ParserTests, MissingOpenParenInDefine) {
	const auto file = rls::parser::ParseString("define foo:");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected '('");
}

TEST(ParserTests, MissingCloseParenInDefine) {
	const auto file = rls::parser::ParseString("define foo(");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected ')'");
}

TEST(ParserTests, MissingColonInDefine) {
	const auto file = rls::parser::ParseString("define foo() true");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected ':'");
}

TEST(ParserTests, MissingExprInDefine) {
	const auto file = rls::parser::ParseString("define foo():");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected expression");
}

TEST(ParserTests, MissingCloseBraceInRegion) {
	const auto file = rls::parser::ParseString("region RR_TEST { name: \"Test\" scene: SCENE_TEST");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected '}'");
}

TEST(ParserTests, ErrorPositionIsAccurate) {
	const auto file = rls::parser::ParseString("define foo() true");
	//                                          1234567890123456
	//                                                        ^ col 14 (the 't' of true)

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].span.start.line, 1u);
	EXPECT_EQ(file.diagnostics[0].span.start.column, 14u);
}

TEST(ParserTests, MultilineErrorPosition) {
	const auto file = rls::parser::ParseString(
		"define foo():\n"
		"    true\n"
		"invalid");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].span.start.line, 3u);
	EXPECT_EQ(file.diagnostics[0].span.start.column, 1u);
	EXPECT_EQ(file.diagnostics[0].message, "expected declaration or end of file");
}

TEST(ParserTests, TrailingOrInMatchExpr) {
	const auto file = rls::parser::ParseString(
		"define _():\n"
		"    match x {\n"
		"        A: true or\n"
		"    }");

	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "trailing 'or' without a following match arm");
}

TEST(ParserTests, ValidSourceReturnsFile) {
	const auto file =
		rls::parser::ParseString("region RR_TEST { name: \"Test\" scene: SCENE_TEST }");

	EXPECT_TRUE(file.diagnostics.empty());
	ASSERT_EQ(file.declarations.size(), 1u);
	const auto* region =
		std::get_if<rls::ast::RegionDecl>(&file.declarations[0]);
	ASSERT_NE(region, nullptr);
	EXPECT_EQ(region->key, "RR_TEST");
	const auto* scene = region->body.findData("scene");
	ASSERT_NE(scene, nullptr);
	EXPECT_EQ(std::get<rls::ast::Identifier>(scene->value->node).name, "SCENE_TEST");
}

TEST(ParserTests, EditorModeMatchesStrictModeForValidSource) {
	const std::string source =
		"define check(target: Item): can_kill(quantity: target, 2)\n"
		"region RR_TEST { events { EVENT_TEST: true } }\n"
		"enum Color { RED, BLUE }";
	const auto strict = rls::parser::ParseStringWithIndex(
		source, "parity.rls", rls::parser::ParseMode::Strict);
	const auto editor = rls::parser::ParseStringWithIndex(
		source, "parity.rls", rls::parser::ParseMode::Editor);

	EXPECT_TRUE(strict.file.diagnostics.empty());
	EXPECT_TRUE(editor.file.diagnostics.empty());
	ASSERT_EQ(strict.file.declarations.size(), editor.file.declarations.size());
	ASSERT_EQ(strict.file.declarations.size(), 3u);

	const auto& strictDefine = std::get<DefineDecl>(strict.file.declarations[0]);
	const auto& editorDefine = std::get<DefineDecl>(editor.file.declarations[0]);
	EXPECT_EQ(strictDefine.name.text, editorDefine.name.text);
	expectSameSpan(strictDefine.name.span, editorDefine.name.span);
	ASSERT_EQ(strictDefine.params.size(), editorDefine.params.size());
	EXPECT_EQ(strictDefine.params[0].name.text, editorDefine.params[0].name.text);
	expectSameSpan(strictDefine.params[0].name.span, editorDefine.params[0].name.span);

	const auto& strictRegion = std::get<RegionDecl>(strict.file.declarations[1]);
	const auto& editorRegion = std::get<RegionDecl>(editor.file.declarations[1]);
	EXPECT_EQ(strictRegion.key.text, editorRegion.key.text);
	expectSameSpan(strictRegion.key.span, editorRegion.key.span);

	ASSERT_EQ(strict.sourceIndex.declarations().size(), editor.sourceIndex.declarations().size());
	for (size_t index = 0; index < strict.sourceIndex.declarations().size(); ++index) {
		EXPECT_EQ(strict.sourceIndex.declarations()[index].kind,
			editor.sourceIndex.declarations()[index].kind);
		expectSameSpan(strict.sourceIndex.declarations()[index].span,
			editor.sourceIndex.declarations()[index].span);
	}

	const auto strictName = strict.sourceIndex.nameAt({1, 30});
	const auto editorName = editor.sourceIndex.nameAt({1, 30});
	ASSERT_TRUE(strictName);
	ASSERT_TRUE(editorName);
	EXPECT_EQ(strictName->kind, editorName->kind);
	EXPECT_EQ(strictName->text, editorName->text);
	expectSameSpan(strictName->span, editorName->span);

	const auto strictCall = strict.sourceIndex.enclosingCall({1, 49});
	const auto editorCall = editor.sourceIndex.enclosingCall({1, 49});
	ASSERT_TRUE(strictCall);
	ASSERT_TRUE(editorCall);
	EXPECT_EQ(strictCall->activeArgument, editorCall->activeArgument);
	ASSERT_EQ(strictCall->argumentRanges.size(), editorCall->argumentRanges.size());
	for (size_t index = 0; index < strictCall->argumentRanges.size(); ++index) {
		expectSameSpan(strictCall->argumentRanges[index], editorCall->argumentRanges[index]);
	}

	const auto strictRegionContext = strict.sourceIndex.regionContextAt({2, 32});
	const auto editorRegionContext = editor.sourceIndex.regionContextAt({2, 32});
	ASSERT_TRUE(strictRegionContext);
	ASSERT_TRUE(editorRegionContext);
	EXPECT_EQ(strictRegionContext->name, editorRegionContext->name);
	EXPECT_EQ(strictRegionContext->activeSection, editorRegionContext->activeSection);
	EXPECT_EQ(strictRegionContext->activeSectionEntries,
		editorRegionContext->activeSectionEntries);
	EXPECT_EQ(strict.sourceIndex.enumNames(), editor.sourceIndex.enumNames());
	EXPECT_EQ(strict.sourceIndex.enumNames(), std::vector<std::string>{"Color"});
}

TEST(ParserTests, EditorModeMatchesStrictModeAcrossExamples) {
	const auto examples = std::filesystem::path(RLS_REPO_ROOT) / "examples";
	ASSERT_TRUE(std::filesystem::is_directory(examples));
	size_t fileCount = 0;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(examples)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".rls") continue;
		++fileCount;
		std::ifstream stream(entry.path(), std::ios::binary);
		ASSERT_TRUE(stream) << entry.path();
		const std::string source{
			std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>()};
		const auto filename = entry.path().generic_string();
		const auto strict = rls::parser::ParseStringWithIndex(
			source, filename, rls::parser::ParseMode::Strict);
		const auto editor = rls::parser::ParseStringWithIndex(
			source, filename, rls::parser::ParseMode::Editor);

		EXPECT_TRUE(strict.file.diagnostics.empty()) << entry.path();
		EXPECT_TRUE(editor.file.diagnostics.empty()) << entry.path();
		EXPECT_EQ(strict.file.declarations.size(), editor.file.declarations.size())
			<< entry.path();
		ASSERT_EQ(strict.sourceIndex.declarations().size(),
			editor.sourceIndex.declarations().size()) << entry.path();
		for (size_t index = 0; index < strict.sourceIndex.declarations().size(); ++index) {
			expectSameSpan(strict.sourceIndex.declarations()[index].span,
				editor.sourceIndex.declarations()[index].span);
		}
		EXPECT_EQ(strict.sourceIndex.regionNames(), editor.sourceIndex.regionNames())
			<< entry.path();
		EXPECT_EQ(strict.sourceIndex.enumNames(), editor.sourceIndex.enumNames())
			<< entry.path();

		const auto sourceText = SourceText::FromUtf8(source);
		ASSERT_TRUE(sourceText) << entry.path();
		for (size_t offset = 0; offset <= source.size(); ++offset) {
			const auto position = sourceText->utf8PositionAtByteOffset(offset);
			ASSERT_TRUE(position) << entry.path() << " at byte " << offset;
			const auto strictName = strict.sourceIndex.nameAt(*position);
			const auto editorName = editor.sourceIndex.nameAt(*position);
			ASSERT_EQ(strictName.has_value(), editorName.has_value())
				<< entry.path() << " at byte " << offset;
			if (strictName && editorName) {
				EXPECT_EQ(strictName->kind, editorName->kind);
				EXPECT_EQ(strictName->text, editorName->text);
				expectSameSpan(strictName->span, editorName->span);
			}
			const auto strictSyntax = strict.sourceIndex.syntaxAt(*position);
			const auto editorSyntax = editor.sourceIndex.syntaxAt(*position);
			ASSERT_EQ(strictSyntax.has_value(), editorSyntax.has_value())
				<< entry.path() << " at byte " << offset;
			if (strictSyntax && editorSyntax) {
				EXPECT_EQ(strictSyntax->kind, editorSyntax->kind);
				expectSameSpan(strictSyntax->span, editorSyntax->span);
			}
		}
	}
	EXPECT_GT(fileCount, 0u);
}

TEST(ParserTests, EditorModeKeepsCompleteDeclarationsAroundMalformedSyntax) {
	const std::string source =
		"define before(): true\n"
		"define broken(\n"
		"define after(): before()\n";
	const auto editor = rls::parser::ParseStringWithIndex(
		source, "partial.rls", rls::parser::ParseMode::Editor);
	const auto strict = rls::parser::ParseStringWithIndex(
		source, "partial.rls", rls::parser::ParseMode::Strict);

	ASSERT_FALSE(editor.file.diagnostics.empty());
	ASSERT_EQ(editor.file.declarations.size(), 2u);
	EXPECT_EQ(std::get<DefineDecl>(editor.file.declarations[0]).name, "before");
	EXPECT_EQ(std::get<DefineDecl>(editor.file.declarations[1]).name, "after");
	EXPECT_EQ(std::get<DefineDecl>(editor.file.declarations[1]).name.span.start.line, 3u);
	EXPECT_TRUE(editor.sourceIndex.nameAt({1, 8}));
	EXPECT_TRUE(editor.sourceIndex.nameAt({3, 8}));
	EXPECT_FALSE(editor.sourceIndex.nameAt({2, 8}));

	EXPECT_FALSE(strict.file.diagnostics.empty());
	EXPECT_TRUE(strict.file.declarations.empty());
}

TEST(ParserTests, EditorModeSynchronizesAfterMalformedConstructs) {
	const std::vector<std::string> sources = {
		"define broken(\ndefine after(): true\n",
		"enum Broken {\ndefine after(): true\n",
		"region RR_BROKEN { events { EVENT_PARTIAL\ndefine after(): true\n",
		"define broken(): target(\ndefine after(): true\n",
		"define broken(): true ?\ndefine after(): true\n",
	};

	for (const auto& source : sources) {
		SCOPED_TRACE(source);
		const auto parsed = rls::parser::ParseStringWithIndex(
			source, "synchronization.rls", rls::parser::ParseMode::Editor);
		ASSERT_FALSE(parsed.file.diagnostics.empty());
		ASSERT_EQ(parsed.file.declarations.size(), 1u);
		const auto* define = std::get_if<DefineDecl>(&parsed.file.declarations[0]);
		ASSERT_NE(define, nullptr);
		EXPECT_EQ(define->name, "after");
		EXPECT_EQ(define->name.span.start.line, 2u);
	}
}

TEST(ParserTests, EditorSyntaxClassifiesCompleteAndRecoveredEnums) {
	const auto completeSource = SourceText::FromUtf8("enum Color { RED }");
	ASSERT_TRUE(completeSource);
	const auto completeFile = rls::parser::ParseString(
		completeSource->content(), "complete.rls");
	const auto complete = rls::parser::ParseEditorSyntax(
		*completeSource, "complete.rls", completeFile);
	ASSERT_EQ(complete.enumDeclarations.size(), 1u);
	EXPECT_EQ(complete.enumDeclarations[0].name.text, "Color");
	EXPECT_EQ(complete.enumDeclarations[0].status,
		rls::parser::SyntaxRecoveryStatus::Complete);

	const auto recoveredSource = SourceText::FromUtf8("enum Color {");
	ASSERT_TRUE(recoveredSource);
	const auto recoveredFile = rls::parser::ParseString(
		recoveredSource->content(), "recovered.rls");
	const auto recovered = rls::parser::ParseEditorSyntax(
		*recoveredSource, "recovered.rls", recoveredFile);
	ASSERT_EQ(recovered.enumDeclarations.size(), 1u);
	EXPECT_EQ(recovered.enumDeclarations[0].name.text, "Color");
	EXPECT_EQ(recovered.enumDeclarations[0].status,
		rls::parser::SyntaxRecoveryStatus::Recovered);
	EXPECT_EQ(recovered.enumDeclarations[0].span.start.column, 1u);
	EXPECT_EQ(recovered.enumDeclarations[0].span.end.column, 11u);
}

TEST(ParserTests, EditorSyntaxRecoversMemberAccesses) {
	const auto source = SourceText::FromUtf8(
		"define first(): Color.RED\n"
		"define second(): Color.\n"
		"define ignored(): \"Quoted.FAKE\" # Commented.FAKE\n");
	ASSERT_TRUE(source);
	const auto syntax = rls::parser::ParseEditorSyntax(
		*source, "members.rls", File{});
	ASSERT_EQ(syntax.memberAccesses.size(), 2u);
	EXPECT_EQ(syntax.memberAccesses[0].object.text, "Color");
	EXPECT_EQ(syntax.memberAccesses[0].memberSpan.start.column, 23u);
	EXPECT_EQ(syntax.memberAccesses[0].memberSpan.end.column, 26u);
	EXPECT_EQ(syntax.memberAccesses[0].status,
		rls::parser::SyntaxRecoveryStatus::Complete);
	EXPECT_EQ(syntax.memberAccesses[1].object.text, "Color");
	EXPECT_EQ(syntax.memberAccesses[1].memberSpan.start.column, 24u);
	EXPECT_EQ(syntax.memberAccesses[1].memberSpan.end.column, 24u);
	EXPECT_EQ(syntax.memberAccesses[1].status,
		rls::parser::SyntaxRecoveryStatus::Recovered);
}

TEST(ParserTests, EditorSyntaxRecoversFunctionTypePositions) {
	const auto source = SourceText::FromUtf8(
		"# define hidden(value: Fake)\n"
		"\"extern define hidden() -> Fake\"\n"
		"define choose(first = nested(a, b), second: Col\n"
		"extern define convert(value: Bool) -> \n"
		"define ignored(value = true ? false : true\n");
	ASSERT_TRUE(source);
	const auto syntax = rls::parser::ParseEditorSyntax(
		*source, "types.rls", File{});
	ASSERT_EQ(syntax.typePositions.size(), 3u);
	EXPECT_EQ(syntax.typePositions[0].kind,
		rls::parser::EditorTypePositionKind::Parameter);
	EXPECT_EQ(syntax.typePositions[0].status,
		rls::parser::SyntaxRecoveryStatus::Complete);
	EXPECT_EQ(syntax.typePositions[1].kind,
		rls::parser::EditorTypePositionKind::Parameter);
	EXPECT_EQ(syntax.typePositions[1].status,
		rls::parser::SyntaxRecoveryStatus::Complete);
	EXPECT_EQ(syntax.typePositions[2].kind,
		rls::parser::EditorTypePositionKind::Return);
	EXPECT_EQ(syntax.typePositions[2].status,
		rls::parser::SyntaxRecoveryStatus::Recovered);
}

TEST(ParserTests, WhitespaceOnlyReturnsEmpty) {
	const auto file = parse("  \n\n  ");
	EXPECT_TRUE(file.declarations.empty());
}

TEST(ParserTests, CommentOnlyReturnsEmpty) {
	const auto file = parse("# comment\n");
	EXPECT_TRUE(file.declarations.empty());
}

// == Expression leaf nodes ====================================================

TEST(ParseExpr, BoolTrue) {
	const auto& e = parseExpr("true");
	ASSERT_TRUE(std::holds_alternative<BoolLiteral>(e.node));
	EXPECT_TRUE(std::get<BoolLiteral>(e.node).value);
}

TEST(ParseExpr, BoolFalse) {
	const auto& e = parseExpr("false");
	ASSERT_TRUE(std::holds_alternative<BoolLiteral>(e.node));
	EXPECT_FALSE(std::get<BoolLiteral>(e.node).value);
}

TEST(ParseExpr, BoolAlways) {
	const auto& e = parseExpr("always");
	ASSERT_TRUE(std::holds_alternative<BoolLiteral>(e.node));
	EXPECT_TRUE(std::get<BoolLiteral>(e.node).value);
}

TEST(ParseExpr, BoolNever) {
	const auto& e = parseExpr("never");
	ASSERT_TRUE(std::holds_alternative<BoolLiteral>(e.node));
	EXPECT_FALSE(std::get<BoolLiteral>(e.node).value);
}

TEST(ParseExpr, Integer) {
	const auto& e = parseExpr("42");
	ASSERT_TRUE(std::holds_alternative<IntLiteral>(e.node));
	EXPECT_EQ(std::get<IntLiteral>(e.node).value, 42);
}

TEST(ParseExpr, NegativeInteger) {
	const auto& e = parseExpr("-7");
	ASSERT_TRUE(std::holds_alternative<IntLiteral>(e.node));
	EXPECT_EQ(std::get<IntLiteral>(e.node).value, -7);
}

TEST(ParseExpr, Identifier) {
	const auto& e = parseExpr("RG_HOOKSHOT");
	ASSERT_TRUE(std::holds_alternative<Identifier>(e.node));
	const auto& ident = std::get<Identifier>(e.node);
	EXPECT_EQ(ident.name, "RG_HOOKSHOT");
	EXPECT_EQ(ident.name.span.start.line, 1u);
	EXPECT_EQ(ident.name.span.start.column, 13u);
	EXPECT_EQ(ident.name.span.end.line, 1u);
	EXPECT_EQ(ident.name.span.end.column, 24u);
}

// == Unary expression =========================================================

TEST(ParseExpr, UnaryNot) {
	const auto& e = parseExpr("not true");
	ASSERT_TRUE(std::holds_alternative<UnaryExpr>(e.node));
	const auto& u = std::get<UnaryExpr>(e.node);
	EXPECT_EQ(u.op, UnaryOp::Not);
	EXPECT_TRUE(std::holds_alternative<BoolLiteral>(u.operand->node));
}

TEST(ParseExpr, DoubleNot) {
	const auto& e = parseExpr("not not RG_FOO");
	ASSERT_TRUE(std::holds_alternative<UnaryExpr>(e.node));
	const auto& outer = std::get<UnaryExpr>(e.node);
	ASSERT_TRUE(std::holds_alternative<UnaryExpr>(outer.operand->node));
	const auto& inner = std::get<UnaryExpr>(outer.operand->node);
	EXPECT_TRUE(std::holds_alternative<Identifier>(inner.operand->node));
}

// == Binary expressions =======================================================

TEST(ParseExpr, BinaryAnd) {
	const auto& e = parseExpr("is_child() and RG_HOOKSHOT");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	const auto& bin = std::get<BinaryExpr>(e.node);
	EXPECT_EQ(bin.op, BinaryOp::And);
	EXPECT_TRUE(std::holds_alternative<CallExpr>(bin.left->node));
	EXPECT_TRUE(std::holds_alternative<Identifier>(bin.right->node));
}

TEST(ParseExpr, BinaryOr) {
	const auto& e = parseExpr("true or false");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Or);
}

TEST(ParseExpr, ComparisonEq) {
	const auto& e = parseExpr("x == 1");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Eq);
}

TEST(ParseExpr, ComparisonNotEq) {
	const auto& e = parseExpr("x != 1");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::NotEq);
}

TEST(ParseExpr, ComparisonLt) {
	const auto& e = parseExpr("x < 5");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Lt);
}

TEST(ParseExpr, ComparisonGtEq) {
	const auto& e = parseExpr("x >= 3");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::GtEq);
}

TEST(ParseExpr, ComparisonIs) {
	const auto& e = parseExpr("setting is RG_FOO");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Eq);
}

TEST(ParseExpr, ComparisonIsNot) {
	const auto& e = parseExpr("setting is not RG_FOO");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::NotEq);
}

TEST(ParseExpr, ArithmeticAdd) {
	const auto& e = parseExpr("1 + 2");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Add);
}

TEST(ParseExpr, ArithmeticSub) {
	const auto& e = parseExpr("10 - 3");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Sub);
}

TEST(ParseExpr, ArithmeticMul) {
	const auto& e = parseExpr("2 * 3");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Mul);
}

TEST(ParseExpr, ArithmeticDiv) {
	const auto& e = parseExpr("10 / 2");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	EXPECT_EQ(std::get<BinaryExpr>(e.node).op, BinaryOp::Div);
}

TEST(ParseExpr, MulDivPrecedenceOverAddSub) {
	// 1 + 2 * 3 should be 1 + (2 * 3)
	const auto& e = parseExpr("1 + 2 * 3");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	const auto& add = std::get<BinaryExpr>(e.node);
	EXPECT_EQ(add.op, BinaryOp::Add);
	EXPECT_TRUE(std::holds_alternative<IntLiteral>(add.left->node));
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(add.right->node));
	EXPECT_EQ(std::get<BinaryExpr>(add.right->node).op, BinaryOp::Mul);
}

TEST(ParseExpr, AndOrPrecedence) {
	// a or b and c should be a or (b and c)
	const auto& e = parseExpr("RG_A or RG_B and RG_C");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	const auto& orExpr = std::get<BinaryExpr>(e.node);
	EXPECT_EQ(orExpr.op, BinaryOp::Or);
	EXPECT_TRUE(std::holds_alternative<Identifier>(orExpr.left->node));
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(orExpr.right->node));
	EXPECT_EQ(std::get<BinaryExpr>(orExpr.right->node).op, BinaryOp::And);
}

TEST(ParseExpr, LeftAssociativeChain) {
	// 1 + 2 + 3 should be (1 + 2) + 3
	const auto& e = parseExpr("1 + 2 + 3");
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(e.node));
	const auto& outer = std::get<BinaryExpr>(e.node);
	EXPECT_EQ(outer.op, BinaryOp::Add);
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(outer.left->node));
	const auto& inner = std::get<BinaryExpr>(outer.left->node);
	EXPECT_EQ(inner.op, BinaryOp::Add);
	EXPECT_TRUE(std::holds_alternative<IntLiteral>(outer.right->node));
	EXPECT_EQ(std::get<IntLiteral>(outer.right->node).value, 3);
}

// == Ternary expression =======================================================

TEST(ParseExpr, Ternary) {
	const auto& e = parseExpr("is_adult() ? RG_HOOKSHOT : RG_BOOMERANG");
	ASSERT_TRUE(std::holds_alternative<TernaryExpr>(e.node));
	const auto& t = std::get<TernaryExpr>(e.node);
	EXPECT_TRUE(std::holds_alternative<CallExpr>(t.condition->node));
	EXPECT_TRUE(std::holds_alternative<Identifier>(t.thenBranch->node));
	EXPECT_EQ(std::get<Identifier>(t.thenBranch->node).name, "RG_HOOKSHOT");
	EXPECT_EQ(std::get<Identifier>(t.elseBranch->node).name, "RG_BOOMERANG");
}

TEST(ParseExpr, NestedTernary) {
	// a ? b : c ? d : e  ==>  a ? b : (c ? d : e)
	const auto& e = parseExpr("true ? 1 : false ? 2 : 3");
	ASSERT_TRUE(std::holds_alternative<TernaryExpr>(e.node));
	const auto& outer = std::get<TernaryExpr>(e.node);
	ASSERT_TRUE(std::holds_alternative<TernaryExpr>(outer.elseBranch->node));
}

// == Call expression ==========================================================

TEST(ParseExpr, CallNoArgs) {
	const auto& e = parseExpr("has_explosives()");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	EXPECT_EQ(call.callee.text, "has_explosives");
	EXPECT_TRUE(call.args.empty());
}

TEST(ParseExpr, AnyAgeFunctionCall) {
	const auto& e = parseExpr("any_age(has(RG_HOOKSHOT))");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& c = std::get<CallExpr>(e.node);
	EXPECT_EQ(c.callee.text, "any_age");
	ASSERT_EQ(c.args.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<CallExpr>(c.args[0].value->node));
}

TEST(ParseExpr, InvokeCallResult) {
	const auto& e = parseExpr("make_cond(has(RG_HOOKSHOT))()");
	ASSERT_TRUE(std::holds_alternative<InvokeExpr>(e.node));
	const auto& invoke = std::get<InvokeExpr>(e.node);
	ASSERT_TRUE(std::holds_alternative<CallExpr>(invoke.callee->node));
	const auto& call = std::get<CallExpr>(invoke.callee->node);
	EXPECT_EQ(call.callee.text, "make_cond");
	ASSERT_EQ(call.args.size(), 1u);
}

TEST(ParseExpr, CallSinglePositionalArg) {
	const auto& e = parseExpr("has(RG_HOOKSHOT)");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	EXPECT_EQ(call.callee.text, "has");
	ASSERT_EQ(call.args.size(), 1u);
	EXPECT_FALSE(call.args[0].name.has_value());
	EXPECT_TRUE(std::holds_alternative<Identifier>(call.args[0].value->node));
}

TEST(ParseExpr, CallMultipleArgs) {
	const auto& e = parseExpr("can_kill(RE_ARMOS, ED_CLOSE, false)");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	EXPECT_EQ(call.callee.text, "can_kill");
	ASSERT_EQ(call.args.size(), 3u);
	EXPECT_FALSE(call.args[0].name.has_value());
	EXPECT_FALSE(call.args[1].name.has_value());
	EXPECT_FALSE(call.args[2].name.has_value());
}

TEST(ParseExpr, CallNamedArg) {
	const auto& e = parseExpr("foo(x: 1, y: 2)");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	EXPECT_EQ(call.callee.span.start.line, 1u);
	EXPECT_EQ(call.callee.span.start.column, 13u);
	EXPECT_EQ(call.callee.span.end.column, 16u);
	ASSERT_EQ(call.args.size(), 2u);
	ASSERT_TRUE(call.args[0].name.has_value());
	EXPECT_EQ(*call.args[0].name, "x");
	EXPECT_EQ(call.args[0].name->span.start.column, 17u);
	EXPECT_EQ(call.args[0].name->span.end.column, 18u);
	ASSERT_TRUE(call.args[1].name.has_value());
	EXPECT_EQ(*call.args[1].name, "y");
	EXPECT_EQ(call.args[1].name->span.start.column, 23u);
	EXPECT_EQ(call.args[1].name->span.end.column, 24u);
}

TEST(ParseExpr, CallMixedArgs) {
	const auto& e = parseExpr("can_kill(RE_ARMOS, distance: ED_CLOSE)");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	ASSERT_EQ(call.args.size(), 2u);
	EXPECT_FALSE(call.args[0].name.has_value());
	ASSERT_TRUE(call.args[1].name.has_value());
	EXPECT_EQ(*call.args[1].name, "distance");
}

// == Source index ============================================================

TEST(SourceIndexTests, IndexesDeclarationsNamesExpressionsAndCalls) {
	const auto parsed = rls::parser::ParseStringWithIndex(
		"define check(target: Item): can_kill(quantity: target, 2)\n"
		"region RR_TEST { events { EVENT_TEST: true } }");
	const auto& index = parsed.sourceIndex;

	ASSERT_EQ(index.declarations().size(), 2u);
	EXPECT_EQ(index.declarationsIn("in_memory").size(), 2u);
	EXPECT_TRUE(index.declarationsIn("other.rls").empty());
	const auto declaration = index.nameAt({1, 9});
	ASSERT_TRUE(declaration);
	EXPECT_EQ(declaration->kind, rls::parser::SourceNameKind::Declaration);
	EXPECT_EQ(declaration->text, "check");

	const auto parameter = index.nameAt({1, 15});
	ASSERT_TRUE(parameter);
	EXPECT_EQ(parameter->kind, rls::parser::SourceNameKind::Parameter);
	EXPECT_EQ(parameter->text, "target");
	const auto& define = std::get<DefineDecl>(parsed.file.declarations[0]);
	ASSERT_EQ(define.params.size(), 1u);
	EXPECT_EQ(define.params[0].span.start.column, 14u);
	EXPECT_EQ(define.params[0].span.end.column, 26u);

	const auto type = index.nameAt({1, 23});
	ASSERT_TRUE(type);
	EXPECT_EQ(type->kind, rls::parser::SourceNameKind::Type);
	EXPECT_EQ(type->text, "Item");

	const auto callee = index.nameAt({1, 30});
	ASSERT_TRUE(callee);
	EXPECT_EQ(callee->kind, rls::parser::SourceNameKind::CallCallee);
	EXPECT_EQ(callee->text, "can_kill");

	const auto syntax = index.syntaxAt({1, 30});
	ASSERT_TRUE(syntax);
	EXPECT_EQ(syntax->kind, rls::parser::SyntaxKind::Name);

	const auto label = index.nameAt({1, 39});
	ASSERT_TRUE(label);
	EXPECT_EQ(label->kind, rls::parser::SourceNameKind::ArgumentLabel);
	EXPECT_EQ(label->text, "quantity");

	const auto expression = index.enclosingExpression({1, 49});
	ASSERT_TRUE(expression);
	EXPECT_EQ(expression->kind, rls::parser::SyntaxKind::Expression);

	const auto call = index.enclosingCall({1, 49});
	ASSERT_TRUE(call);
	ASSERT_TRUE(call->activeArgument);
	EXPECT_EQ(*call->activeArgument, 0u);
	EXPECT_EQ(call->argumentRanges.size(), 2u);

	const auto secondArgument = index.enclosingCall({1, 56});
	ASSERT_TRUE(secondArgument);
	ASSERT_TRUE(secondArgument->activeArgument);
	EXPECT_EQ(*secondArgument->activeArgument, 1u);

	const auto& region = std::get<RegionDecl>(parsed.file.declarations[1]);
	ASSERT_EQ(region.body.sections.size(), 1u);
	EXPECT_EQ(region.body.sections[0].span.start.line, 2u);
	EXPECT_EQ(region.body.sections[0].span.start.column, 18u);
	EXPECT_EQ(region.body.sections[0].span.end.line, 2u);
	EXPECT_GT(region.body.sections[0].span.end.column, 18u);

	const auto section = index.syntaxAt({2, 18});
	ASSERT_TRUE(section);
	EXPECT_EQ(section->kind, rls::parser::SyntaxKind::Section);
}

TEST(SourceIndexTests, IgnoresCommentsAndWhitespaceButIndexesStringsAndRecoverySafely) {
	const auto parsed = rls::parser::ParseStringWithIndex(
		"# a source comment\n"
		"define label(): \"value\"\n"
		"\n");
	const auto& index = parsed.sourceIndex;
	EXPECT_FALSE(index.syntaxAt({1, 4}));
	EXPECT_FALSE(index.nameAt({1, 4}));
	EXPECT_FALSE(index.syntaxAt({3, 1}));
	const auto stringExpression = index.enclosingExpression({2, 18});
	ASSERT_TRUE(stringExpression);
	EXPECT_EQ(stringExpression->kind, rls::parser::SyntaxKind::Expression);

	const auto malformed = rls::parser::ParseStringWithIndex("define broken(");
	EXPECT_FALSE(malformed.file.diagnostics.empty());
	EXPECT_TRUE(malformed.sourceIndex.declarations().empty());
	EXPECT_FALSE(malformed.sourceIndex.syntaxAt({1, 8}));
	EXPECT_FALSE(malformed.sourceIndex.nameAt({1, 8}));
}

TEST(SourceIndexTests, ReportsCompleteAndRecoveredRegionContexts) {
	const auto complete = rls::parser::ParseStringWithIndex(
		"region RR_TEST {\n"
		"  name: \"} region RR_FAKE {\"\n"
		"  # locations { FAKE: true }\n"
		"  events { EVENT_TEST: here == here }\n"
		"}\n"
		"extend region RR_TEST { locations {  } }\n",
		"regions.rls");
	const auto region = complete.sourceIndex.regionContextAt({2, 3});
	ASSERT_TRUE(region);
	EXPECT_FALSE(region->extension);
	EXPECT_EQ(region->dataKeys, std::vector<std::string>{"name"});
	EXPECT_EQ(region->sectionKinds,
		std::vector<SectionKind>{SectionKind::Events});
	EXPECT_FALSE(region->activeSection);
	const auto event = complete.sourceIndex.regionContextAt({4, 24});
	ASSERT_TRUE(event);
	EXPECT_EQ(event->activeSection, SectionKind::Events);
	const auto extension = complete.sourceIndex.regionContextAt({6, 36});
	ASSERT_TRUE(extension);
	EXPECT_TRUE(extension->extension);
	EXPECT_EQ(extension->activeSection, SectionKind::Locations);

	const auto recovered = rls::parser::ParseStringWithIndex(
		"region RR_BROKEN {\n"
		"  name: \"Broken\"\n"
		"  loc\n",
		"broken-region.rls", rls::parser::ParseMode::Editor);
	ASSERT_FALSE(recovered.file.diagnostics.empty());
	const auto recoveredRegion = recovered.sourceIndex.regionContextAt({3, 5});
	ASSERT_TRUE(recoveredRegion);
	EXPECT_FALSE(recoveredRegion->extension);
	EXPECT_EQ(recoveredRegion->dataKeys, std::vector<std::string>{"name"});

	const auto recoveredExtension = rls::parser::ParseStringWithIndex(
		"extend region RR_BROKEN { events { EVENT_PARTIAL\n"
		"region RR_NEXT {\n",
		"broken-extension.rls", rls::parser::ParseMode::Editor);
	const auto extensionContext =
		recoveredExtension.sourceIndex.regionContextAt({1, 49});
	ASSERT_TRUE(extensionContext);
	EXPECT_TRUE(extensionContext->extension);
	EXPECT_EQ(extensionContext->activeSection, SectionKind::Events);
	EXPECT_EQ(recoveredExtension.sourceIndex.regionNames(),
		std::vector<std::string>{"RR_NEXT"});

	const auto strict = rls::parser::ParseStringWithIndex(
		"region RR_BROKEN {", "strict-region.rls",
		rls::parser::ParseMode::Strict);
	EXPECT_FALSE(strict.sourceIndex.regionContextAt({1, 19}));
}

TEST(SourceIndexTests, ReportsRecoveredSectionEntryLabelContexts) {
	const auto parsed = rls::parser::ParseStringWithIndex(
		"region RR_TEST {\n"
		"  events {\n"
		"    EVENT_EXISTING: true\n"
		"    EVENT_PAR\n"
		"  }\n"
		"  locations {\n"
		"    \n"
		"  }\n"
		"}\n",
		"section-entries.rls", rls::parser::ParseMode::Editor);
	ASSERT_FALSE(parsed.file.diagnostics.empty());

	const auto event = parsed.sourceIndex.sectionEntryAt({4, 14});
	ASSERT_TRUE(event);
	EXPECT_EQ(event->kind, SectionKind::Events);
	EXPECT_EQ(event->labelSpan.start.column, 5u);
	EXPECT_EQ(event->labelSpan.end.column, 14u);
	const auto eventRegion = parsed.sourceIndex.regionContextAt({4, 14});
	ASSERT_TRUE(eventRegion);
	EXPECT_EQ(eventRegion->name, "RR_TEST");
	EXPECT_EQ(eventRegion->activeSection, SectionKind::Events);
	EXPECT_EQ(eventRegion->activeSectionEntries,
		std::vector<std::string>{"EVENT_EXISTING"});
	EXPECT_EQ(parsed.sourceIndex.sectionEntryNames(SectionKind::Events),
		std::vector<std::string>{"EVENT_EXISTING"});
	EXPECT_EQ(parsed.sourceIndex.sectionEntryNames(
		SectionKind::Events, "RR_TEST"),
		std::vector<std::string>{"EVENT_EXISTING"});
	EXPECT_EQ(parsed.sourceIndex.regionNames(),
		std::vector<std::string>{"RR_TEST"});

	const auto location = parsed.sourceIndex.sectionEntryAt({7, 5});
	ASSERT_TRUE(location);
	EXPECT_EQ(location->kind, SectionKind::Locations);
	EXPECT_EQ(location->labelSpan.start.column, 5u);
	EXPECT_EQ(location->labelSpan.end.column, 5u);
	EXPECT_FALSE(parsed.sourceIndex.sectionEntryAt({3, 21}));

	const auto strict = rls::parser::ParseStringWithIndex(
		"region RR_TEST { events { EVENT_PARTIAL",
		"strict-section.rls", rls::parser::ParseMode::Strict);
	EXPECT_FALSE(strict.sourceIndex.sectionEntryAt({1, 46}));
}

TEST(SourceIndexTests, ReportsCompleteAndRecoveredMemberAccessContexts) {
	const auto complete = rls::parser::ParseStringWithIndex(
		"define check(): Color.RED\n", "member.rls");
	const auto completeMember = complete.sourceIndex.memberAccessAt({1, 24});
	ASSERT_TRUE(completeMember);
	EXPECT_EQ(completeMember->object, "Color");
	EXPECT_EQ(completeMember->memberSpan.start.column, 23u);
	EXPECT_EQ(completeMember->memberSpan.end.column, 26u);
	EXPECT_FALSE(complete.sourceIndex.memberAccessAt({1, 20}));

	const auto recovered = rls::parser::ParseStringWithIndex(
		"define first(): Color.\n"
		"define second(): Color.R\n"
		"define ignored(): \"Color.FAKE\" # Color.COMMENT\n",
		"recovered-member.rls", rls::parser::ParseMode::Editor);
	ASSERT_FALSE(recovered.file.diagnostics.empty());
	const auto emptyMember = recovered.sourceIndex.memberAccessAt({1, 23});
	ASSERT_TRUE(emptyMember);
	EXPECT_EQ(emptyMember->object, "Color");
	EXPECT_EQ(emptyMember->memberSpan.start.column, 23u);
	EXPECT_EQ(emptyMember->memberSpan.end.column, 23u);
	const auto partialMember = recovered.sourceIndex.memberAccessAt({2, 25});
	ASSERT_TRUE(partialMember);
	EXPECT_EQ(partialMember->object, "Color");
	EXPECT_FALSE(recovered.sourceIndex.memberAccessAt({3, 31}));

	const auto strict = rls::parser::ParseStringWithIndex(
		"define first(): Color.",
		"strict-member.rls", rls::parser::ParseMode::Strict);
	EXPECT_FALSE(strict.sourceIndex.memberAccessAt({1, 23}));
}

TEST(SourceIndexTests, ReportsRecoveredNamedArgumentContexts) {
	const auto emptySource = rls::parser::ParseStringWithIndex(
		"define first(): target(", "empty-argument.rls",
		rls::parser::ParseMode::Editor);
	ASSERT_FALSE(emptySource.file.diagnostics.empty());
	const auto empty = emptySource.sourceIndex.namedArgumentAt({1, 24});
	ASSERT_TRUE(empty);
	EXPECT_EQ(empty->callee, "target");
	EXPECT_EQ(empty->activeArgument, 0u);
	ASSERT_EQ(empty->argumentLabels.size(), 1u);
	EXPECT_FALSE(empty->argumentLabels[0]);
	EXPECT_EQ(empty->labelSpan.start.column, 24u);
	EXPECT_EQ(empty->labelSpan.end.column, 24u);
	const auto emptyValue = emptySource.sourceIndex.callArgumentAt({1, 24});
	ASSERT_TRUE(emptyValue);
	EXPECT_EQ(emptyValue->callee, "target");
	EXPECT_EQ(emptyValue->activeArgument, 0u);
	const auto emptyCall = emptySource.sourceIndex.enclosingCall({1, 24});
	ASSERT_TRUE(emptyCall);
	EXPECT_EQ(emptyCall->activeArgument, 0u);

	const auto partialSource = rls::parser::ParseStringWithIndex(
		"define second(): target(first: true, se", "partial-argument.rls",
		rls::parser::ParseMode::Editor);
	ASSERT_FALSE(partialSource.file.diagnostics.empty());
	const auto partial = partialSource.sourceIndex.namedArgumentAt({1, 40});
	ASSERT_TRUE(partial);
	EXPECT_EQ(partial->callee, "target");
	EXPECT_EQ(partial->activeArgument, 1u);
	ASSERT_EQ(partial->argumentLabels.size(), 2u);
	EXPECT_EQ(partial->argumentLabels[0], "first");
	EXPECT_FALSE(partial->argumentLabels[1]);
	const auto namedValue = partialSource.sourceIndex.callArgumentAt({1, 32});
	ASSERT_TRUE(namedValue);
	EXPECT_EQ(namedValue->activeArgument, 0u);
	EXPECT_EQ(namedValue->valueSpan.start.column, 32u);
	const auto partialValue = partialSource.sourceIndex.callArgumentAt({1, 40});
	ASSERT_TRUE(partialValue);
	EXPECT_EQ(partialValue->activeArgument, 1u);

	const auto nestedSource = rls::parser::ParseStringWithIndex(
		"define third(): target(true, nested(value), th", "nested-argument.rls",
		rls::parser::ParseMode::Editor);
	ASSERT_FALSE(nestedSource.file.diagnostics.empty());
	const auto nested = nestedSource.sourceIndex.namedArgumentAt({1, 47});
	ASSERT_TRUE(nested);
	EXPECT_EQ(nested->callee, "target");
	EXPECT_EQ(nested->activeArgument, 2u);
	ASSERT_EQ(nested->argumentLabels.size(), 3u);
	EXPECT_FALSE(nested->argumentLabels[0]);
	EXPECT_FALSE(nested->argumentLabels[1]);
	EXPECT_FALSE(nested->argumentLabels[2]);
	const auto nestedValue = nestedSource.sourceIndex.callArgumentAt({1, 38});
	ASSERT_TRUE(nestedValue);
	EXPECT_EQ(nestedValue->callee, "nested");
	EXPECT_EQ(nestedValue->activeArgument, 0u);

	const std::string blankNamedSource =
		"define fourth(): target(first:";
	const auto blankNamed = rls::parser::ParseStringWithIndex(
		blankNamedSource, "blank-named-argument.rls",
		rls::parser::ParseMode::Editor);
	const auto blankNamedValue = blankNamed.sourceIndex.callArgumentAt({1, 31});
	ASSERT_TRUE(blankNamedValue);
	EXPECT_EQ(blankNamedValue->argumentLabels[0], "first");
	EXPECT_EQ(blankNamedValue->valueSpan.start.line,
		blankNamedValue->valueSpan.end.line);
	EXPECT_EQ(blankNamedValue->valueSpan.start.column,
		blankNamedValue->valueSpan.end.column);

	const std::string trailingSlotSource =
		"define fifth(): target(first: true, ";
	const auto trailingSlot = rls::parser::ParseStringWithIndex(
		trailingSlotSource, "trailing-call-slot.rls",
		rls::parser::ParseMode::Editor);
	const auto trailingValue = trailingSlot.sourceIndex.callArgumentAt({1, 37});
	ASSERT_TRUE(trailingValue);
	EXPECT_EQ(trailingValue->activeArgument, 1u);
	ASSERT_EQ(trailingValue->argumentLabels.size(), 2u);
	EXPECT_EQ(trailingValue->argumentLabels[0], "first");
	EXPECT_FALSE(trailingValue->argumentLabels[1]);

	const auto ignored = rls::parser::ParseStringWithIndex(
		"define text(): \"target(fake:)\" # target(comment:)\n"
		"define broken(",
		"ignored-calls.rls", rls::parser::ParseMode::Editor);
	EXPECT_FALSE(ignored.sourceIndex.callArgumentAt({1, 28}));
	EXPECT_FALSE(ignored.sourceIndex.namedArgumentAt({1, 46}));

	const auto strict = rls::parser::ParseStringWithIndex(
		"define strict(): target(", "strict-argument.rls",
		rls::parser::ParseMode::Strict);
	EXPECT_FALSE(strict.sourceIndex.namedArgumentAt({1, 25}));
	EXPECT_FALSE(strict.sourceIndex.callArgumentAt({1, 25}));

	const auto closedTrailingSlot = rls::parser::ParseStringWithIndex(
		"define sixth(): target(true,)", "closed-trailing-slot.rls",
		rls::parser::ParseMode::Editor);
	const auto closedCall = closedTrailingSlot.sourceIndex.enclosingCall({1, 29});
	ASSERT_TRUE(closedCall);
	EXPECT_EQ(closedCall->activeArgument, 1u);
	ASSERT_EQ(closedCall->argumentRanges.size(), 2u);
	EXPECT_EQ(closedCall->argumentRanges[1].start.column, 29u);
	EXPECT_EQ(closedCall->argumentRanges[1].end.column, 29u);
}

TEST(SourceIndexTests, ReportsRecoveredFunctionTypePositions) {
	const auto positionAtEnd = [](const std::string& source) {
		const auto text = SourceText::FromUtf8(source);
		EXPECT_TRUE(text);
		return *text->utf8PositionAtByteOffset(source.size());
	};

	const std::string parameterSource =
		"enum Color { RED }\ndefine choose(value: Col";
	const auto parameter = rls::parser::ParseStringWithIndex(
		parameterSource, "parameter-type.rls", rls::parser::ParseMode::Editor);
	ASSERT_FALSE(parameter.file.diagnostics.empty());
	const auto parameterType = parameter.sourceIndex.typePositionAt(
		positionAtEnd(parameterSource));
	ASSERT_TRUE(parameterType);
	EXPECT_EQ(parameter.sourceIndex.enumNames(), std::vector<std::string>{"Color"});
	const auto strictParameter = rls::parser::ParseStringWithIndex(
		parameterSource, "parameter-type.rls", rls::parser::ParseMode::Strict);
	EXPECT_TRUE(strictParameter.sourceIndex.enumNames().empty());

	const auto filteredEnums = rls::parser::ParseStringWithIndex(
		"# enum Commented { VALUE }\n"
		"define text(): \"enum Quoted { VALUE }\"\n"
		"enum Real { VALUE }\n"
		"define broken(",
		"filtered-enums.rls", rls::parser::ParseMode::Editor);
	EXPECT_EQ(filteredEnums.sourceIndex.enumNames(),
		std::vector<std::string>{"Real"});

	const std::string blankParameterSource = "define choose(value: ";
	const auto blankParameter = rls::parser::ParseStringWithIndex(
		blankParameterSource, "blank-parameter-type.rls",
		rls::parser::ParseMode::Editor);
	const auto blankParameterType = blankParameter.sourceIndex.typePositionAt(
		positionAtEnd(blankParameterSource));
	ASSERT_TRUE(blankParameterType);
	EXPECT_EQ(blankParameterType->typeSpan.start.column, 21u);
	EXPECT_EQ(blankParameterType->typeSpan.end.column, 22u);

	const std::string returnSource =
		"extern define choose(value: Bool) -> Col";
	const auto returnType = rls::parser::ParseStringWithIndex(
		returnSource, "return-type.rls");
	ASSERT_TRUE(returnType.file.diagnostics.empty());
	EXPECT_TRUE(returnType.sourceIndex.typePositionAt(positionAtEnd(returnSource)));

	const std::string blankReturnSource = "extern define choose() -> ";
	const auto blankReturn = rls::parser::ParseStringWithIndex(
		blankReturnSource, "blank-return-type.rls",
		rls::parser::ParseMode::Editor);
	EXPECT_TRUE(blankReturn.sourceIndex.typePositionAt(
		positionAtEnd(blankReturnSource)));

	const std::string defaultSource =
		"define choose(value = true ? false : tru";
	const auto defaultExpression = rls::parser::ParseStringWithIndex(
		defaultSource, "default-expression.rls", rls::parser::ParseMode::Editor);
	EXPECT_FALSE(defaultExpression.sourceIndex.typePositionAt(
		positionAtEnd(defaultSource)));

	const auto strictBlank = rls::parser::ParseStringWithIndex(
		blankParameterSource, "strict-blank-type.rls",
		rls::parser::ParseMode::Strict);
	EXPECT_FALSE(strictBlank.sourceIndex.typePositionAt(
		positionAtEnd(blankParameterSource)));
}

TEST(ParseExpr, NestedCalls) {
	const auto& e = parseExpr("can_use(setting(RSK_FOO))");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& outer = std::get<CallExpr>(e.node);
	ASSERT_EQ(outer.args.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<CallExpr>(outer.args[0].value->node));
	const auto& inner = std::get<CallExpr>(outer.args[0].value->node);
	EXPECT_EQ(inner.callee.text, "setting");
}

// == Any-age host function ====================================================

TEST(ParseExpr, AnyAgeCall) {
	const auto& e = parseExpr("any_age(has(RG_HOOKSHOT))");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	EXPECT_EQ(call.callee, "any_age");
	ASSERT_EQ(call.args.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<CallExpr>(call.args[0].value->node));
}

// == here keyword =============================================================

TEST(ParseExpr, HereKeyword) {
	const auto& e = parseExpr("here");
	ASSERT_TRUE(std::holds_alternative<HereRef>(e.node));
	// resolvedRegion is empty at parse time — sema fills it in.
	EXPECT_EQ(std::get<HereRef>(e.node).resolvedRegion.text, "");
}

// == Match expression =========================================================

TEST(ParseExpr, MatchSingleArm) {
	const auto& e = parseExpr(
		"match distance {\n"
		"  ED_CLOSE: can_use(RG_KOKIRI_SWORD)\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(e.node));
	const auto& m = std::get<MatchExpr>(e.node);
	ASSERT_TRUE(std::holds_alternative<Identifier>(m.discriminant->node));
	EXPECT_EQ(std::get<Identifier>(m.discriminant->node).name, "distance");
	ASSERT_EQ(m.arms.size(), 1u);
	EXPECT_FALSE(m.arms[0].isDefault);
	ASSERT_EQ(m.arms[0].patterns.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<Identifier>(m.arms[0].patterns[0]->node));
	EXPECT_EQ(std::get<Identifier>(m.arms[0].patterns[0]->node).name, "ED_CLOSE");
	EXPECT_FALSE(m.arms[0].fallthrough);
}

TEST(ParseExpr, MatchMultipleArms) {
	const auto& e = parseExpr(
		"match distance {\n"
		"  ED_CLOSE: can_use(RG_KOKIRI_SWORD)\n"
		"  ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(e.node));
	const auto& m = std::get<MatchExpr>(e.node);
	ASSERT_EQ(m.arms.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<Identifier>(m.arms[0].patterns[0]->node));
	ASSERT_TRUE(std::holds_alternative<Identifier>(m.arms[1].patterns[0]->node));
	EXPECT_EQ(std::get<Identifier>(m.arms[0].patterns[0]->node).name, "ED_CLOSE");
	EXPECT_EQ(std::get<Identifier>(m.arms[1].patterns[0]->node).name, "ED_FAR");
}

TEST(ParseExpr, MatchArmWithOrPatterns) {
	const auto& e = parseExpr(
		"match x {\n"
		"  A or B: true\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(e.node));
	const auto& arm = std::get<MatchExpr>(e.node).arms[0];
	EXPECT_FALSE(arm.isDefault);
	ASSERT_EQ(arm.patterns.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<Identifier>(arm.patterns[0]->node));
	ASSERT_TRUE(std::holds_alternative<Identifier>(arm.patterns[1]->node));
	EXPECT_EQ(std::get<Identifier>(arm.patterns[0]->node).name, "A");
	EXPECT_EQ(std::get<Identifier>(arm.patterns[1]->node).name, "B");
}

TEST(ParseExpr, MatchDefaultArm) {
	const auto& e = parseExpr(
		"match x {\n"
		"  _: true\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(e.node));
	const auto& arm = std::get<MatchExpr>(e.node).arms[0];
	EXPECT_TRUE(arm.isDefault);
	EXPECT_TRUE(arm.patterns.empty());
}

TEST(ParseExpr, MatchArmFallthrough) {
	const auto& e = parseExpr(
		"match distance {\n"
		"  ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or\n"
		"  ED_CLOSE: has_explosives() or\n"
		"  ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(e.node));
	const auto& m = std::get<MatchExpr>(e.node);
	ASSERT_EQ(m.arms.size(), 3u);
	EXPECT_TRUE(m.arms[0].fallthrough);
	EXPECT_TRUE(m.arms[1].fallthrough);
	EXPECT_FALSE(m.arms[2].fallthrough);
}

// == Span tracking ============================================================

TEST(ParseExpr, SpanIsNonZero) {
	const auto file = parse("define foo(): true");
	const auto& def = std::get<DefineDecl>(file.declarations[0]);
	EXPECT_GT(def.span.start.line, 0u);
	EXPECT_GT(def.span.start.column, 0u);
}

TEST(ParseSpans, PreservesCompleteRangesForAstNodes) {
	const auto file = parse(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  events {\n"
		"    EVENT_TEST: has(ITEM) and true\n"
		"  }\n"
		"}\n"
		"define check(value: Item): not Item.VALUE and make_cond([1, 2])() ? true : false\n"
		"enum Color { RED, GREEN = 2 }\n"
		"extern enum External { VALUE, EXT_* }\n");

	auto expectCompleteSpan = [](const Span& span) {
		EXPECT_EQ(span.file, "in_memory");
		EXPECT_GT(span.start.line, 0u);
		EXPECT_GT(span.start.column, 0u);
		EXPECT_TRUE(span.end.line > span.start.line ||
			(span.end.line == span.start.line && span.end.column > span.start.column));
	};
	auto expectPosition = [](Position actual, uint32_t line, uint32_t column) {
		EXPECT_EQ(actual.line, line);
		EXPECT_EQ(actual.column, column);
	};

	ASSERT_EQ(file.declarations.size(), 4u);
	const auto& region = std::get<RegionDecl>(file.declarations[0]);
	expectCompleteSpan(region.span);
	expectPosition(region.span.start, 1, 1);
	expectPosition(region.span.end, 6, 2);
	ASSERT_EQ(region.body.data.size(), 1u);
	expectCompleteSpan(region.body.data[0].span);
	expectPosition(region.body.data[0].span.start, 2, 3);
	expectPosition(region.body.data[0].span.end, 2, 15);
	expectCompleteSpan(region.body.data[0].key.span);
	expectCompleteSpan(region.body.data[0].value->span);
	ASSERT_EQ(region.body.sections.size(), 1u);
	const auto& section = region.body.sections[0];
	expectCompleteSpan(section.span);
	expectPosition(section.span.start, 3, 3);
	expectPosition(section.span.end, 5, 4);
	ASSERT_EQ(section.entries.size(), 1u);
	expectCompleteSpan(section.entries[0].span);
	expectPosition(section.entries[0].span.start, 4, 5);
	expectPosition(section.entries[0].span.end, 4, 35);
	expectCompleteSpan(section.entries[0].name.span);
	expectCompleteSpan(section.entries[0].condition->span);

	const auto& define = std::get<DefineDecl>(file.declarations[1]);
	expectCompleteSpan(define.span);
	expectCompleteSpan(define.name.span);
	expectCompleteSpan(define.params[0].name.span);
	expectCompleteSpan(define.params[0].type->name.span);
	expectCompleteSpan(define.body->span);
	const auto& ternary = std::get<TernaryExpr>(define.body->node);
	expectCompleteSpan(ternary.condition->span);
	const auto& logical = std::get<BinaryExpr>(ternary.condition->node);
	expectCompleteSpan(logical.left->span);
	const auto& member = std::get<MemberExpr>(std::get<UnaryExpr>(logical.left->node).operand->node);
	expectCompleteSpan(member.object.span);
	expectCompleteSpan(member.member.span);
	const auto& invoke = std::get<InvokeExpr>(logical.right->node);
	expectCompleteSpan(invoke.callee->span);
	const auto& call = std::get<CallExpr>(invoke.callee->node);
	expectCompleteSpan(call.callee.span);
	expectCompleteSpan(call.args[0].value->span);

	const auto& enumDecl = std::get<EnumDecl>(file.declarations[2]);
	expectCompleteSpan(enumDecl.span);
	expectCompleteSpan(enumDecl.name.span);
	for (const auto& member : enumDecl.members) {
		expectCompleteSpan(member.span);
		expectCompleteSpan(member.name.span);
	}

	const auto& externEnum = std::get<ExternEnumDecl>(file.declarations[3]);
	expectCompleteSpan(externEnum.span);
	expectCompleteSpan(externEnum.name.span);
	const auto& externalMember = std::get<EnumMemberDecl>(externEnum.entries[0]);
	expectCompleteSpan(externalMember.span);
	expectCompleteSpan(externalMember.name.span);
	expectCompleteSpan(std::get<EnumPatternDecl>(externEnum.entries[1]).span);
}

// == Define declaration =======================================================

TEST(ParseDefine, NoParams) {
	const auto& decl = parseDecl(
		"define has_explosives():\n"
		"  has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)"
	);
	const auto& def = std::get<DefineDecl>(decl);
	EXPECT_EQ(def.name, "has_explosives");
	EXPECT_TRUE(def.params.empty());
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(def.body->node));
	EXPECT_EQ(std::get<BinaryExpr>(def.body->node).op, BinaryOp::Or);
}

TEST(ParseDefine, SingleParam) {
	const auto& decl = parseDecl("define can_use(item): has(item)");
	const auto& def = std::get<DefineDecl>(decl);
	EXPECT_EQ(def.name, "can_use");
	ASSERT_EQ(def.params.size(), 1u);
	EXPECT_EQ(def.params[0].name, "item");
	EXPECT_FALSE(def.params[0].type.has_value());
	EXPECT_EQ(def.params[0].defaultValue, nullptr);
}

TEST(ParseDefine, MultipleParams) {
	const auto& decl = parseDecl(
		"define can_kill(target, distance, wallOrFloor): true"
	);
	const auto& def = std::get<DefineDecl>(decl);
	ASSERT_EQ(def.params.size(), 3u);
	EXPECT_EQ(def.params[0].name, "target");
	EXPECT_EQ(def.params[1].name, "distance");
	EXPECT_EQ(def.params[2].name, "wallOrFloor");
}

TEST(ParseDefine, ParamWithType) {
	const auto& decl = parseDecl("define foo(x: int): true");
	const auto& def = std::get<DefineDecl>(decl);
	ASSERT_EQ(def.params.size(), 1u);
	EXPECT_EQ(def.params[0].name.span.start.line, 1u);
	EXPECT_EQ(def.params[0].name.span.start.column, 12u);
	EXPECT_EQ(def.params[0].name.span.end.column, 13u);
	ASSERT_TRUE(def.params[0].type.has_value());
	EXPECT_EQ(*def.params[0].type, "int");
	EXPECT_EQ(def.params[0].type->name.span.start.column, 15u);
	EXPECT_EQ(def.params[0].type->name.span.end.column, 18u);
	EXPECT_EQ(def.params[0].defaultValue, nullptr);
}

TEST(ParseDefine, ParamWithDefault) {
	const auto& decl = parseDecl(
		"define foo(distance = ED_CLOSE): true"
	);
	const auto& def = std::get<DefineDecl>(decl);
	ASSERT_EQ(def.params.size(), 1u);
	ASSERT_NE(def.params[0].defaultValue, nullptr);
	EXPECT_TRUE(std::holds_alternative<Identifier>(
		def.params[0].defaultValue->node));
	EXPECT_EQ(std::get<Identifier>(def.params[0].defaultValue->node).name,
	          "ED_CLOSE");
}

TEST(ParseDefine, ParamWithTypeAndDefault) {
	const auto& decl = parseDecl("define foo(x: int = 0): true");
	const auto& def = std::get<DefineDecl>(decl);
	ASSERT_EQ(def.params.size(), 1u);
	ASSERT_TRUE(def.params[0].type.has_value());
	EXPECT_EQ(*def.params[0].type, "int");
	ASSERT_NE(def.params[0].defaultValue, nullptr);
	EXPECT_EQ(std::get<IntLiteral>(def.params[0].defaultValue->node).value, 0);
}

TEST(ParseDefine, ComplexBody) {
	const auto& decl = parseDecl(
		"define spirit_key_logic():\n"
		"  keys(SCENE_SPIRIT_TEMPLE, has_explosives() ? 1 : 2)"
	);
	const auto& def = std::get<DefineDecl>(decl);
	EXPECT_EQ(def.name, "spirit_key_logic");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(def.body->node));
	const auto& call = std::get<CallExpr>(def.body->node);
	EXPECT_EQ(call.callee.text, "keys");
	ASSERT_EQ(call.args.size(), 2u);
	// Second arg should be a ternary
	EXPECT_TRUE(std::holds_alternative<TernaryExpr>(call.args[1].value->node));
}

// == Extern define declaration ===============================================

TEST(ParseExternDefine, NoParams) {
	const auto& decl = parseDecl("extern define has(item) -> Bool");
	const auto& ext = std::get<ExternDefineDecl>(decl);
	EXPECT_EQ(ext.name, "has");
	ASSERT_EQ(ext.params.size(), 1u);
	EXPECT_EQ(ext.params[0].name, "item");
	EXPECT_FALSE(ext.params[0].type.has_value());
	EXPECT_EQ(ext.params[0].defaultValue, nullptr);
	ASSERT_TRUE(ext.returnType.has_value());
	EXPECT_EQ(*ext.returnType, "Bool");
}

TEST(ParseExternDefine, TypedAndDefaultedParams) {
	const auto& decl = parseDecl("extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater = false) -> Bool");
	const auto& ext = std::get<ExternDefineDecl>(decl);
	EXPECT_EQ(ext.name, "can_hit_switch");
	ASSERT_EQ(ext.params.size(), 2u);

	EXPECT_EQ(ext.params[0].name, "distance");
	ASSERT_TRUE(ext.params[0].type.has_value());
	EXPECT_EQ(*ext.params[0].type, "Distance");
	ASSERT_NE(ext.params[0].defaultValue, nullptr);
	EXPECT_TRUE(std::holds_alternative<Identifier>(ext.params[0].defaultValue->node));

	EXPECT_EQ(ext.params[1].name, "inWater");
	EXPECT_FALSE(ext.params[1].type.has_value());
	ASSERT_NE(ext.params[1].defaultValue, nullptr);
	EXPECT_TRUE(std::holds_alternative<BoolLiteral>(ext.params[1].defaultValue->node));
	ASSERT_TRUE(ext.returnType.has_value());
	EXPECT_EQ(*ext.returnType, "Bool");
}

// == Enum declaration ========================================================

TEST(ParseEnum, Empty) {
	const auto& decl = parseDecl("enum Item {}");
	const auto& e = std::get<EnumDecl>(decl);
	EXPECT_EQ(e.name, "Item");
	EXPECT_TRUE(e.members.empty());
}

TEST(ParseEnum, MembersWithOptionalExplicitValues) {
	const auto& decl = parseDecl("enum Item { RG_HOOKSHOT, RG_FAIRY_BOW = 17 }");
	const auto& e = std::get<EnumDecl>(decl);
	EXPECT_EQ(e.name, "Item");
	ASSERT_EQ(e.members.size(), 2u);
	EXPECT_EQ(e.members[0].name, "RG_HOOKSHOT");
	EXPECT_FALSE(e.members[0].explicitValue.has_value());
	EXPECT_EQ(e.members[1].name, "RG_FAIRY_BOW");
	ASSERT_TRUE(e.members[1].explicitValue.has_value());
	EXPECT_EQ(*e.members[1].explicitValue, 17);
}

// == Extern enum declaration =================================================

TEST(ParseExternEnum, Empty) {
	const auto& decl = parseDecl("extern enum Item {}");
	const auto& e = std::get<ExternEnumDecl>(decl);
	EXPECT_EQ(e.name, "Item");
	EXPECT_TRUE(e.entries.empty());
}

TEST(ParseExternEnum, MixedEntries) {
	const auto& decl = parseDecl("extern enum Item { RG_HOOKSHOT, RG_*, *_KEY, R*_BOSS, RG_FAIRY_BOW = 9 }");
	const auto& e = std::get<ExternEnumDecl>(decl);
	EXPECT_EQ(e.name, "Item");
	ASSERT_EQ(e.entries.size(), 5u);

	ASSERT_TRUE(std::holds_alternative<EnumMemberDecl>(e.entries[0]));
	EXPECT_EQ(std::get<EnumMemberDecl>(e.entries[0]).name, "RG_HOOKSHOT");

	ASSERT_TRUE(std::holds_alternative<EnumPatternDecl>(e.entries[1]));
	EXPECT_EQ(std::get<EnumPatternDecl>(e.entries[1]).pattern, "RG_*");

	ASSERT_TRUE(std::holds_alternative<EnumPatternDecl>(e.entries[2]));
	EXPECT_EQ(std::get<EnumPatternDecl>(e.entries[2]).pattern, "*_KEY");

	ASSERT_TRUE(std::holds_alternative<EnumPatternDecl>(e.entries[3]));
	EXPECT_EQ(std::get<EnumPatternDecl>(e.entries[3]).pattern, "R*_BOSS");

	ASSERT_TRUE(std::holds_alternative<EnumMemberDecl>(e.entries[4]));
	const auto& member = std::get<EnumMemberDecl>(e.entries[4]);
	EXPECT_EQ(member.name, "RG_FAIRY_BOW");
	ASSERT_TRUE(member.explicitValue.has_value());
	EXPECT_EQ(*member.explicitValue, 9);
}

// == Region declaration =======================================================

TEST(ParseMemberAccess, BasicDottedAccess) {
	const auto& expr = parseExpr("Item.RG_HOOKSHOT");
	const auto& m = std::get<MemberExpr>(expr.node);
	EXPECT_EQ(m.object.text, "Item");
	EXPECT_EQ(m.member.text, "RG_HOOKSHOT");
}

TEST(ParseMemberAccess, SpanIsNonZero) {
	const auto& expr = parseExpr("Item.RG_HOOKSHOT");
	EXPECT_EQ(expr.span.start.line, 1u);
	EXPECT_EQ(expr.span.start.column, 13u);
	EXPECT_EQ(expr.span.end.line, 1u);
	EXPECT_EQ(expr.span.end.column, 29u);
}

TEST(ParseMemberAccess, UsedAsCallArg) {
	const auto& expr = parseExpr("has(Item.RG_HOOKSHOT)");
	const auto& call = std::get<CallExpr>(expr.node);
	EXPECT_EQ(call.callee.text, "has");
	ASSERT_EQ(call.args.size(), 1u);
	const auto& m = std::get<MemberExpr>(call.args[0].value->node);
	EXPECT_EQ(m.object.text, "Item");
	EXPECT_EQ(m.member.text, "RG_HOOKSHOT");
}

TEST(ParseMemberAccess, UsedInBinaryExpr) {
	const auto& expr = parseExpr("Item.RG_HOOKSHOT == Item.RG_FAIRY_BOW");
	const auto& bin = std::get<BinaryExpr>(expr.node);
	EXPECT_EQ(bin.op, BinaryOp::Eq);
	const auto& lhs = std::get<MemberExpr>(bin.left->node);
	EXPECT_EQ(lhs.object.text, "Item");
	EXPECT_EQ(lhs.member.text, "RG_HOOKSHOT");
	const auto& rhs = std::get<MemberExpr>(bin.right->node);
	EXPECT_EQ(rhs.object.text, "Item");
	EXPECT_EQ(rhs.member.text, "RG_FAIRY_BOW");
}

TEST(ParseEnumDiagnostics, MissingEnumName) {
	const auto file = parse("enum { RG_HOOKSHOT }");
	EXPECT_TRUE(file.declarations.empty());
	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected identifier");
}

TEST(ParseEnumDiagnostics, MissingCloseBrace) {
	const auto file = parse("extern enum Item { RG_*");
	EXPECT_TRUE(file.declarations.empty());
	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected '}'");
}

TEST(ParseMemberAccessDiagnostics, MissingMemberNameAfterDot) {
	const auto file = parse("define _(): Item.");
	EXPECT_TRUE(file.declarations.empty());
	ASSERT_FALSE(file.diagnostics.empty());
	EXPECT_EQ(file.diagnostics[0].level, DiagnosticLevel::Error);
	EXPECT_EQ(file.diagnostics[0].message, "expected declaration or end of file");
}

// == Region declaration =======================================================

TEST(ParseRegion, MinimalRegion) {
	const auto& decl = parseDecl("region RR_TEST { name: \"Test\" scene: SCENE_TEST }");
	const auto& region = std::get<RegionDecl>(decl);
	EXPECT_EQ(region.key, "RR_TEST");
	EXPECT_EQ(region.key.span.start.line, 1u);
	EXPECT_EQ(region.key.span.start.column, 8u);
	EXPECT_EQ(region.key.span.end.column, 15u);
	ASSERT_EQ(region.body.data.size(), 2u);
	const auto* name = region.body.findData("name");
	ASSERT_NE(name, nullptr);
	EXPECT_EQ(std::get<StringLiteral>(name->value->node).value, "Test");
	const auto* scene = region.body.findData("scene");
	ASSERT_NE(scene, nullptr);
	EXPECT_EQ(std::get<Identifier>(scene->value->node).name, "SCENE_TEST");
	EXPECT_EQ(scene->key.span.start.column, 31u);
	EXPECT_EQ(scene->key.span.end.column, 36u);
	EXPECT_TRUE(region.body.sections.empty());
}

TEST(ParseRegion, ExpressionValuedData) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  warpCost: 2 + 3\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	const auto* warpCost = region.body.findData("warpCost");
	ASSERT_NE(warpCost, nullptr);
	EXPECT_TRUE(std::holds_alternative<BinaryExpr>(warpCost->value->node));
}

TEST(ParseRegion, ListValuedData) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  areas: [AREA_A, AREA_B]\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	const auto* areas = region.body.findData("areas");
	ASSERT_NE(areas, nullptr);
	const auto& list = std::get<ListExpr>(areas->value->node);
	ASSERT_EQ(list.elements.size(), 2u);
	EXPECT_EQ(std::get<Identifier>(list.elements[0]->node).name, "AREA_A");
	EXPECT_EQ(std::get<Identifier>(list.elements[1]->node).name, "AREA_B");
}

TEST(ParseRegion, WithSections) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  events {\n"
		"    EV_TEST: can_break_pots()\n"
		"  }\n"
		"  locations {\n"
		"    RC_TEST_POT: can_break_pots()\n"
		"  }\n"
		"  exits {\n"
		"    RR_OTHER: always\n"
		"  }\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	ASSERT_EQ(region.body.sections.size(), 3u);
	EXPECT_EQ(region.body.sections[0].kind, SectionKind::Events);
	EXPECT_EQ(region.body.sections[1].kind, SectionKind::Locations);
	EXPECT_EQ(region.body.sections[2].kind, SectionKind::Exits);
}

TEST(ParseRegion, SectionEntries) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  locations {\n"
		"    RC_POT_1: can_break_pots()\n"
		"    RC_POT_2: always\n"
		"  }\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	ASSERT_EQ(region.body.sections.size(), 1u);
	const auto& section = region.body.sections[0];
	EXPECT_EQ(section.kind, SectionKind::Locations);
	ASSERT_EQ(section.entries.size(), 2u);
	EXPECT_EQ(section.entries[0].name, "RC_POT_1");
	EXPECT_TRUE(std::holds_alternative<CallExpr>(section.entries[0].condition->node));
	EXPECT_EQ(section.entries[1].name, "RC_POT_2");
	EXPECT_TRUE(std::holds_alternative<BoolLiteral>(section.entries[1].condition->node));
}

TEST(ParseRegion, EmptySection) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  events {}\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	ASSERT_EQ(region.body.sections.size(), 1u);
	EXPECT_EQ(region.body.sections[0].kind, SectionKind::Events);
	EXPECT_TRUE(region.body.sections[0].entries.empty());
}

TEST(ParseRegion, FullRegion) {
	const auto& decl = parseDecl(
		"region RR_SPIRIT_FOYER {\n"
		"  name: \"Spirit Foyer\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  timePasses: TimePasses.Yes\n"
		"  areas: [AREA_SPIRIT_TEMPLE]\n"
		"  locations {\n"
		"    RC_SPIRIT_LOBBY_POT: can_break_pots()\n"
		"  }\n"
		"  exits {\n"
		"    RR_SPIRIT_ENTRYWAY: always\n"
		"    RR_SPIRIT_CHILD: is_child() and has(RG_STICKS)\n"
		"  }\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	EXPECT_EQ(region.key, "RR_SPIRIT_FOYER");
	ASSERT_NE(region.body.findData("scene"), nullptr);
	ASSERT_NE(region.body.findData("timePasses"), nullptr);
	ASSERT_NE(region.body.findData("areas"), nullptr);
	ASSERT_EQ(region.body.sections.size(), 2u);
	EXPECT_EQ(region.body.sections[0].entries.size(), 1u);
	EXPECT_EQ(region.body.sections[1].entries.size(), 2u);
}

// == Extend region declaration ================================================

TEST(ParseExtend, Empty) {
	const auto& decl = parseDecl("extend region RR_TEST {}");
	const auto& ext = std::get<ExtendRegionDecl>(decl);
	EXPECT_EQ(ext.name, "RR_TEST");
	EXPECT_TRUE(ext.sections.empty());
}

TEST(ParseExtend, WithSection) {
	const auto& decl = parseDecl(
		"extend region RR_TEST {\n"
		"  locations {\n"
		"    RC_POT_1: can_break_pots()\n"
		"  }\n"
		"}"
	);
	const auto& ext = std::get<ExtendRegionDecl>(decl);
	EXPECT_EQ(ext.name, "RR_TEST");
	ASSERT_EQ(ext.sections.size(), 1u);
	EXPECT_EQ(ext.sections[0].kind, SectionKind::Locations);
	ASSERT_EQ(ext.sections[0].entries.size(), 1u);
	EXPECT_EQ(ext.sections[0].entries[0].name, "RC_POT_1");
}

TEST(ParseExtend, MultipleSections) {
	const auto& decl = parseDecl(
		"extend region RR_TEST {\n"
		"  locations {\n"
		"    RC_POT: can_break_pots()\n"
		"  }\n"
		"  events {\n"
		"    EV_TEST: always\n"
		"  }\n"
		"}"
	);
	const auto& ext = std::get<ExtendRegionDecl>(decl);
	ASSERT_EQ(ext.sections.size(), 2u);
}

// == Multi-declaration files ==================================================

TEST(ParseFile, MultipleRegions) {
	const auto file = parse(
		"region RR_A {\n"
		"  name: \"A\"\n"
		"  scene: SCENE_A\n"
		"}\n"
		"\n"
		"region RR_B {\n"
		"  name: \"B\"\n"
		"  scene: SCENE_B\n"
		"}\n"
	);
	ASSERT_EQ(file.declarations.size(), 2u);
	EXPECT_EQ(std::get<RegionDecl>(file.declarations[0]).key, "RR_A");
	EXPECT_EQ(std::get<RegionDecl>(file.declarations[1]).key, "RR_B");
}

TEST(ParseFile, MixedDeclarations) {
	const auto file = parse(
		"enum Item { RG_HOOKSHOT }\n"
		"extern enum HostItem { RG_* }\n"
		"\n"
		"extern define has(item) -> Bool\n"
		"\n"
		"define has_explosives():\n"
		"  has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n"
		"\n"
		"define kill_fn(e: Enemy):\n"
		"  has_explosives()\n"
		"\n"
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_FOO\n"
		"  exits {\n"
		"    RR_OTHER: kill_fn(RE_ARMOS)\n"
		"  }\n"
		"}\n"
		"\n"
		"extend region RR_TEST {\n"
		"  locations {\n"
		"    RC_TEST_POT: can_break_pots()\n"
		"  }\n"
		"}\n"
	);
	ASSERT_EQ(file.declarations.size(), 7u);
	EXPECT_TRUE(std::holds_alternative<EnumDecl>(file.declarations[0]));
	EXPECT_TRUE(std::holds_alternative<ExternEnumDecl>(file.declarations[1]));
	EXPECT_TRUE(std::holds_alternative<ExternDefineDecl>(file.declarations[2]));
	EXPECT_TRUE(std::holds_alternative<DefineDecl>(file.declarations[3]));
	EXPECT_TRUE(std::holds_alternative<DefineDecl>(file.declarations[4]));
	EXPECT_TRUE(std::holds_alternative<RegionDecl>(file.declarations[5]));
	EXPECT_TRUE(std::holds_alternative<ExtendRegionDecl>(file.declarations[6]));
}

TEST(ParseFile, ExternDefineCallNamedArgsPreserved) {
	const auto file = parse(
		"extern define keys(sc: Scene, amount: Int) -> Bool\n"
		"define can_open_spirit():\n"
		"  keys(amount: 3, sc: SCENE_SPIRIT_TEMPLE)\n"
	);

	ASSERT_EQ(file.declarations.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<DefineDecl>(file.declarations[1]));
	const auto& def = std::get<DefineDecl>(file.declarations[1]);
	ASSERT_TRUE(std::holds_alternative<CallExpr>(def.body->node));
	const auto& call = std::get<CallExpr>(def.body->node);
	EXPECT_EQ(call.callee.text, "keys");
	ASSERT_EQ(call.args.size(), 2u);

	ASSERT_TRUE(call.args[0].name.has_value());
	EXPECT_EQ(*call.args[0].name, "amount");
	ASSERT_TRUE(call.args[1].name.has_value());
	EXPECT_EQ(*call.args[1].name, "sc");

	ASSERT_TRUE(std::holds_alternative<IntLiteral>(call.args[0].value->node));
	EXPECT_EQ(std::get<IntLiteral>(call.args[0].value->node).value, 3);
	ASSERT_TRUE(std::holds_alternative<Identifier>(call.args[1].value->node));
	EXPECT_EQ(std::get<Identifier>(call.args[1].value->node).name, "SCENE_SPIRIT_TEMPLE");
}

TEST(ParseFile, WithComments) {
	const auto file = parse(
		"# helpers\n"
		"define foo(): true\n"
		"\n"
		"# regions\n"
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"}\n"
	);
	ASSERT_EQ(file.declarations.size(), 2u);
}

// == Realistic end-to-end =====================================================

TEST(ParseRealistic, SpiritTempleExcerpt) {
	const auto file = parse(
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  name: \"Spirit Temple Foyer\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()\n"
		"  }\n"
		"  exits {\n"
		"    RR_SPIRIT_TEMPLE_ENTRYWAY: always\n"
		"    RR_SPIRIT_TEMPLE_CHILD: is_child()\n"
		"    RR_SPIRIT_TEMPLE_ADULT:\n"
		"      is_adult() and can_use(RG_SILVER_GAUNTLETS)\n"
		"  }\n"
		"}\n"
	);
	ASSERT_EQ(file.declarations.size(), 1u);
	const auto& region = std::get<RegionDecl>(file.declarations[0]);
	EXPECT_EQ(region.key, "RR_SPIRIT_TEMPLE_FOYER");
	ASSERT_NE(region.body.findData("scene"), nullptr);
	ASSERT_EQ(region.body.sections.size(), 2u);

	const auto& locs = region.body.sections[0];
	EXPECT_EQ(locs.kind, SectionKind::Locations);
	EXPECT_EQ(locs.entries.size(), 2u);

	const auto& exits = region.body.sections[1];
	EXPECT_EQ(exits.kind, SectionKind::Exits);
	ASSERT_EQ(exits.entries.size(), 3u);
	EXPECT_EQ(exits.entries[0].name, "RR_SPIRIT_TEMPLE_ENTRYWAY");
	// Third exit should be a binary and
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(exits.entries[2].condition->node));
	EXPECT_EQ(std::get<BinaryExpr>(exits.entries[2].condition->node).op, BinaryOp::And);
}

TEST(ParseRealistic, DefineWithMatch) {
	const auto file = parse(
		"define can_hit_switch(distance = ED_CLOSE, inWater = false):\n"
		"  match distance {\n"
		"    ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or\n"
		"    ED_CLOSE: has_explosives() or\n"
		"    ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"  }\n"
	);
	ASSERT_EQ(file.declarations.size(), 1u);
	const auto& def = std::get<DefineDecl>(file.declarations[0]);
	EXPECT_EQ(def.name, "can_hit_switch");
	ASSERT_EQ(def.params.size(), 2u);
	EXPECT_EQ(def.params[0].name, "distance");
	EXPECT_EQ(def.params[1].name, "inWater");
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(def.body->node));
	const auto& m = std::get<MatchExpr>(def.body->node);
	ASSERT_TRUE(std::holds_alternative<Identifier>(m.discriminant->node));
	EXPECT_EQ(std::get<Identifier>(m.discriminant->node).name, "distance");
	ASSERT_EQ(m.arms.size(), 3u);
	EXPECT_TRUE(m.arms[0].fallthrough);
	EXPECT_TRUE(m.arms[1].fallthrough);
	EXPECT_FALSE(m.arms[2].fallthrough);
}

TEST(ParseRealistic, SpiritSharedCallInRegionExit) {
	const auto file = parse(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  exits {\n"
		"    RR_TARGET: spirit_shared(\n"
		"      first_region: RR_ROOM_A,\n"
		"      first_condition: has(RG_HOOKSHOT),\n"
		"      second_region: here,\n"
		"      second_condition: can_use(RG_BOOMERANG)\n"
		"    )\n"
		"  }\n"
		"}\n"
	);
	const auto& region = std::get<RegionDecl>(file.declarations[0]);
	const auto& exits = region.body.sections[0];
	ASSERT_EQ(exits.entries.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<CallExpr>(exits.entries[0].condition->node));
	const auto& call = std::get<CallExpr>(exits.entries[0].condition->node);
	EXPECT_EQ(call.callee, "spirit_shared");
	ASSERT_EQ(call.args.size(), 4u);
	ASSERT_TRUE(call.args[0].name.has_value());
	EXPECT_EQ(*call.args[0].name, "first_region");
	ASSERT_TRUE(call.args[1].name.has_value());
	EXPECT_EQ(*call.args[1].name, "first_condition");
	ASSERT_TRUE(call.args[2].name.has_value());
	EXPECT_EQ(*call.args[2].name, "second_region");
	ASSERT_TRUE(call.args[3].name.has_value());
	EXPECT_EQ(*call.args[3].name, "second_condition");
}

TEST(ParseRealistic, AnyAgeCallInRegionExit) {
	const auto file = parse(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  exits {\n"
		"    RR_TARGET: any_age(has(RG_HOOKSHOT) or has(RG_BOOMERANG))\n"
		"  }\n"
		"}\n"
	);
	const auto& region = std::get<RegionDecl>(file.declarations[0]);
	const auto& exits = region.body.sections[0];
	ASSERT_TRUE(std::holds_alternative<CallExpr>(exits.entries[0].condition->node));
	const auto& call = std::get<CallExpr>(exits.entries[0].condition->node);
	EXPECT_EQ(call.callee, "any_age");
}