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
