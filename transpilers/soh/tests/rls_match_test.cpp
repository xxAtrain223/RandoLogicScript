#include <gtest/gtest.h>
#include "rls_match.h"

// == Basic matching ===========================================================

TEST(RlsMatch, SingleArmMatches) {
	int d = 1;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return true; }, false);
	EXPECT_TRUE(result);
}

TEST(RlsMatch, SingleArmNoMatch) {
	int d = 2;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return true; }, false);
	EXPECT_FALSE(result);
}

TEST(RlsMatch, MultipleArmsMatchSecond) {
	int d = 2;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return true; }, false,
		[&]{ return d == 2; }, [&]{ return true; }, false);
	EXPECT_TRUE(result);
}

TEST(RlsMatch, MultipleArmsNoMatch) {
	int d = 99;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return true; }, false,
		[&]{ return d == 2; }, [&]{ return true; }, false);
	EXPECT_FALSE(result);
}

// == Fallthrough ==============================================================

TEST(RlsMatch, FallthroughAccumulatesBodies) {
	// Simulates: match d { 1: A or  2: B or  3: C }
	// With d=1, result should be A || B || C.
	int d = 1;
	bool a = false, b = true, c = false;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return a; }, true,
		[&]{ return d == 2; }, [&]{ return b; }, true,
		[&]{ return d == 3; }, [&]{ return c; }, false);
	EXPECT_TRUE(result); // false || true || false
}

TEST(RlsMatch, FallthroughFromMiddle) {
	// d=2: should evaluate arms 2 and 3 only.
	int d = 2;
	bool a = true, b = false, c = true;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return a; }, true,
		[&]{ return d == 2; }, [&]{ return b; }, true,
		[&]{ return d == 3; }, [&]{ return c; }, false);
	EXPECT_TRUE(result); // false || true
}

TEST(RlsMatch, FallthroughStopsAtNonFallthrough) {
	// d=1: arms 1 (ft) → 2 (no ft) → stop. Arm 3 not evaluated.
	int d = 1;
	bool arm3Evaluated = false;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return false; }, true,
		[&]{ return d == 2; }, [&]{ return false; }, false,
		[&]{ return d == 3; }, [&]{ arm3Evaluated = true; return true; }, false);
	EXPECT_FALSE(result);
	EXPECT_FALSE(arm3Evaluated);
}

// == Short-circuit evaluation =================================================

TEST(RlsMatch, ShortCircuitsOnTrueBody) {
	// d=1: arm 1 body returns true, fallthrough → arm 2 body should NOT be
	// evaluated because of C++ || short-circuit.
	int d = 1;
	bool arm2Evaluated = false;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ return true; }, true,
		[&]{ return d == 2; }, [&]{ arm2Evaluated = true; return true; }, false);
	EXPECT_TRUE(result);
	EXPECT_FALSE(arm2Evaluated);
}

TEST(RlsMatch, NonMatchedArmsNotEvaluated) {
	// d=2: arm 1 condition fails, arm 1 body should NOT be evaluated.
	int d = 2;
	bool arm1BodyEvaluated = false;
	bool result = rls::match(
		[&]{ return d == 1; }, [&]{ arm1BodyEvaluated = true; return true; }, false,
		[&]{ return d == 2; }, [&]{ return true; }, false);
	EXPECT_TRUE(result);
	EXPECT_FALSE(arm1BodyEvaluated);
}

// == Multi-value arms (compound conditions) ===================================

TEST(RlsMatch, CompoundConditionMatchesFirst) {
	// Simulates: match d { 1 or 2: true }
	int d = 1;
	bool result = rls::match(
		[&]{ return d == 1 || d == 2; }, [&]{ return true; }, false);
	EXPECT_TRUE(result);
}

TEST(RlsMatch, CompoundConditionMatchesSecond) {
	int d = 2;
	bool result = rls::match(
		[&]{ return d == 1 || d == 2; }, [&]{ return true; }, false);
	EXPECT_TRUE(result);
}

TEST(RlsMatch, CompoundConditionNoMatch) {
	int d = 3;
	bool result = rls::match(
		[&]{ return d == 1 || d == 2; }, [&]{ return true; }, false);
	EXPECT_FALSE(result);
}

// == Realistic distance dispatch ==============================================

TEST(RlsMatch, DistanceDispatchClosest) {
	// Simulates can_hit_switch(ED_CLOSE) with 3 arms.
	// ED_CLOSE(ft) → ED_HOOKSHOT(ft) → ED_FAR(no ft)
	// All three arms contribute.
	enum Distance { ED_CLOSE, ED_HOOKSHOT, ED_FAR };

	Distance d = ED_CLOSE;
	bool hasSword = true, hasHookshot = false, hasBow = false;

	bool result = rls::match(
		[&]{ return d == ED_CLOSE; },    [&]{ return hasSword; },    true,
		[&]{ return d == ED_HOOKSHOT; }, [&]{ return hasHookshot; }, true,
		[&]{ return d == ED_FAR; },      [&]{ return hasBow; },      false);

	EXPECT_TRUE(result); // hasSword is true
}

TEST(RlsMatch, DistanceDispatchMiddle) {
	// can_hit_switch(ED_HOOKSHOT): should evaluate ED_HOOKSHOT and ED_FAR.
	enum Distance { ED_CLOSE, ED_HOOKSHOT, ED_FAR };

	Distance d = ED_HOOKSHOT;
	bool hasSword = true, hasHookshot = false, hasBow = true;

	bool result = rls::match(
		[&]{ return d == ED_CLOSE; },    [&]{ return hasSword; },    true,
		[&]{ return d == ED_HOOKSHOT; }, [&]{ return hasHookshot; }, true,
		[&]{ return d == ED_FAR; },      [&]{ return hasBow; },      false);

	EXPECT_TRUE(result); // false || true → true (from ED_FAR)
}

TEST(RlsMatch, DistanceDispatchFarthest) {
	// can_hit_switch(ED_FAR): only ED_FAR arm evaluates.
	enum Distance { ED_CLOSE, ED_HOOKSHOT, ED_FAR };

	Distance d = ED_FAR;
	bool hasSword = true, hasHookshot = true, hasBow = false;

	bool result = rls::match(
		[&]{ return d == ED_CLOSE; },    [&]{ return hasSword; },    true,
		[&]{ return d == ED_HOOKSHOT; }, [&]{ return hasHookshot; }, true,
		[&]{ return d == ED_FAR; },      [&]{ return hasBow; },      false);

	EXPECT_FALSE(result); // only hasBow, which is false
}

// == Non-bool return types ====================================================

TEST(RlsMatch, IntReturnMatches) {
	int d = 1;
	int result = rls::match(
		[&]{ return d == 1; }, [&]{ return 42; }, false);
	EXPECT_EQ(result, 42);
}

TEST(RlsMatch, IntReturnNoMatch) {
	int d = 99;
	int result = rls::match(
		[&]{ return d == 1; }, [&]{ return 42; }, false);
	EXPECT_EQ(result, 0); // value-initialized default
}

TEST(RlsMatch, IntReturnFallthroughSkipsFalsy) {
	// First arm body returns 0 (falsy), so fallthrough continues to next arm.
	int d = 1;
	int result = rls::match(
		[&]{ return d == 1; }, [&]{ return 0; },  true,
		[&]{ return d == 2; }, [&]{ return 42; }, false);
	EXPECT_EQ(result, 42);
}

TEST(RlsMatch, IntReturnFallthroughShortCircuits) {
	// First arm body returns 7 (truthy), so fallthrough returns immediately.
	int d = 1;
	bool arm2Evaluated = false;
	int result = rls::match(
		[&]{ return d == 1; }, [&]{ return 7; }, true,
		[&]{ return d == 2; }, [&]() -> int { arm2Evaluated = true; return 42; }, false);
	EXPECT_EQ(result, 7);
	EXPECT_FALSE(arm2Evaluated);
}

TEST(RlsMatch, EnumReturnMatches) {
	enum Color { None = 0, Red = 1, Blue = 2, Green = 3 };
	int d = 2;
	Color result = rls::match(
		[&]{ return d == 1; }, [&]{ return Red; },  false,
		[&]{ return d == 2; }, [&]{ return Blue; }, false);
	EXPECT_EQ(result, Blue);
}

TEST(RlsMatch, EnumReturnNoMatch) {
	enum Color { None = 0, Red = 1, Blue = 2 };
	int d = 99;
	Color result = rls::match(
		[&]{ return d == 1; }, [&]{ return Red; }, false);
	EXPECT_EQ(result, None); // value-initialized to 0
}
