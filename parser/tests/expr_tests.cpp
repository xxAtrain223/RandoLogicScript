#include <gtest/gtest.h>

#include "grammar.h"

#include <tao/pegtl.hpp>

using namespace rls::parser::grammar;

// == Helper ===================================================================

/// Returns true if the rule matches the ENTIRE input string.
template <typename Rule>
bool matches(const std::string& input) {
	tao::pegtl::memory_input in(input, "test");
	try {
		return tao::pegtl::parse<tao::pegtl::must<Rule, tao::pegtl::eof>>(in);
	} catch (const tao::pegtl::parse_error&) {
		return false;
	}
}

// == Atoms ====================================================================

TEST(ExprAtom, BoolKeywordTrue) {
	EXPECT_TRUE(matches<atom>("true"));
}

TEST(ExprAtom, BoolKeywordFalse) {
	EXPECT_TRUE(matches<atom>("false"));
}

TEST(ExprAtom, BoolKeywordAlways) {
	EXPECT_TRUE(matches<atom>("always"));
}

TEST(ExprAtom, BoolKeywordNever) {
	EXPECT_TRUE(matches<atom>("never"));
}

TEST(ExprAtom, AgeIsChild) {
	EXPECT_TRUE(matches<atom>("is_child"));
}

TEST(ExprAtom, AgeIsAdult) {
	EXPECT_TRUE(matches<atom>("is_adult"));
}

TEST(ExprAtom, TimeAtDay) {
	EXPECT_TRUE(matches<atom>("at_day"));
}

TEST(ExprAtom, TimeAtNight) {
	EXPECT_TRUE(matches<atom>("at_night"));
}

TEST(ExprAtom, DungeonIsVanilla) {
	EXPECT_TRUE(matches<atom>("is_vanilla"));
}

TEST(ExprAtom, DungeonIsMq) {
	EXPECT_TRUE(matches<atom>("is_mq"));
}

TEST(ExprAtom, Identifier) {
	EXPECT_TRUE(matches<atom>("RG_HOOKSHOT"));
}

TEST(ExprAtom, Integer) {
	EXPECT_TRUE(matches<atom>("42"));
}

TEST(ExprAtom, NegativeInteger) {
	EXPECT_TRUE(matches<atom>("-1"));
}

// == Primary expressions ======================================================

TEST(ExprPrimary, Atom) {
	EXPECT_TRUE(matches<primary>("always"));
}

TEST(ExprPrimary, ParenthesisedAtom) {
	EXPECT_TRUE(matches<primary>("(always)"));
}

TEST(ExprPrimary, ParenthesisedWithWhitespace) {
	EXPECT_TRUE(matches<primary>("( RG_HOOKSHOT )"));
}

TEST(ExprPrimary, NestedParens) {
	EXPECT_TRUE(matches<primary>("((42))"));
}

TEST(ExprPrimary, MissingClosingParenFails) {
    EXPECT_FALSE(matches<primary>("(true"));
}

// == Calls ====================================================================

TEST(ExprCall, NoArgs) {
	EXPECT_TRUE(matches<call>("has_explosives()"));
}

TEST(ExprCall, SingleArg) {
	EXPECT_TRUE(matches<call>("has(RG_HOOKSHOT)"));
}

TEST(ExprCall, MultipleArgs) {
	EXPECT_TRUE(matches<call>("can_kill(RE_ARMOS, ED_CLOSE, false)"));
}

TEST(ExprCall, NamedArg) {
	EXPECT_TRUE(matches<call>("can_kill(RE_ARMOS, distance: ED_CLOSE)"));
}

TEST(ExprCall, AllNamedArgs) {
	EXPECT_TRUE(matches<call>("foo(x: 1, y: 2)"));
}

TEST(ExprCall, NestedCalls) {
	EXPECT_TRUE(matches<call>("can_use(setting(RSK_FOO))"));
}

TEST(ExprCall, WhitespaceBetweenArgs) {
	EXPECT_TRUE(matches<call>("foo( x , y , z )"));
}

TEST(ExprCall, CallAsPrimary) {
	EXPECT_TRUE(matches<primary>("has(RG_HOOKSHOT)"));
}

TEST(ExprCall, EmptyParensFails) {
	// Tests that an identifier without parens is NOT a call
	EXPECT_FALSE(matches<call>("has_explosives"));
}

// == Unary ====================================================================

TEST(ExprUnary, NotAtom) {
	EXPECT_TRUE(matches<unary>("not always"));
}

TEST(ExprUnary, NotIdentifier) {
	EXPECT_TRUE(matches<unary>("not RG_HOOKSHOT"));
}

TEST(ExprUnary, DoubleNot) {
	EXPECT_TRUE(matches<unary>("not not true"));
}

TEST(ExprUnary, NotCall) {
	EXPECT_TRUE(matches<unary>("not has(RG_HOOKSHOT)"));
}

TEST(ExprUnary, NotParen) {
	EXPECT_TRUE(matches<unary>("not (true)"));
}

TEST(ExprUnary, BareAtomIsUnary) {
	// A plain atom is valid as unary (no "not")
	EXPECT_TRUE(matches<unary>("true"));
}

// == Mul / Div ================================================================

TEST(ExprMulDiv, Multiply) {
	EXPECT_TRUE(matches<mul_div>("2 * 3"));
}

TEST(ExprMulDiv, Divide) {
	EXPECT_TRUE(matches<mul_div>("10 / 2"));
}

TEST(ExprMulDiv, Chain) {
	EXPECT_TRUE(matches<mul_div>("2 * 3 / 4"));
}

TEST(ExprMulDiv, NoOp) {
	// Single unary is valid mul_div
	EXPECT_TRUE(matches<mul_div>("42"));
}

TEST(ExprMulDiv, WithCalls) {
	EXPECT_TRUE(matches<mul_div>("stone_count() * 2"));
}

// == Add / Sub ================================================================

TEST(ExprAddSub, Add) {
	EXPECT_TRUE(matches<add_sub>("1 + 2"));
}

TEST(ExprAddSub, Subtract) {
	EXPECT_TRUE(matches<add_sub>("10 - 3"));
}

TEST(ExprAddSub, Chain) {
	EXPECT_TRUE(matches<add_sub>("1 + 2 - 3 + 4"));
}

TEST(ExprAddSub, MulDivPrecedence) {
	EXPECT_TRUE(matches<add_sub>("1 + 2 * 3"));
}

TEST(ExprAddSub, NoOp) {
	EXPECT_TRUE(matches<add_sub>("42"));
}

// == Comparison ===============================================================

TEST(ExprComparison, EqualSymbol) {
	EXPECT_TRUE(matches<comparison>("x == 3"));
}

TEST(ExprComparison, NotEqualSymbol) {
	EXPECT_TRUE(matches<comparison>("x != 3"));
}

TEST(ExprComparison, GreaterThan) {
	EXPECT_TRUE(matches<comparison>("x > 3"));
}

TEST(ExprComparison, LessThan) {
	EXPECT_TRUE(matches<comparison>("x < 3"));
}

TEST(ExprComparison, GreaterEqual) {
	EXPECT_TRUE(matches<comparison>("x >= 3"));
}

TEST(ExprComparison, LessEqual) {
	EXPECT_TRUE(matches<comparison>("x <= 3"));
}

TEST(ExprComparison, IsKeyword) {
	EXPECT_TRUE(matches<comparison>("setting(RSK_FOREST) is RO_CLOSED_FOREST_ON"));
}

TEST(ExprComparison, IsNotKeyword) {
	EXPECT_TRUE(matches<comparison>("setting(RSK_FOREST) is not RO_CLOSED_FOREST_ON"));
}

TEST(ExprComparison, NoOp) {
	EXPECT_TRUE(matches<comparison>("42"));
}

TEST(ExprComparison, WithArithmetic) {
	EXPECT_TRUE(matches<comparison>("stone_count() + 1 >= 3"));
}

// == And ======================================================================

TEST(ExprAnd, TwoOperands) {
	EXPECT_TRUE(matches<and_expr>("is_adult and has(RG_HOOKSHOT)"));
}

TEST(ExprAnd, ThreeOperands) {
	EXPECT_TRUE(matches<and_expr>("is_adult and has(RG_HOOKSHOT) and at_day"));
}

TEST(ExprAnd, WithComparison) {
	EXPECT_TRUE(matches<and_expr>("x > 3 and y < 5"));
}

TEST(ExprAnd, NoOp) {
	EXPECT_TRUE(matches<and_expr>("true"));
}

// == Or =======================================================================

TEST(ExprOr, TwoOperands) {
	EXPECT_TRUE(matches<or_expr>("has(RG_HOOKSHOT) or has(RG_BOOMERANG)"));
}

TEST(ExprOr, ThreeOperands) {
	EXPECT_TRUE(matches<or_expr>("a or b or c"));
}

TEST(ExprOr, WithAnd) {
	// And binds tighter than Or
	EXPECT_TRUE(matches<or_expr>("a and b or c and d"));
}

TEST(ExprOr, NoOp) {
	EXPECT_TRUE(matches<or_expr>("true"));
}

// == Ternary ==================================================================

TEST(ExprTernary, Simple) {
	EXPECT_TRUE(matches<ternary>("is_adult ? 1 : 2"));
}

TEST(ExprTernary, WithExpressions) {
	EXPECT_TRUE(matches<ternary>("has_explosives() ? 1 : 2"));
}

TEST(ExprTernary, Nested) {
	EXPECT_TRUE(matches<ternary>("a ? b ? 1 : 2 : 3"));
}

TEST(ExprTernary, ElseTernary) {
    EXPECT_TRUE(matches<ternary>("a ? 1 : b ? 2 : 3"));
}

TEST(ExprTernary, NoOp) {
	EXPECT_TRUE(matches<ternary>("true"));
}

TEST(ExprTernary, WithComparison) {
	EXPECT_TRUE(matches<ternary>("stone_count() >= 3 ? true : false"));
}

// == Full expr ================================================================

TEST(Expr, SimpleAtom) {
	EXPECT_TRUE(matches<expr>("true"));
}

TEST(Expr, Identifier) {
	EXPECT_TRUE(matches<expr>("RG_HOOKSHOT"));
}

TEST(Expr, Integer) {
	EXPECT_TRUE(matches<expr>("42"));
}

TEST(Expr, NotExpr) {
	EXPECT_TRUE(matches<expr>("not true"));
}

TEST(Expr, OrExpr) {
	EXPECT_TRUE(matches<expr>("has(RG_HOOKSHOT) or has(RG_BOOMERANG)"));
}

TEST(Expr, AndExpr) {
	EXPECT_TRUE(matches<expr>("is_adult and has(RG_HOOKSHOT)"));
}

TEST(Expr, MixedBoolOps) {
	EXPECT_TRUE(matches<expr>("a and b or c and d"));
}

TEST(Expr, FullComplex) {
	EXPECT_TRUE(matches<expr>(
		"is_adult and (has(RG_HOOKSHOT) or has(RG_LONGSHOT)) and stone_count() >= 3"
	));
}

TEST(Expr, TernaryExpr) {
	EXPECT_TRUE(matches<expr>("has_explosives() ? 1 : 2"));
}

TEST(Expr, NestedCalls) {
	EXPECT_TRUE(matches<expr>("can_use(setting(RSK_FOO))"));
}

TEST(Expr, NotWithCall) {
	EXPECT_TRUE(matches<expr>("not setting(RSK_SUNLIGHT_ARROWS)"));
}

TEST(Expr, SettingIsComparison) {
	EXPECT_TRUE(matches<expr>("setting(RSK_FOREST) is RO_CLOSED_FOREST_ON"));
}

TEST(Expr, SettingIsNotComparison) {
	EXPECT_TRUE(matches<expr>("setting(RSK_FOREST) is not RO_CLOSED_FOREST_ON"));
}

TEST(Expr, ComparisonWithArithmetic) {
	EXPECT_TRUE(matches<expr>("fire_timer() >= 48"));
}

TEST(Expr, ArithmeticChain) {
	EXPECT_TRUE(matches<expr>("1 + 2 * 3 - 4 / 2"));
}

TEST(Expr, ParenGrouping) {
	EXPECT_TRUE(matches<expr>("(a or b) and (c or d)"));
}

TEST(Expr, EmptyFails) {
	EXPECT_FALSE(matches<expr>(""));
}

TEST(Expr, OperatorAloneFails) {
	EXPECT_FALSE(matches<expr>("and"));
}

TEST(Expr, TrailingOpFails) {
	EXPECT_FALSE(matches<expr>("a and"));
}

// == Shared blocks ============================================================

TEST(ExprShared, SimpleBlock) {
	EXPECT_TRUE(matches<shared_block>(
		"shared { from RR_ROOM_A: has(RG_HOOKSHOT) }"
	));
}

TEST(ExprShared, MultipleBranches) {
	EXPECT_TRUE(matches<shared_block>(
		"shared { from RR_ROOM_A: true from RR_ROOM_B: false }"
	));
}

TEST(ExprShared, WithHere) {
	EXPECT_TRUE(matches<shared_block>(
		"shared { from here: always }"
	));
}

TEST(ExprShared, WithAnyAge) {
	EXPECT_TRUE(matches<shared_block>(
		"shared any_age { from RR_ROOM_A: true }"
	));
}

TEST(ExprShared, WithNewlines) {
	EXPECT_TRUE(matches<shared_block>(
		"shared {\n"
		"  from RR_ROOM_A: has(RG_HOOKSHOT)\n"
		"  from here: always\n"
		"}"
	));
}

TEST(ExprShared, AsPrimary) {
	EXPECT_TRUE(matches<primary>(
		"shared { from here: true }"
	));
}

TEST(ExprShared, InOrExpr) {
	EXPECT_TRUE(matches<expr>(
		"shared { from here: true } or has(RG_LONGSHOT)"
	));
}

// == Any-age blocks ===========================================================

TEST(ExprAnyAge, SimpleBlock) {
	EXPECT_TRUE(matches<any_age_block>(
		"any_age { has(RG_HOOKSHOT) }"
	));
}

TEST(ExprAnyAge, WithComplexExpr) {
	EXPECT_TRUE(matches<any_age_block>(
		"any_age { is_adult and has(RG_HOOKSHOT) or is_child and has(RG_BOOMERANG) }"
	));
}

TEST(ExprAnyAge, AsPrimary) {
	EXPECT_TRUE(matches<primary>(
		"any_age { true }"
	));
}

// == Match expressions ========================================================

TEST(ExprMatch, SingleArm) {
	EXPECT_TRUE(matches<match_expr>(
		"match distance { ED_CLOSE: true }"
	));
}

TEST(ExprMatch, MultipleArms) {
	EXPECT_TRUE(matches<match_expr>(
		"match distance {\n"
		"  ED_CLOSE: can_use(RG_KOKIRI_SWORD)\n"
		"  ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"}"
	));
}

TEST(ExprMatch, PatternWithOr) {
	EXPECT_TRUE(matches<match_expr>(
		"match distance { ED_CLOSE or ED_MID: true }"
	));
}

TEST(ExprMatch, TrailingOr) {
	EXPECT_TRUE(matches<match_expr>(
		"match distance {\n"
		"  ED_CLOSE: can_use(RG_KOKIRI_SWORD) or\n"
		"  ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"}"
	));
}

TEST(ExprMatch, ComplexArmExpr) {
	EXPECT_TRUE(matches<match_expr>(
		"match distance {\n"
		"  ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or can_use(RG_MEGATON_HAMMER) or\n"
		"  ED_CLOSE: has_explosives() or\n"
		"  ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)\n"
		"}"
	));
}

TEST(ExprMatch, AsPrimary) {
	EXPECT_TRUE(matches<primary>(
		"match x { A: true }"
	));
}

TEST(ExprMatch, FinalTrailingOr) {
    EXPECT_FALSE(matches<match_expr>(
        "match distance {\n"
        "  ED_CLOSE: can_use(RG_KOKIRI_SWORD) or\n"
        "  ED_FAR: can_use(RG_FAIRY_BOW) or\n"
        "}"
    ));
}

// == Realistic RLS expressions ================================================

TEST(ExprRealistic, HasExplosives) {
	EXPECT_TRUE(matches<expr>(
		"has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)"
	));
}

TEST(ExprRealistic, SpiritExitCondition) {
	EXPECT_TRUE(matches<expr>(
		"(flag(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE) and can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false))"
		" or can_use(RG_HOVER_BOOTS)"
		" or can_use(RG_LONGSHOT)"
	));
}

TEST(ExprRealistic, KeysWithTernary) {
	EXPECT_TRUE(matches<expr>(
		"keys(SCENE_SPIRIT_TEMPLE, has_explosives() ? 1 : 2)"
	));
}

TEST(ExprRealistic, SettingComparison) {
	EXPECT_TRUE(matches<expr>(
		"setting(RSK_FOREST) is RO_CLOSED_FOREST_ON and has(RG_KOKIRI_EMERALD)"
	));
}

TEST(ExprRealistic, AgeAndAbility) {
	EXPECT_TRUE(matches<expr>(
		"(is_adult and trick(RT_SPIRIT_STATUE_JUMP))"
		" or can_use(RG_HOVER_BOOTS)"
		" or (can_use(RG_ZELDAS_LULLABY) and can_use(RG_HOOKSHOT))"
	));
}

TEST(ExprRealistic, MultiNot) {
	EXPECT_TRUE(matches<expr>(
		"not setting(RSK_SUNLIGHT_ARROWS) and is_adult"
	));
}

TEST(ExprRealistic, EffectiveHealthCheck) {
	EXPECT_TRUE(matches<expr>(
		"effective_health() > 2 and fire_timer() >= 48"
	));
}

TEST(ExprRealistic, SharedInExpr) {
	EXPECT_TRUE(matches<expr>(
		"shared {\n"
		"  from here: can_use(RG_BOOMERANG)\n"
		"  from RR_SPIRIT_TEMPLE_GS_LEDGE: has(RG_HOOKSHOT)\n"
		"}"
	));
}

TEST(ExprRealistic, AnyAgeInExpr) {
	EXPECT_TRUE(matches<expr>(
		"any_age { has(RG_HOOKSHOT) or has(RG_BOOMERANG) }"
	));
}

TEST(ExprRealistic, MatchInExpr) {
	EXPECT_TRUE(matches<expr>(
		"match distance {\n"
		"  ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or can_use(RG_MEGATON_HAMMER) or\n"
		"  ED_CLOSE: has_explosives() or\n"
		"  ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)\n"
		"}"
	));
}
