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

// == Params ===================================================================

TEST(DeclParam, BareName) {
	EXPECT_TRUE(matches<param>("distance"));
}

TEST(DeclParam, WithType) {
	EXPECT_TRUE(matches<param>("distance: int"));
}

TEST(DeclParam, WithDefault) {
	EXPECT_TRUE(matches<param>("distance = ED_CLOSE"));
}

TEST(DeclParam, WithTypeAndDefault) {
	EXPECT_TRUE(matches<param>("distance: int = ED_CLOSE"));
}

TEST(DeclParam, DefaultExprIsCall) {
	EXPECT_TRUE(matches<param>("x = foo()"));
}

TEST(DeclParams, Single) {
	EXPECT_TRUE(matches<params>("x"));
}

TEST(DeclParams, Multiple) {
	EXPECT_TRUE(matches<params>("x, y, z"));
}

TEST(DeclParams, MixedStyles) {
	EXPECT_TRUE(matches<params>("distance: int = ED_CLOSE, wallOrFloor = true, inWater"));
}

TEST(DeclParams, WithWhitespace) {
	EXPECT_TRUE(matches<params>("x , y , z"));
}

// == Entry ====================================================================

TEST(DeclEntry, SimpleExpr) {
	EXPECT_TRUE(matches<entry>("RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()"));
}

TEST(DeclEntry, ComplexExpr) {
	EXPECT_TRUE(matches<entry>("RR_SPIRIT_TEMPLE_CHILD: is_child and has(RG_STICKS)"));
}

TEST(DeclEntry, AlwaysExpr) {
	EXPECT_TRUE(matches<entry>("RR_SPIRIT_TEMPLE_ENTRYWAY: always"));
}

// == Section ==================================================================

TEST(DeclSection, EmptyEvents) {
	EXPECT_TRUE(matches<section>("events {}"));
}

TEST(DeclSection, EmptyLocations) {
	EXPECT_TRUE(matches<section>("locations {}"));
}

TEST(DeclSection, EmptyExits) {
	EXPECT_TRUE(matches<section>("exits {}"));
}

TEST(DeclSection, SingleEntry) {
	EXPECT_TRUE(matches<section>("events { SPIRIT_NUT_ACCESS: can_break_pots() }"));
}

TEST(DeclSection, MultipleEntries) {
	EXPECT_TRUE(matches<section>(
		"locations {\n"
		"  RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"  RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()\n"
		"}"
	));
}

TEST(DeclSection, ExitsWithComplexExprs) {
	EXPECT_TRUE(matches<section>(
		"exits {\n"
		"  RR_SPIRIT_TEMPLE_ENTRYWAY: always\n"
		"  RR_SPIRIT_TEMPLE_CHILD_SIDE_HUB:\n"
		"    is_child and has(RG_STICKS)\n"
		"}"
	));
}

// == Region properties ========================================================

TEST(DeclRegionProps, SceneOnly) {
	EXPECT_TRUE(matches<region_props>("name: \"Test\"\nscene: SCENE_SPIRIT_TEMPLE"));
}

TEST(DeclRegionProps, MissingSceneFails) {
	EXPECT_FALSE(matches<region_props>(""));
	EXPECT_FALSE(matches<region_props>("time_passes"));
	EXPECT_FALSE(matches<region_props>("areas: AREA_A"));
	EXPECT_FALSE(matches<region_props>("scene: SCENE_SPIRIT_TEMPLE"));
}

TEST(DeclRegionProps, TimePasses) {
	EXPECT_TRUE(matches<region_props>("name: \"Test\"\nscene: SCENE_SPIRIT_TEMPLE\n  time_passes"));
}

TEST(DeclRegionProps, NoTimePasses) {
	EXPECT_TRUE(matches<region_props>("name: \"Test\"\nscene: SCENE_SPIRIT_TEMPLE\n  no_time_passes"));
}

TEST(DeclRegionProps, AreasOnly) {
	EXPECT_TRUE(matches<region_props>("name: \"Test\"\nscene: SCENE_SPIRIT_TEMPLE\n  areas: AREA_SPIRIT_TEMPLE"));
}

TEST(DeclRegionProps, AreasMultiple) {
	EXPECT_TRUE(matches<region_props>("name: \"Test\"\nscene: SCENE_SPIRIT_TEMPLE\n  areas: AREA_A, AREA_B, AREA_C"));
}

TEST(DeclRegionProps, SceneAndTimePasses) {
	EXPECT_TRUE(matches<region_props>("name: \"Test\"\nscene: SCENE_SPIRIT_TEMPLE\n  time_passes"));
}

TEST(DeclRegionProps, AllThree) {
	EXPECT_TRUE(matches<region_props>(
		"name: \"Test\"\n"
		"scene: SCENE_SPIRIT_TEMPLE\n"
		"  time_passes\n"
		"  areas: AREA_A, AREA_B"
	));
}

// == Region declaration =======================================================

TEST(DeclRegion, Empty) {
	EXPECT_TRUE(matches<region_decl>("region RR_TEST {\n  name: \"Test\"\n  scene: SCENE_TEST\n}"));
}

TEST(DeclRegion, MissingSceneFails) {
	EXPECT_FALSE(matches<region_decl>("region RR_TEST {}"));
}

TEST(DeclRegion, WithScene) {
	EXPECT_TRUE(matches<region_decl>(
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  name: \"Spirit Temple Foyer\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"}"
	));
}

TEST(DeclRegion, WithSceneAndTimePasses) {
	EXPECT_TRUE(matches<region_decl>(
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  name: \"Spirit Temple Foyer\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  time_passes\n"
		"}"
	));
}

TEST(DeclRegion, WithSections) {
	EXPECT_TRUE(matches<region_decl>(
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  name: \"Spirit Temple Foyer\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()\n"
		"  }\n"
		"  exits {\n"
		"    RR_SPIRIT_TEMPLE_ENTRYWAY: always\n"
		"  }\n"
		"}"
	));
}

TEST(DeclRegion, MultipleSections) {
	EXPECT_TRUE(matches<region_decl>(
		"region RR_SPIRIT_TEMPLE_CHILD_SIDE {\n"
		"  name: \"Spirit Temple Child Side\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  events {\n"
		"    SPIRIT_NUT_ACCESS: can_break_pots()\n"
		"  }\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_CHILD_CHEST: always\n"
		"  }\n"
		"  exits {\n"
		"    RR_SPIRIT_TEMPLE_FOYER: always\n"
		"  }\n"
		"}"
	));
}

TEST(DeclRegion, NoTimePasses) {
	EXPECT_TRUE(matches<region_decl>(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  no_time_passes\n"
		"}"
	));
}

TEST(DeclRegion, WithAreas) {
	EXPECT_TRUE(matches<region_decl>(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_FOO\n"
		"  areas: AREA_A, AREA_B\n"
		"}"
	));
}

// == Extend region ============================================================

TEST(DeclExtend, Empty) {
	EXPECT_TRUE(matches<extend_decl>("extend region RR_TEST {}"));
}

TEST(DeclExtend, WithSection) {
	EXPECT_TRUE(matches<extend_decl>(
		"extend region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"  }\n"
		"}"
	));
}

TEST(DeclExtend, MultipleSections) {
	EXPECT_TRUE(matches<extend_decl>(
		"extend region RR_TEST {\n"
		"  locations {\n"
		"    RC_TEST_POT: can_break_pots()\n"
		"  }\n"
		"  events {\n"
		"    EV_TEST: always\n"
		"  }\n"
		"}"
	));
}

// == Define ===================================================================

TEST(DeclDefine, NoParams) {
	EXPECT_TRUE(matches<define_decl>(
		"define has_explosives():\n"
		"  has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)"
	));
}

TEST(DeclDefine, SingleParam) {
	EXPECT_TRUE(matches<define_decl>(
		"define can_use(item): has(item)"
	));
}

TEST(DeclDefine, MultipleParams) {
	EXPECT_TRUE(matches<define_decl>(
		"define can_kill(target, distance, wallOrFloor): true"
	));
}

TEST(DeclDefine, ParamsWithDefaults) {
	EXPECT_TRUE(matches<define_decl>(
		"define can_hit_switch(distance = ED_CLOSE, inWater = false):\n"
		"  match distance {\n"
		"    ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or\n"
		"    ED_FAR: can_use(RG_FAIRY_BOW)\n"
		"  }"
	));
}

TEST(DeclDefine, ParamsWithTypes) {
	EXPECT_TRUE(matches<define_decl>(
		"define water_level(level: int): true"
	));
}

TEST(DeclDefine, ParamsWithTypeAndDefault) {
	EXPECT_TRUE(matches<define_decl>(
		"define foo(x: int = 0, y: int = 1): x + y"
	));
}

TEST(DeclDefine, ComplexBody) {
	EXPECT_TRUE(matches<define_decl>(
		"define spirit_explosive_key_logic():\n"
		"  keys(SCENE_SPIRIT_TEMPLE, has_explosives() ? 1 : 2)"
	));
}

// == Extern define ============================================================

TEST(DeclExternDefine, NoParams) {
	EXPECT_TRUE(matches<extern_define_decl>(
		"extern define has(item) -> Bool"
	));
}

TEST(DeclExternDefine, WithParams) {
	EXPECT_TRUE(matches<extern_define_decl>(
		"extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater = false) -> Bool"
	));
}

TEST(DeclExternDefine, MissingReturnTypeFails) {
	EXPECT_FALSE(matches<extern_define_decl>(
		"extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater = false)"
	));
}

// == Enemy ====================================================================

TEST(DeclEnemy, SingleField) {
	EXPECT_TRUE(matches<enemy_decl>(
		"enemy RE_ARMOS {\n"
		"  kill: can_use(RG_KOKIRI_SWORD)\n"
		"}"
	));
}

TEST(DeclEnemy, AllFields) {
	EXPECT_TRUE(matches<enemy_decl>(
		"enemy RE_ARMOS {\n"
		"  kill: can_use(RG_KOKIRI_SWORD) or has_explosives()\n"
		"  pass: can_use(RG_HOOKSHOT)\n"
		"  drop: can_use(RG_KOKIRI_SWORD)\n"
		"  avoid: true\n"
		"}"
	));
}

TEST(DeclEnemy, FieldWithParams) {
	EXPECT_TRUE(matches<enemy_decl>(
		"enemy RE_GREEN_BUBBLE {\n"
		"  kill(distance = ED_CLOSE, wallOrFloor = true): can_use(RG_KOKIRI_SWORD)\n"
		"  pass(distance = ED_CLOSE, wallOrFloor = false): can_use(RG_HOOKSHOT)\n"
		"}"
	));
}

TEST(DeclEnemy, FieldWithEmptyParams) {
	EXPECT_TRUE(matches<enemy_decl>(
		"enemy RE_TEST {\n"
		"  kill(): true\n"
		"}"
	));
}

TEST(DeclEnemy, FieldWithTypedParams) {
	EXPECT_TRUE(matches<enemy_decl>(
		"enemy RE_TEST {\n"
		"  kill(distance: int = ED_CLOSE): true\n"
		"}"
	));
}

// == Declaration (top-level choice) ===========================================

TEST(DeclDeclaration, Region) {
	EXPECT_TRUE(matches<declaration>("region RR_TEST {\n  name: \"Test\"\n  scene: SCENE_TEST\n}"));
}

TEST(DeclDeclaration, Extend) {
	EXPECT_TRUE(matches<declaration>("extend region RR_TEST {}"));
}

TEST(DeclDeclaration, Define) {
	EXPECT_TRUE(matches<declaration>("define foo(): true"));
}

TEST(DeclDeclaration, ExternDefine) {
	EXPECT_TRUE(matches<declaration>("extern define can_use(item) -> Bool"));
}

TEST(DeclDeclaration, Enemy) {
	EXPECT_TRUE(matches<declaration>("enemy RE_TEST { kill: true }"));
}

// == File =====================================================================

TEST(DeclFile, Empty) {
	EXPECT_TRUE(matches<rls_file>(""));
}

TEST(DeclFile, WhitespaceOnly) {
	EXPECT_TRUE(matches<rls_file>("  \n  \n  "));
}

TEST(DeclFile, CommentOnly) {
	EXPECT_TRUE(matches<rls_file>("# this is a comment\n"));
}

TEST(DeclFile, SingleRegion) {
	EXPECT_TRUE(matches<rls_file>("region RR_TEST {\n  name: \"Test\"\n  scene: SCENE_TEST\n}"));
}

TEST(DeclFile, SingleDefine) {
	EXPECT_TRUE(matches<rls_file>("define foo(): true"));
}

TEST(DeclFile, SingleExternDefine) {
	EXPECT_TRUE(matches<rls_file>("extern define can_use(item) -> Bool"));
}

TEST(DeclFile, MultipleDeclarations) {
	EXPECT_TRUE(matches<rls_file>(
		"# Spirit Temple\n"
		"\n"
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  name: \"Spirit Temple Foyer\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"  }\n"
		"  exits {\n"
		"    RR_SPIRIT_TEMPLE_ENTRYWAY: always\n"
		"  }\n"
		"}\n"
		"\n"
		"region RR_SPIRIT_TEMPLE_CHILD {\n"
		"  name: \"Spirit Temple Child\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  events {\n"
		"    SPIRIT_NUT_ACCESS: can_break_pots()\n"
		"  }\n"
		"}\n"
	));
}

TEST(DeclFile, MixedDeclarations) {
	EXPECT_TRUE(matches<rls_file>(
		"extern define has(item) -> Bool\n"
		"extern define can_use(item) -> Bool\n"
		"\n"
		"define has_explosives():\n"
		"  has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n"
		"\n"
		"enemy RE_ARMOS {\n"
		"  kill: can_use(RG_KOKIRI_SWORD) or has_explosives()\n"
		"  pass: can_use(RG_HOOKSHOT)\n"
		"}\n"
		"\n"
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_FOO\n"
		"  exits {\n"
		"    RR_OTHER: can_kill(RE_ARMOS)\n"
		"  }\n"
		"}\n"
		"\n"
		"extend region RR_TEST {\n"
		"  locations {\n"
		"    RC_TEST_POT: can_break_pots()\n"
		"  }\n"
		"}\n"
	));
}

TEST(DeclFile, WithCommentsBetweenDecls) {
	EXPECT_TRUE(matches<rls_file>(
		"# helpers\n"
		"define foo(): true\n"
		"\n"
		"# regions\n"
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"}\n"
	));
}

// == Realistic full file ======================================================

TEST(DeclRealistic, SpiritTempleExcerpt) {
	EXPECT_TRUE(matches<rls_file>(
		"# spirit_temple.rls\n"
		"\n"
		"region RR_SPIRIT_TEMPLE_ENTRYWAY {\n"
		"  name: \"Spirit Temple Entryway\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  exits {\n"
		"    RR_DESERT_COLOSSUS: always\n"
		"  }\n"
		"}\n"
		"\n"
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  name: \"Spirit Temple Foyer\"\n"
		"  scene: SCENE_SPIRIT_TEMPLE\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()\n"
		"  }\n"
		"  exits {\n"
		"    RR_SPIRIT_TEMPLE_ENTRYWAY: always\n"
		"    RR_SPIRIT_TEMPLE_CHILD_SIDE_HUB: is_child\n"
		"    RR_SPIRIT_TEMPLE_ADULT_SIDE_HUB:\n"
		"      is_adult and can_use(RG_SILVER_GAUNTLETS)\n"
		"  }\n"
		"}\n"
	));
}

TEST(DeclRealistic, DefineWithMatch) {
	EXPECT_TRUE(matches<rls_file>(
		"define can_hit_switch(distance = ED_CLOSE, inWater = false):\n"
		"  match distance {\n"
		"    ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or can_use(RG_MEGATON_HAMMER) or\n"
		"    ED_CLOSE: has_explosives() or\n"
		"    ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)\n"
		"  }\n"
	));
}

TEST(DeclRealistic, EnemyWithParamFields) {
	EXPECT_TRUE(matches<rls_file>(
		"enemy RE_GREEN_BUBBLE {\n"
		"  kill(distance = ED_CLOSE, wallOrFloor = true):\n"
		"    can_use(RG_KOKIRI_SWORD) or has_explosives()\n"
		"  pass(distance = ED_CLOSE, wallOrFloor = false):\n"
		"    can_use(RG_HOOKSHOT) or can_use(RG_BOOMERANG)\n"
		"  drop: can_use(RG_KOKIRI_SWORD)\n"
		"  avoid: true\n"
		"}\n"
	));
}

TEST(DeclRealistic, ExtendWithPots) {
	EXPECT_TRUE(matches<rls_file>(
		"# spirit_temple_pots.rls\n"
		"\n"
		"extend region RR_SPIRIT_TEMPLE_FOYER {\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"    RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()\n"
		"  }\n"
		"}\n"
		"\n"
		"extend region RR_SPIRIT_TEMPLE_CHILD_SIDE {\n"
		"  locations {\n"
		"    RC_SPIRIT_TEMPLE_CHILD_POT: can_break_pots()\n"
		"  }\n"
		"}\n"
	));
}

TEST(DeclRealistic, SharedInRegionExit) {
	EXPECT_TRUE(matches<rls_file>(
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
	));
}

TEST(DeclRealistic, AnyAgeInRegionExit) {
	EXPECT_TRUE(matches<rls_file>(
		"region RR_TEST {\n"
		"  name: \"Test\"\n"
		"  scene: SCENE_TEST\n"
		"  exits {\n"
		"    RR_TARGET: any_age { has(RG_HOOKSHOT) or has(RG_BOOMERANG) }\n"
		"  }\n"
		"}\n"
	));
}
