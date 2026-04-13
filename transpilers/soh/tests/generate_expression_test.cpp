#include "helpers.h"
#include "generate_expression.h"

using rls::transpilers::soh::GenerateExpression;
using namespace rls::transpilers::soh_tests;

rls::ast::ExprPtr sourceToExpression(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	const auto defineDecl = project.DefineDecls.find(defineName);
	return defineDecl != project.DefineDecls.end()
		? std::move(const_cast<rls::ast::DefineDecl*>(defineDecl->second)->body)
		: nullptr;
}

rls::ast::ExprPtr sourceToRegionExpression(
	const std::string& source,
	const std::string& regionName,
	rls::ast::SectionKind sectionKind,
	const std::string& entryName)
{
	auto project = resolveFromSource(source);
	const auto it = project.RegionDecls.find(regionName);
	if (it == project.RegionDecls.end()) return nullptr;

	auto* region = const_cast<rls::ast::RegionDecl*>(it->second);
	for (auto& section : region->body.sections) {
		if (section.kind != sectionKind) continue;
		for (auto& entry : section.entries) {
			if (entry.name == entryName) {
				return std::move(entry.condition);
			}
		}
	}
	return nullptr;
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

TEST(SohExpressions, IsChildIsAdultKeywords) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    is_child\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsChild");

	expr = sourceToExpression(
		"define test():\n"
		"    is_adult\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsAdult");
}

TEST(SohExpressions, AtDayAtNightKeywords) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    at_day\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->AtDay");

	expr = sourceToExpression(
		"define test():\n"
		"    at_night\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->AtNight");
}

TEST(SohExpressions, IsVanillaIsMqKeywords) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    is_vanilla\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsVanilla()");

	expr = sourceToExpression(
		"define test():\n"
		"    is_mq\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsMQ()");
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
		"item == RG_SILVER_GAUNTLETS ? 1 : 0");
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
		"logic->HasItem(RG_HOOKSHOT)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    can_use(RG_HOOKSHOT)\n",
		"test")),
		"logic->CanUse(RG_HOOKSHOT)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    keys(SCENE_SPIRIT_TEMPLE, 3)\n",
		"test")),
		"logic->SmallKeys(SCENE_SPIRIT_TEMPLE, 3)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    flag(LOGIC_SPIRIT_PLATFORM_LOWERED)\n",
		"test")),
		"logic->Get(LOGIC_SPIRIT_PLATFORM_LOWERED)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    setting(RSK_SUNLIGHT_ARROWS)\n",
		"test")),
		"ctx->GetOption(RSK_SUNLIGHT_ARROWS)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    trick(RT_SPIRIT_CHILD_CHU)\n",
		"test")),
		"ctx->GetTrickOption(RT_SPIRIT_CHILD_CHU)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    hearts()\n",
		"test")),
		"logic->Hearts()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    effective_health()\n",
		"test")),
		"logic->EffectiveHealth()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    trial_skipped(TK_LIGHT_TRIAL)\n",
		"test")),
		"ctx->GetTrial(TK_LIGHT_TRIAL)->IsSkipped()");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    check_price(RC_KF_SHOP_ITEM_1)\n",
		"test")),
		"GetCheckPrice(RC_KF_SHOP_ITEM_1)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    can_plant_bean(RR_KOKIRI_FOREST, RG_KOKIRI_FOREST_BEAN_SOUL)\n",
		"test")),
		"CanPlantBean(RR_KOKIRI_FOREST, RG_KOKIRI_FOREST_BEAN_SOUL)");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    triforce_pieces()\n",
		"test")),
		"logic->GetSaveContext()->ship.quest.data.randomizer.triforcePiecesCollected");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    big_poes()\n",
		"test")),
		"logic->BigPoes");
}

TEST(SohExpressions, CallEnemyFunctions) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"enemy RE_GOLD_SKULLTULA {\n"
		"    kill(wallOrFloor = true):\n"
		"        match distance {\n"
		"            ED_CLOSE: can_use(RG_MEGATON_HAMMER) or\n"
		"            ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or\n"
		"            ED_MASTER_SWORD_JUMPSLASH: can_use(RG_MASTER_SWORD) or\n"
		"            ED_LONG_JUMPSLASH: can_use(RG_BIGGORON_SWORD) or can_use(RG_STICKS) or\n"
		"            ED_BOMB_THROW: can_use(RG_BOMB_BAG) or\n"
		"            ED_BOOMERANG: can_use(RG_BOOMERANG) or can_use(RG_DINS_FIRE) or\n"
		"            ED_HOOKSHOT: can_use(RG_HOOKSHOT) or\n"
		"            ED_LONGSHOT: can_use(RG_LONGSHOT) or\n"
		"                can_use(RG_BOMBCHU_5) or\n"
		"            ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)\n"
		"        }\n"
		"    drop:\n"
		"        match distance {\n"
		"            ED_CLOSE or\n"
		"            ED_SHORT_JUMPSLASH or\n"
		"            ED_MASTER_SWORD_JUMPSLASH or\n"
		"            ED_LONG_JUMPSLASH or\n"
		"            ED_BOMB_THROW or\n"
		"            ED_BOOMERANG: can_use(RG_BOOMERANG) or\n"
		"            ED_HOOKSHOT: can_use(RG_HOOKSHOT) or\n"
		"            ED_LONGSHOT: can_use(RG_LONGSHOT)\n"
		"        }\n"
		"}\n"
		"define test():\n"
		"    can_kill(RE_GOLD_SKULLTULA, ED_CLOSE, wallOrFloor: false) or\n"
		"    can_pass(RE_GOLD_SKULLTULA) or\n"
		"    can_get_drop(RE_GOLD_SKULLTULA) or\n"
		"    can_avoid(RE_GOLD_SKULLTULA)\n",
		"test")),
		"logic->CanKillEnemy(RE_GOLD_SKULLTULA, ED_CLOSE, false) || "
		"logic->CanPassEnemy(RE_GOLD_SKULLTULA) || "
		"logic->CanGetEnemyDrop(RE_GOLD_SKULLTULA) || "
		"logic->CanAvoidEnemy(RE_GOLD_SKULLTULA)");
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
		"    is_child and can_use(RG_BOTTLE_WITH_BUGS) and has(bean)\n"
		"\n"
		"define test():\n"
		"    CanSpawnSoilSkull(RG_KOKIRI_FOREST_BEAN_SOUL)\n",
		"test")),
		"CanSpawnSoilSkull(RG_KOKIRI_FOREST_BEAN_SOUL)");

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
		"CanHitSwitch(ED_BOOMERANG)");
}

TEST(SohExpressions, AnyAgeBlockSimple) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    any_age { true }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"AnyAgeTime([]{return true;})");
}

TEST(SohExpressions, AnyAgeBlockWithCall) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    any_age { can_use(RG_HOOKSHOT) }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"AnyAgeTime([]{return logic->CanUse(RG_HOOKSHOT);})");
}

TEST(SohExpressions, AnyAgeBlockWithCompoundCondition) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    any_age { has(RG_HOOKSHOT) or has(RG_BOOMERANG) }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"AnyAgeTime([]{return logic->HasItem(RG_HOOKSHOT) || logic->HasItem(RG_BOOMERANG);})");
}

TEST(SohExpressions, AnyAgeBlockWithExternalCondition) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    any_age { can_kill(RE_ARMOS) } and can_use(RG_STICKS)\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"AnyAgeTime([]{return logic->CanKillEnemy(RE_ARMOS);}) && logic->CanUse(RG_STICKS)");
}

TEST(SohExpressions, SharedBlockSingleBranch) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    shared {\n"
		"        from RR_ROOM_A: has(RG_HOOKSHOT)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_ROOM_A, []{return logic->HasItem(RG_HOOKSHOT);}, false)");
}

TEST(SohExpressions, SharedBlockTwoBranches) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    shared {\n"
		"        from RR_ROOM_A: has(RG_HOOKSHOT)\n"
		"        from RR_ROOM_B: can_use(RG_FAIRY_BOW)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_ROOM_A, []{return logic->HasItem(RG_HOOKSHOT);}, false, "
		"RR_ROOM_B, []{return logic->CanUse(RG_FAIRY_BOW);})");
}

TEST(SohExpressions, SharedBlockThreeBranches) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    shared {\n"
		"        from RR_ROOM_A: can_use(RG_HOOKSHOT)\n"
		"        from RR_ROOM_B: can_use(RG_FAIRY_BOW)\n"
		"        from RR_ROOM_C: can_use(RG_LONGSHOT)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_ROOM_A, []{return logic->CanUse(RG_HOOKSHOT);}, false, "
		"RR_ROOM_B, []{return logic->CanUse(RG_FAIRY_BOW);}, "
		"RR_ROOM_C, []{return logic->CanUse(RG_LONGSHOT);})");
}

TEST(SohExpressions, SharedBlockAnyAge) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    shared any_age {\n"
		"        from RR_ROOM_A: true\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_ROOM_A, []{return true;}, true)");
}

TEST(SohExpressions, SharedBlockAnyAgeMultipleBranches) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    shared any_age {\n"
		"        from RR_ROOM_A: has(RG_HOOKSHOT)\n"
		"        from RR_ROOM_B: can_use(RG_FAIRY_BOW)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_ROOM_A, []{return logic->HasItem(RG_HOOKSHOT);}, true, "
		"RR_ROOM_B, []{return logic->CanUse(RG_FAIRY_BOW);})");
}

TEST(SohExpressions, SharedBlockComplexConditions) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    shared {\n"
		"        from RR_ROOM_A:\n"
		"            can_use(RG_HOOKSHOT) or can_use(RG_BOOMERANG)\n"
		"        from RR_ROOM_B:\n"
		"            can_get_drop(RE_GOLD_SKULLTULA,\n"
		"                trick(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_ROOM_A, []{return logic->CanUse(RG_HOOKSHOT) || logic->CanUse(RG_BOOMERANG);}, false, "
		"RR_ROOM_B, []{return logic->CanGetEnemyDrop(RE_GOLD_SKULLTULA, ctx->GetTrickOption(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT);})");
}

TEST(SohExpressions, SharedBlockWithExternalCondition) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    has(RG_OPEN_CHEST) and shared {\n"
		"        from RR_ROOM_A: has(RG_HOOKSHOT)\n"
		"        from RR_ROOM_B: can_use(RG_FAIRY_BOW)\n"
		"    }\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->HasItem(RG_OPEN_CHEST) && "
		"SpiritShared(RR_ROOM_A, []{return logic->HasItem(RG_HOOKSHOT);}, false, "
		"RR_ROOM_B, []{return logic->CanUse(RG_FAIRY_BOW);})");
}

TEST(SohExpressions, SharedBlockFromHere) {
	auto expr = sourceToRegionExpression(
		"region RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD {\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_TEST_LOCATION: shared {\n"
		"            from here: has(RG_HOOKSHOT)\n"
		"            from RR_OTHER_ROOM: can_use(RG_FAIRY_BOW)\n"
		"        }\n"
		"    }\n"
		"}\n",
		"RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD",
		rls::ast::SectionKind::Locations,
		"RC_TEST_LOCATION");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD, []{return logic->HasItem(RG_HOOKSHOT);}, false, "
		"RR_OTHER_ROOM, []{return logic->CanUse(RG_FAIRY_BOW);})");
}

TEST(SohExpressions, SharedBlockFromHereOnly) {
	auto expr = sourceToRegionExpression(
		"region RR_SPIRIT_TEMPLE_SUN_BLOCK_CHEST_LEDGE {\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    events {\n"
		"        LOGIC_SPIRIT_SUN_BLOCK_TORCH: shared any_age {\n"
		"            from here: true\n"
		"        }\n"
		"    }\n"
		"}\n",
		"RR_SPIRIT_TEMPLE_SUN_BLOCK_CHEST_LEDGE",
		rls::ast::SectionKind::Events,
		"LOGIC_SPIRIT_SUN_BLOCK_TORCH");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_SPIRIT_TEMPLE_SUN_BLOCK_CHEST_LEDGE, []{return true;}, true)");
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
		"[&]{return distance == ED_CLOSE;}, "
		"[&]{return logic->CanUse(RG_KOKIRI_SWORD);}, false)");
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
		"[&]{return distance == ED_CLOSE;}, "
		"[&]{return logic->CanUse(RG_KOKIRI_SWORD);}, false, "
		"[&]{return distance == ED_FAR;}, "
		"[&]{return logic->CanUse(RG_FAIRY_BOW);}, false)");
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
		"[&]{return distance == ED_CLOSE;}, "
		"[&]{return logic->CanUse(RG_KOKIRI_SWORD);}, true, "
		"[&]{return distance == ED_HOOKSHOT;}, "
		"[&]{return logic->CanUse(RG_HOOKSHOT);}, true, "
		"[&]{return distance == ED_FAR;}, "
		"[&]{return logic->CanUse(RG_FAIRY_BOW);}, false)");
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
		"[&]{return distance == ED_CLOSE || distance == ED_SHORT_JUMPSLASH;}, "
		"[&]{return logic->CanUse(RG_KOKIRI_SWORD);}, false)");
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
		"[&]{return distance == ED_SHORT_JUMPSLASH;}, "
		"[&]{return logic->CanUse(RG_KOKIRI_SWORD);}, true, "
		"[&]{return distance == ED_BOMB_THROW;}, "
		"[&]{return !inWater && logic->CanUse(RG_BOMB_BAG);}, true, "
		"[&]{return distance == ED_FAR;}, "
		"[&]{return logic->CanUse(RG_FAIRY_BOW);}, false)");
}

TEST(SohExpressions, SharedBlockFromHereWithExternalCondition) {
	auto expr = sourceToRegionExpression(
		"region RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD {\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_SPIRIT_TEMPLE_MAP_CHEST: has(RG_OPEN_CHEST) and shared {\n"
		"            from here:\n"
		"                has(RG_HOOKSHOT)\n"
		"                or (trick(RT_SPIRIT_MAP_CHEST) and can_use(RG_FAIRY_BOW))\n"
		"            from RR_SPIRIT_TEMPLE_STATUE_ROOM:\n"
		"                can_use(RG_DINS_FIRE)\n"
		"        }\n"
		"    }\n"
		"}\n",
		"RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD",
		rls::ast::SectionKind::Locations,
		"RC_SPIRIT_TEMPLE_MAP_CHEST");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->HasItem(RG_OPEN_CHEST) && "
		"SpiritShared(RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD, "
		"[]{return logic->HasItem(RG_HOOKSHOT) || ctx->GetTrickOption(RT_SPIRIT_MAP_CHEST) && logic->CanUse(RG_FAIRY_BOW);}, false, "
		"RR_SPIRIT_TEMPLE_STATUE_ROOM, "
		"[]{return logic->CanUse(RG_DINS_FIRE);})");
}

TEST(SohExpressions, SharedBlockFromHereThreeBranches) {
	auto expr = sourceToRegionExpression(
		"region RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD {\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_SPIRIT_TEMPLE_GS_LOBBY: shared {\n"
		"            from here:\n"
		"                can_get_drop(RE_GOLD_SKULLTULA, ED_LONGSHOT)\n"
		"            from RR_SPIRIT_TEMPLE_INNER_WEST_HAND:\n"
		"                can_get_drop(RE_GOLD_SKULLTULA,\n"
		"                    trick(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT)\n"
		"            from RR_SPIRIT_TEMPLE_GS_LEDGE:\n"
		"                can_kill(RE_GOLD_SKULLTULA)\n"
		"        }\n"
		"    }\n"
		"}\n",
		"RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD",
		rls::ast::SectionKind::Locations,
		"RC_SPIRIT_TEMPLE_GS_LOBBY");
	EXPECT_EQ(GenerateExpression(expr),
		"SpiritShared(RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD, "
		"[]{return logic->CanGetEnemyDrop(RE_GOLD_SKULLTULA, ED_LONGSHOT);}, false, "
		"RR_SPIRIT_TEMPLE_INNER_WEST_HAND, "
		"[]{return logic->CanGetEnemyDrop(RE_GOLD_SKULLTULA, ctx->GetTrickOption(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT);}, "
		"RR_SPIRIT_TEMPLE_GS_LEDGE, "
		"[]{return logic->CanKillEnemy(RE_GOLD_SKULLTULA);})");
}
