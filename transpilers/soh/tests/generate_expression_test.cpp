#include "helpers.h"

using namespace rls::transpilers::soh_tests;

namespace {
struct ResolvedExpression {
	rls::ast::Project project;
	rls::ast::ExprPtr expr;
};
}

static std::string GenerateExpression(const ResolvedExpression& resolved) {
	return rls::transpilers::soh::SohTranspiler(resolved.project).GenerateExpression(resolved.expr);
}

ResolvedExpression sourceToExpression(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	auto defineDecl = project.DefineDecls.find(defineName);
	if (defineDecl == project.DefineDecls.end()) {
		return { std::move(project), nullptr };
	}

	return {
		std::move(project),
		std::move(const_cast<rls::ast::DefineDecl*>(defineDecl->second)->body)
	};
}

ResolvedExpression sourceToRegionExpression(
	const std::string& source,
	const std::string& regionName,
	rls::ast::SectionKind sectionKind,
	const std::string& entryName)
{
	auto project = resolveFromSource(source);
	auto it = project.RegionDecls.find(regionName);
	if (it == project.RegionDecls.end()) {
		return { std::move(project), nullptr };
	}

	auto* region = const_cast<rls::ast::RegionDecl*>(it->second);
	for (auto& section : region->body.sections) {
		if (section.kind != sectionKind) continue;
		for (auto& entry : section.entries) {
			if (entry.name.text == entryName) {
				return { std::move(project), std::move(entry.condition) };
			}
		}
	}

	return { std::move(project), nullptr };
}

TEST(SohExpressions, BoolLiteral) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    true\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true");

	expr = sourceToExpression(
		"define test():\n"
		"    always\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true");

	expr = sourceToExpression(
		"define test():\n"
		"    false\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"false");

	expr = sourceToExpression(
		"define test():\n"
		"    never\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"false");
}

TEST(SohExpressions, IntLiteral) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    42\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"42");

	expr = sourceToExpression(
		"define test():\n"
		"    -7\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"-7");
}

TEST(SohExpressions, Identifier) {
	auto expr = sourceToExpression(
		"define test(item: Item):\n"
		"    item\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"item");
}

TEST(SohExpressions, UnaryNot) {
	auto expr = sourceToExpression(
		"define test(value: Bool):\n"
		"    not value\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"!value");
}

TEST(SohExpressions, BinaryAndOr) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    true and false\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true && false");

	expr = sourceToExpression(
		"define test():\n"
		"    true or false\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true || false");
}

TEST(SohExpressions, BinaryPrecedenceParens) {
	auto expr = sourceToExpression(
		"define test(a: Bool, b: Bool, c: Bool):\n"
		"    (a or b) and c\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"(a || b) && c");

	expr = sourceToExpression(
		"define test(a: Bool, b: Bool, c: Bool):\n"
		"    a and (b or c)\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"a && (b || c)");

	expr = sourceToExpression(
		"define test(a: Bool, b: Bool, c: Bool):\n"
		"    a and b or c\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"a && b || c");

	expr = sourceToExpression(
		"define test(a: Int, b: Int, c: Int):\n"
		"    (a + b) * c\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"(a + b) * c");

	expr = sourceToExpression(
		"define test(a: Int, b: Int, c: Int):\n"
		"    a - (b - c)\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"a - (b - c)");

	expr = sourceToExpression(
		"define test(a: Int, b: Int, c: Int):\n"
		"    a - b - c\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"a - b - c");
}

TEST(SohExpressions, UnaryNotPrecedenceParens) {
	auto expr = sourceToExpression(
		"define test(a: Bool, b: Bool):\n"
		"    not (a or b)\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"!(a || b)");

	expr = sourceToExpression(
		"define test(a: Bool, b: Bool):\n"
		"    not (a and b)\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"!(a && b)");
}

TEST(SohExpressions, BinaryComparisons) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 == 2\n",
		"test")),
		"1 == 2");


	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 is 2\n",
		"test")),
		"1 == 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 != 2\n",
		"test")),
		"1 != 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 is not 2\n",
		"test")),
		"1 != 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 < 2\n",
		"test")),
		"1 < 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 <= 2\n",
		"test")),
		"1 <= 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    2 > 1\n",
		"test")),
		"2 > 1");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    2 >= 1\n",
		"test")),
		"2 >= 1");
}

TEST(SohExpressions, BinaryArithmetic) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 + 2\n",
		"test")),
		"1 + 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    5 - 3\n",
		"test")),
		"5 - 3");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    4 * 2\n",
		"test")),
		"4 * 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    8 / 2\n",
		"test")),
		"8 / 2");
}

TEST(SohExpressions, TernarySimple) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    true ? 1 : 2\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true ? 1 : 2");
}

TEST(SohExpressions, TernaryWithIdentifier) {
	auto expr = sourceToExpression(
		"define test(item: Item):\n"
		"    item is RG_SILVER_GAUNTLETS ? 1 : 0\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"item == RandomizerGet::RG_SILVER_GAUNTLETS ? 1 : 0");
}

TEST(SohExpressions, TernaryNestedRightAssociative) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    1 ? 2 ? 3 : 4 : 5\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"1 ? 2 ? 3 : 4 : 5");
}

TEST(SohExpressions, CallHostFunctions) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT)\n",
		"test")),
		"has(RandomizerGet::RG_HOOKSHOT)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    can_use(RG_HOOKSHOT)\n",
		"test")),
		"can_use(RandomizerGet::RG_HOOKSHOT)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    keys(SCENE_SPIRIT_TEMPLE, 3)\n",
		"test")),
		"keys(SceneID::SCENE_SPIRIT_TEMPLE, 3)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    flag(LOGIC_SPIRIT_PLATFORM_LOWERED)\n",
		"test")),
		"flag(LogicVal::LOGIC_SPIRIT_PLATFORM_LOWERED)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_SUNLIGHT_ARROWS)\n",
		"test")),
		"setting(RSK_SUNLIGHT_ARROWS)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    trick(RT_SPIRIT_CHILD_CHU)\n",
		"test")),
		"trick(RandomizerTrick::RT_SPIRIT_CHILD_CHU)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    hearts()\n",
		"test")),
		"hearts()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    effective_health()\n",
		"test")),
		"effective_health()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    trial_skipped(TK_LIGHT_TRIAL)\n",
		"test")),
		"trial_skipped(TrialKey::TK_LIGHT_TRIAL)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    check_price(RC_KF_SHOP_ITEM_1)\n",
		"test")),
		"check_price(RandomizerCheck::RC_KF_SHOP_ITEM_1)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    can_plant_bean(RR_KOKIRI_FOREST, RG_KOKIRI_FOREST_BEAN_SOUL)\n",
		"test")),
		"can_plant_bean(RandomizerRegion::RR_KOKIRI_FOREST, RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    triforce_pieces()\n",
		"test")),
		"triforce_pieces()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    big_poes()\n",
		"test")),
		"big_poes()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    any_age(has(RG_HOOKSHOT) or can_use(RG_BOOMERANG))\n",
		"test")),
		"any_age([]{return has(RandomizerGet::RG_HOOKSHOT) || can_use(RandomizerGet::RG_BOOMERANG);})");
}

TEST(SohExpressions, CallExternDefineReorderedAndDefaultedArgs) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"extern define host_custom(item: Item, distance: Distance = ED_CLOSE, enabled: Bool = false) -> Bool\n"
		"define test():\n"
		"    host_custom(distance: ED_FAR, item: RG_HOOKSHOT) and host_custom(RG_FAIRY_BOW)\n",
		"test")),
		"host_custom(RandomizerGet::RG_HOOKSHOT, EnemyDistance::ED_FAR, false) && host_custom(RandomizerGet::RG_FAIRY_BOW, EnemyDistance::ED_CLOSE, false)");
}

TEST(SohExpressions, CallEnemyFunctions) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define kill_fn(e: Enemy, distance = ED_CLOSE, wallOrFloor = true): always\n"
		"define pass_fn(e: Enemy): always\n"
		"define drop_fn(e: Enemy): always\n"
		"define avoid_fn(e: Enemy): always\n"
		"define test():\n"
		"    kill_fn(RE_GOLD_SKULLTULA, ED_CLOSE, wallOrFloor: false) or\n"
		"    pass_fn(RE_GOLD_SKULLTULA) or\n"
		"    drop_fn(RE_GOLD_SKULLTULA) or\n"
		"    avoid_fn(RE_GOLD_SKULLTULA)\n",
		"test")),
		"kill_fn(RandomizerEnemy::RE_GOLD_SKULLTULA, EnemyDistance::ED_CLOSE, false) || "
		"pass_fn(RandomizerEnemy::RE_GOLD_SKULLTULA) || "
		"drop_fn(RandomizerEnemy::RE_GOLD_SKULLTULA) || "
		"avoid_fn(RandomizerEnemy::RE_GOLD_SKULLTULA)");
}

TEST(SohExpressions, CallDefinedFunctions) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define HookshotOrBoomerang():\n"
		"    can_use(RG_HOOKSHOT) or can_use(RG_BOOMERANG)\n"
		"\n"
		"define test():\n"
		"    HookshotOrBoomerang()\n",
		"test")),
		"HookshotOrBoomerang()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define CanSpawnSoilSkull(bean: Item):\n"
		"    can_use(RG_BOTTLE_WITH_BUGS) and has(bean)\n"
		"\n"
		"define test():\n"
		"    CanSpawnSoilSkull(RG_KOKIRI_FOREST_BEAN_SOUL)\n",
		"test")),
		"CanSpawnSoilSkull(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define CanHitSwitch(distance = ED_CLOSE, inWater = false):\n"
		"    match distance {\n"
		"        ED_CLOSE or\n"
		"        ED_SHORT_JUMPSLASH:\n"
		"            can_use(RG_KOKIRI_SWORD) or can_use(RG_MEGATON_HAMMER) or can_use(RG_GIANTS_KNIFE) or\n"
		"        ED_MASTER_SWORD_JUMPSLASH:\n"
		"            can_use(RG_MASTER_SWORD) or\n"
		"        ED_LONG_JUMPSLASH:\n"
		"            can_use(RG_BIGGORON_SWORD) or can_use(RG_STICKS) or\n"
		"        ED_BOMB_THROW:\n"
		"            can_use(RG_BOMB_BAG) and not inWater or\n"
		"        ED_BOOMERANG:\n"
		"            can_use(RG_BOOMERANG) or\n"
		"        ED_HOOKSHOT:\n"
		"            # RANDOTODO test chu range in a practical example\n"
		"            can_use(RG_HOOKSHOT) or can_use(RG_BOMBCHU_5) or\n"
		"        ED_LONGSHOT:\n"
		"            can_use(RG_LONGSHOT) or\n"
		"        ED_FAR:\n"
		"            can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)\n"
		"    }\n"
		"\n"
		"define test():\n"
		"    CanHitSwitch(ED_BOOMERANG)\n",
		"test")),
		"CanHitSwitch(EnemyDistance::ED_BOOMERANG, false)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define gate(cond: Condition):\n"
		"    cond() and has(RG_HOOKSHOT)\n"
		"\n"
		"define test():\n"
		"    gate(has(RG_BOOMERANG))\n",
		"test")),
		"gate([]{return has(RandomizerGet::RG_BOOMERANG);})");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define gate(cond: Condition):\n"
		"    any_age(cond)\n",
		"gate")),
		"any_age(cond)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define make_cond(cond: Condition):\n"
		"    cond\n"
		"\n"
		"define test(cond: Condition):\n"
		"    make_cond(cond)()\n",
		"test")),
		"make_cond(cond)()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define always_true():\n"
		"    true\n"
		"\n"
		"define return_cond():\n"
		"    always_true\n",
		"return_cond")),
		"always_true");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define always_true():\n"
		"    true\n"
		"\n"
		"define return_cond():\n"
		"    always_true\n"
		"\n"
		"define test():\n"
		"    return_cond()\n",
		"test")),
		"return_cond()");
}

TEST(SohExpressions, CallConditionParametersAndDefaults) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define both(left: Condition, right: Condition):\n"
		"    left() and right()\n"
		"\n"
		"define test():\n"
		"    both(has(RG_HOOKSHOT) or can_use(RG_BOOMERANG), has(RG_FAIRY_BOW))\n",
		"test")),
		"both([]{return has(RandomizerGet::RG_HOOKSHOT) || can_use(RandomizerGet::RG_BOOMERANG);}, []{return has(RandomizerGet::RG_FAIRY_BOW);})");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define always_true():\n"
		"    true\n"
		"\n"
		"define gate(cond: Condition = always_true):\n"
		"    cond()\n"
		"\n"
		"define test():\n"
		"    gate()\n",
		"test")),
		"gate(always_true)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define always_true():\n"
		"    true\n"
		"\n"
		"define gate(cond: Condition = always_true):\n"
		"    cond()\n"
		"\n"
		"define test():\n"
		"    gate(can_use(RG_BOOMERANG))\n",
		"test")),
		"gate([]{return can_use(RandomizerGet::RG_BOOMERANG);})");
}

// == here keyword =============================================================

TEST(SohExpressions, HereEmitsCurrentRegion) {
	auto expr = sourceToRegionExpression(
		"region RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD {\n"
		"    name: \"Spirit Temple Statue Room Child\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_OTHER: can_plant_bean(here, RG_KOKIRI_FOREST_BEAN_SOUL)\n"
		"    }\n"
		"}\n",
		"RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD",
		rls::ast::SectionKind::Exits,
		"RR_OTHER");
	EXPECT_EQ(GenerateExpression(expr),
		"can_plant_bean(RandomizerRegion::RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD, "
		"RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL)");
}

// == Match expression =========================================================

TEST(SohExpressions, MatchSingleArm) {
	auto expr = sourceToExpression(
		"define test(distance: Distance):\n"
		"    match distance {\n"
		"        ED_CLOSE: can_use(RG_KOKIRI_SWORD)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"rls::match("
		"[&]{return distance == EnemyDistance::ED_CLOSE;}, "
		"[&]{return can_use(RandomizerGet::RG_KOKIRI_SWORD);}, false)");
}

TEST(SohExpressions, MatchMultipleArmsNoFallthrough) {
	auto expr = sourceToExpression(
		"define test(distance: Distance):\n"
		"    match distance {\n"
		"        ED_CLOSE: can_use(RG_KOKIRI_SWORD)\n"
		"        ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"rls::match("
		"[&]{return distance == EnemyDistance::ED_CLOSE;}, "
		"[&]{return can_use(RandomizerGet::RG_KOKIRI_SWORD);}, false, "
		"[&]{return distance == EnemyDistance::ED_FAR;}, "
		"[&]{return can_use(RandomizerGet::RG_FAIRY_BOW);}, false)");
}

TEST(SohExpressions, MatchWithFallthrough) {
	auto expr = sourceToExpression(
		"define test(distance: Distance):\n"
		"    match distance {\n"
		"        ED_CLOSE: can_use(RG_KOKIRI_SWORD) or\n"
		"        ED_HOOKSHOT: can_use(RG_HOOKSHOT) or\n"
		"        ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"rls::match("
		"[&]{return distance == EnemyDistance::ED_CLOSE;}, "
		"[&]{return can_use(RandomizerGet::RG_KOKIRI_SWORD);}, true, "
		"[&]{return distance == EnemyDistance::ED_HOOKSHOT;}, "
		"[&]{return can_use(RandomizerGet::RG_HOOKSHOT);}, true, "
		"[&]{return distance == EnemyDistance::ED_FAR;}, "
		"[&]{return can_use(RandomizerGet::RG_FAIRY_BOW);}, false)");
}

TEST(SohExpressions, MatchMultiValueArm) {
	auto expr = sourceToExpression(
		"define test(distance: Distance):\n"
		"    match distance {\n"
		"        ED_CLOSE or ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"rls::match("
		"[&]{return distance == EnemyDistance::ED_CLOSE || distance == EnemyDistance::ED_SHORT_JUMPSLASH;}, "
		"[&]{return can_use(RandomizerGet::RG_KOKIRI_SWORD);}, false)");
}

TEST(SohExpressions, MatchDefaultArm) {
	auto expr = sourceToExpression(
		"define test(distance: Distance):\n"
		"    match distance {\n"
		"        ED_CLOSE: can_use(RG_KOKIRI_SWORD)\n"
		"        _: can_use(RG_FAIRY_BOW)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"rls::match("
		"[&]{return distance == EnemyDistance::ED_CLOSE;}, "
		"[&]{return can_use(RandomizerGet::RG_KOKIRI_SWORD);}, false, "
		"[&]{return true;}, "
		"[&]{return can_use(RandomizerGet::RG_FAIRY_BOW);}, false)");
}

TEST(SohExpressions, MatchComplexCanHitSwitch) {
	auto expr = sourceToExpression(
		"define test(distance = ED_CLOSE, inWater = false):\n"
		"    match distance {\n"
		"        ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or\n"
		"        ED_BOMB_THROW: not inWater and can_use(RG_BOMB_BAG) or\n"
		"        ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"rls::match("
		"[&]{return distance == EnemyDistance::ED_SHORT_JUMPSLASH;}, "
		"[&]{return can_use(RandomizerGet::RG_KOKIRI_SWORD);}, true, "
		"[&]{return distance == EnemyDistance::ED_BOMB_THROW;}, "
		"[&]{return !inWater && can_use(RandomizerGet::RG_BOMB_BAG);}, true, "
		"[&]{return distance == EnemyDistance::ED_FAR;}, "
		"[&]{return can_use(RandomizerGet::RG_FAIRY_BOW);}, false)");
}

