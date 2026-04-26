#include <gtest/gtest.h>
#include <stdexcept>

#include "ast.h"
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
	EXPECT_EQ(region->body.scene, "SCENE_TEST");
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
	EXPECT_EQ(std::get<Identifier>(e.node).name, "RG_HOOKSHOT");
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
	EXPECT_EQ(call.function, "has_explosives");
	EXPECT_TRUE(call.args.empty());
}

TEST(ParseExpr, CallSinglePositionalArg) {
	const auto& e = parseExpr("has(RG_HOOKSHOT)");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	EXPECT_EQ(call.function, "has");
	ASSERT_EQ(call.args.size(), 1u);
	EXPECT_FALSE(call.args[0].name.has_value());
	EXPECT_TRUE(std::holds_alternative<Identifier>(call.args[0].value->node));
}

TEST(ParseExpr, CallMultipleArgs) {
	const auto& e = parseExpr("can_kill(RE_ARMOS, ED_CLOSE, false)");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	EXPECT_EQ(call.function, "can_kill");
	ASSERT_EQ(call.args.size(), 3u);
	EXPECT_FALSE(call.args[0].name.has_value());
	EXPECT_FALSE(call.args[1].name.has_value());
	EXPECT_FALSE(call.args[2].name.has_value());
}

TEST(ParseExpr, CallNamedArg) {
	const auto& e = parseExpr("foo(x: 1, y: 2)");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& call = std::get<CallExpr>(e.node);
	ASSERT_EQ(call.args.size(), 2u);
	ASSERT_TRUE(call.args[0].name.has_value());
	EXPECT_EQ(*call.args[0].name, "x");
	ASSERT_TRUE(call.args[1].name.has_value());
	EXPECT_EQ(*call.args[1].name, "y");
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

TEST(ParseExpr, NestedCalls) {
	const auto& e = parseExpr("can_use(setting(RSK_FOO))");
	ASSERT_TRUE(std::holds_alternative<CallExpr>(e.node));
	const auto& outer = std::get<CallExpr>(e.node);
	ASSERT_EQ(outer.args.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<CallExpr>(outer.args[0].value->node));
	const auto& inner = std::get<CallExpr>(outer.args[0].value->node);
	EXPECT_EQ(inner.function, "setting");
}

// == Shared block =============================================================

TEST(ParseExpr, SharedSimple) {
	const auto& e = parseExpr(
		"shared {\n"
		"  from RR_ROOM_A: has(RG_HOOKSHOT)\n"
		"  from here: true\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(e.node));
	const auto& sb = std::get<SharedBlock>(e.node);
	EXPECT_FALSE(sb.anyAge);
	ASSERT_EQ(sb.branches.size(), 2u);
	ASSERT_TRUE(sb.branches[0].region.has_value());
	EXPECT_EQ(*sb.branches[0].region, "RR_ROOM_A");
	EXPECT_FALSE(sb.branches[1].region.has_value()); // "here"
}

TEST(ParseExpr, SharedAnyAge) {
	const auto& e = parseExpr(
		"shared any_age {\n"
		"  from RR_A: true\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(e.node));
	EXPECT_TRUE(std::get<SharedBlock>(e.node).anyAge);
}

TEST(ParseExpr, SharedMultipleBranches) {
	const auto& e = parseExpr(
		"shared {\n"
		"  from RR_A: true\n"
		"  from RR_B: false\n"
		"  from RR_C: is_adult()\n"
		"}"
	);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(e.node));
	EXPECT_EQ(std::get<SharedBlock>(e.node).branches.size(), 3u);
}

// == Any-age block ============================================================

TEST(ParseExpr, AnyAgeBlock) {
	const auto& e = parseExpr("any_age { has(RG_HOOKSHOT) }");
	ASSERT_TRUE(std::holds_alternative<AnyAgeBlock>(e.node));
	const auto& aab = std::get<AnyAgeBlock>(e.node);
	EXPECT_TRUE(std::holds_alternative<CallExpr>(aab.body->node));
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
	EXPECT_EQ(m.discriminant, "distance");
	ASSERT_EQ(m.arms.size(), 1u);
	EXPECT_FALSE(m.arms[0].isDefault);
	ASSERT_EQ(m.arms[0].patterns.size(), 1u);
	EXPECT_EQ(m.arms[0].patterns[0], "ED_CLOSE");
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
	EXPECT_EQ(m.arms[0].patterns[0], "ED_CLOSE");
	EXPECT_EQ(m.arms[1].patterns[0], "ED_FAR");
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
	EXPECT_EQ(arm.patterns[0], "A");
	EXPECT_EQ(arm.patterns[1], "B");
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
	ASSERT_TRUE(def.params[0].type.has_value());
	EXPECT_EQ(*def.params[0].type, "int");
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
	EXPECT_EQ(call.function, "keys");
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

// == Region declaration =======================================================

TEST(ParseRegion, MinimalRegion) {
	const auto& decl = parseDecl("region RR_TEST { name: \"Test\" scene: SCENE_TEST }");
	const auto& region = std::get<RegionDecl>(decl);
	EXPECT_EQ(region.key, "RR_TEST");
	ASSERT_TRUE(region.body.scene.has_value());
	EXPECT_EQ(*region.body.scene, "SCENE_TEST");
	EXPECT_EQ(region.body.timePasses, TimePasses::Auto);
	EXPECT_TRUE(region.body.areas.empty());
	EXPECT_TRUE(region.body.sections.empty());
}

TEST(ParseRegion, WithTimePasses) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  time_passes\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	EXPECT_EQ(region.body.timePasses, TimePasses::Yes);
}

TEST(ParseRegion, WithNoTimePasses) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  no_time_passes\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	EXPECT_EQ(region.body.timePasses, TimePasses::No);
}

TEST(ParseRegion, WithAreas) {
	const auto& decl = parseDecl(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  areas: AREA_A, AREA_B\n"
		"}"
	);
	const auto& region = std::get<RegionDecl>(decl);
	ASSERT_EQ(region.body.areas.size(), 2u);
	EXPECT_EQ(region.body.areas[0], "AREA_A");
	EXPECT_EQ(region.body.areas[1], "AREA_B");
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
		"  time_passes\n"
		"  areas: AREA_SPIRIT_TEMPLE\n"
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
	EXPECT_EQ(*region.body.scene, "SCENE_SPIRIT_TEMPLE");
	EXPECT_EQ(region.body.timePasses, TimePasses::Yes);
	ASSERT_EQ(region.body.areas.size(), 1u);
	EXPECT_EQ(region.body.areas[0], "AREA_SPIRIT_TEMPLE");
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
	ASSERT_EQ(file.declarations.size(), 5u);
	EXPECT_TRUE(std::holds_alternative<ExternDefineDecl>(file.declarations[0]));
	EXPECT_TRUE(std::holds_alternative<DefineDecl>(file.declarations[1]));
	EXPECT_TRUE(std::holds_alternative<DefineDecl>(file.declarations[2]));
	EXPECT_TRUE(std::holds_alternative<RegionDecl>(file.declarations[3]));
	EXPECT_TRUE(std::holds_alternative<ExtendRegionDecl>(file.declarations[4]));
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
	EXPECT_EQ(call.function, "keys");
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
	EXPECT_EQ(*region.body.scene, "SCENE_SPIRIT_TEMPLE");
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
	EXPECT_EQ(m.discriminant, "distance");
	ASSERT_EQ(m.arms.size(), 3u);
	EXPECT_TRUE(m.arms[0].fallthrough);
	EXPECT_TRUE(m.arms[1].fallthrough);
	EXPECT_FALSE(m.arms[2].fallthrough);
}

TEST(ParseRealistic, SharedInRegionExit) {
	const auto file = parse(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  exits {\n"
		"    RR_TARGET: shared {\n"
		"      from RR_ROOM_A: has(RG_HOOKSHOT)\n"
		"      from here: can_use(RG_BOOMERANG)\n"
		"    }\n"
		"  }\n"
		"}\n"
	);
	const auto& region = std::get<RegionDecl>(file.declarations[0]);
	const auto& exits = region.body.sections[0];
	ASSERT_EQ(exits.entries.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(exits.entries[0].condition->node));
	const auto& sb = std::get<SharedBlock>(exits.entries[0].condition->node);
	EXPECT_FALSE(sb.anyAge);
	ASSERT_EQ(sb.branches.size(), 2u);
	EXPECT_EQ(*sb.branches[0].region, "RR_ROOM_A");
	EXPECT_FALSE(sb.branches[1].region.has_value());
}

TEST(ParseRealistic, AnyAgeInRegionExit) {
	const auto file = parse(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  exits {\n"
		"    RR_TARGET: any_age { has(RG_HOOKSHOT) or has(RG_BOOMERANG) }\n"
		"  }\n"
		"}\n"
	);
	const auto& region = std::get<RegionDecl>(file.declarations[0]);
	const auto& exits = region.body.sections[0];
	ASSERT_TRUE(std::holds_alternative<AnyAgeBlock>(exits.entries[0].condition->node));
}