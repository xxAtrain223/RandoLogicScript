#include <gtest/gtest.h>

#include "ast.h"
#include "parser.h"
#include "sema.h"

// Internal header for direct unit testing.
#include "collect_declarations.h"
#include "resolve_types.h"
#include "type_helpers.h"

using namespace rls::ast;
using namespace rls::sema;

// == Helpers ==================================================================

/// Count diagnostics of a given level.
static size_t countErrors(const std::vector<Diagnostic>& diags) {
	size_t n = 0;
	for (const auto& d : diags)
		if (d.level == DiagnosticLevel::Error) ++n;
	return n;
}

// == typeFromIdentifier (Step 1) ==============================================

TEST(EnumPrefix, Item)       { EXPECT_EQ(typeFromIdentifier("RG_HOOKSHOT"), Type::Item); }
TEST(EnumPrefix, Enemy)      { EXPECT_EQ(typeFromIdentifier("RE_ARMOS"), Type::Enemy); }
TEST(EnumPrefix, Distance)   { EXPECT_EQ(typeFromIdentifier("ED_CLOSE"), Type::Distance); }
TEST(EnumPrefix, Trick)      { EXPECT_EQ(typeFromIdentifier("RT_SPIRIT_CHILD_CHU"), Type::Trick); }
TEST(EnumPrefix, SettingRSK) { EXPECT_EQ(typeFromIdentifier("RSK_SUNLIGHT_ARROWS"), Type::Setting); }
TEST(EnumPrefix, SettingRO)  { EXPECT_EQ(typeFromIdentifier("RO_CLOSED_FOREST_ON"), Type::Setting); }
TEST(EnumPrefix, Region)     { EXPECT_EQ(typeFromIdentifier("RR_SPIRIT_TEMPLE_FOYER"), Type::Region); }
TEST(EnumPrefix, Check)      { EXPECT_EQ(typeFromIdentifier("RC_SPIRIT_TEMPLE_CHEST"), Type::Check); }
TEST(EnumPrefix, Logic)      { EXPECT_EQ(typeFromIdentifier("LOGIC_SPIRIT_PLATFORM"), Type::Logic); }
TEST(EnumPrefix, Scene)      { EXPECT_EQ(typeFromIdentifier("SCENE_SPIRIT_TEMPLE"), Type::Scene); }
TEST(EnumPrefix, Dungeon)    { EXPECT_EQ(typeFromIdentifier("DUNGEON_SPIRIT"), Type::Dungeon); }
TEST(EnumPrefix, Area)       { EXPECT_EQ(typeFromIdentifier("RA_CASTLE_GROUNDS"), Type::Area); }
TEST(EnumPrefix, Trial)      { EXPECT_EQ(typeFromIdentifier("TK_LIGHT_TRIAL"), Type::Trial); }
TEST(EnumPrefix, WaterLevel) { EXPECT_EQ(typeFromIdentifier("WL_HIGH"), Type::WaterLevel); }

TEST(EnumPrefix, UnknownName) { EXPECT_FALSE(typeFromIdentifier("distance").has_value()); }
TEST(EnumPrefix, EmptyString) { EXPECT_FALSE(typeFromIdentifier("").has_value()); }
TEST(EnumPrefix, PrefixOnly)  { EXPECT_EQ(typeFromIdentifier("RG_"), Type::Item); }

// == resolveTypes (Steps 2-3) =================================================

static std::string withHostExterns(const std::string& source) {
	return
		"extern define has(item: Item) -> Bool\n"
		"extern define can_use(item: Item) -> Bool\n"
		"extern define keys(sc: Scene, amount: Int) -> Bool\n"
		"extern define setting(opt: Setting) -> Setting\n"
		"extern define trick(rule: Trick) -> Bool\n"
		"extern define hearts() -> Int\n"
		"extern define check_price(chk: Check = RC_UNKNOWN_CHECK) -> Int\n"
		+ source;
}

/// Parse RLS source, collect declarations, and resolve types.
static std::pair<Project, std::vector<Diagnostic>> resolveFromSource(
	const std::string& source)
{
	Project project;
	project.files.push_back(rls::parser::ParseString(withHostExterns(source)));
	collectDeclarations(project);
	auto diags = resolveTypes(project);
	return {std::move(project), std::move(diags)};
}

/// Find the first region entry condition by region name.
static const Expr* findRegionEntry(const Project& project,
	const std::string& regionName = "RR_TEST")
{
	auto it = project.RegionDecls.find(regionName);
	if (it == project.RegionDecls.end()) return nullptr;
	auto& sections = it->second->body.sections;
	if (sections.empty() || sections[0].entries.empty()) return nullptr;
	return sections[0].entries[0].condition.get();
}

// -- Leaf types ---------------------------------------------------------------

TEST(ResolveTypes, BoolLiteral) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, IntLiteral) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: 42 }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

TEST(ResolveTypes, KeywordExpr) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: is_child }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, IdentifierEnum) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Item);
}

TEST(ResolveTypes, IdentifierUnknown) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: distance }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown identifier 'distance'"), std::string::npos);
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Error);
}

// -- Unary --------------------------------------------------------------------

TEST(ResolveTypes, UnaryNotBool) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: not true }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, UnaryNotIntImplicitConvert) {
	// not 42 — Int is bool-compatible, so no error.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: not 42 }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, UnaryNotTypeMismatch) {
	// not RG_HOOKSHOT — Item is not bool-compatible.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: not RG_HOOKSHOT }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("'not' requires a Bool operand"), std::string::npos);
}

// -- Binary logical -----------------------------------------------------------

TEST(ResolveTypes, BinaryAndBool) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true and false }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, BinaryOrIntImplicit) {
	// 42 or true — Int on left is bool-compatible.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: 42 or true }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, BinaryAndTypeMismatch) {
	// RG_HOOKSHOT and true — Item is not bool-compatible.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT and true }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("'and' requires Bool operands"), std::string::npos);
}

// -- Comparison ---------------------------------------------------------------

TEST(ResolveTypes, EqualitySameType) {
	// RG_HOOKSHOT == RG_FAIRY_BOW  (both Item)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT == RG_FAIRY_BOW }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, EqualityTypeMismatch) {
	// RG_HOOKSHOT == RE_ARMOS  (Item vs Enemy)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT == RE_ARMOS }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("incompatible types"), std::string::npos);
}

TEST(ResolveTypes, OrderingInts) {
	// 3 >= 1
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: 3 >= 1 }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, OrderingNonInt) {
	// RG_HOOKSHOT > RG_FAIRY_BOW  — Item is not Int.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT > RG_FAIRY_BOW }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 2u); // both sides flagged
}

// -- Arithmetic ---------------------------------------------------------------

TEST(ResolveTypes, ArithmeticInts) {
	// 3 + 1
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: 3 + 1 }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

TEST(ResolveTypes, ArithmeticNonInt) {
	// true + 1
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true + 1 }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("arithmetic requires Int"), std::string::npos);
}

// -- Ternary ------------------------------------------------------------------

TEST(ResolveTypes, TernaryOk) {
	// true ? ED_CLOSE : ED_FAR
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true ? ED_CLOSE : ED_FAR }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Distance);
}

TEST(ResolveTypes, TernaryBranchMismatch) {
	// true ? ED_CLOSE : RG_HOOKSHOT
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true ? ED_CLOSE : RG_HOOKSHOT }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("different types"), std::string::npos);
}

TEST(ResolveTypes, TernaryBoolCompatibleBranchesUnify) {
	// true ? 1 : false  →  Int + Bool both bool-compatible → Bool
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true ? 1 : false }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 0u);
	ASSERT_EQ(diags.size(), 1u);
	EXPECT_EQ(diags[0].level, DiagnosticLevel::Warning);
	EXPECT_NE(diags[0].message.find("implicitly converted to Bool"), std::string::npos);
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, TernaryCondNotBool) {
	// RG_HOOKSHOT ? 1 : 2
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT ? 1 : 2 }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("ternary condition must be Bool"), std::string::npos);
	// But the result type is still Int from the branches.
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

// -- Host function calls ------------------------------------------------------

TEST(ResolveTypes, HostCallHas) {
	// has(RG_HOOKSHOT)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, HostCallSetting) {
	// setting(RSK_SUNLIGHT_ARROWS)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: setting(RSK_SUNLIGHT_ARROWS) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Setting);
}

TEST(ResolveTypes, HostCallReturnsInt) {
	// hearts()
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: hearts() }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

TEST(ResolveTypes, HostCallWrongArgType) {
	// has(RE_ARMOS) — Enemy where Item expected.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(RE_ARMOS) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expected Item, got Enemy"), std::string::npos);
	// Return type is still Bool.
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, HostCallTooFewArgs) {
	// has() — missing required arg.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has() }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1 argument(s), got 0"), std::string::npos);
}

TEST(ResolveTypes, HostCallTooManyArgs) {
	// has(RG_HOOKSHOT, RG_FAIRY_BOW) — too many.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(RG_HOOKSHOT, RG_FAIRY_BOW) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1 argument(s), got 2"), std::string::npos);
}

TEST(ResolveTypes, HostCallOptionalArgOmitted) {
	// check_price() — 0 args, optional Check param.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: check_price() }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

TEST(ResolveTypes, HostCallOptionalArgProvided) {
	// check_price(RC_SPIRIT_CHEST)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: check_price(RC_SPIRIT_CHEST) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

TEST(ResolveTypes, HostCallMultipleArgs) {
	// keys(SCENE_SPIRIT_TEMPLE, 3)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: keys(SCENE_SPIRIT_TEMPLE, 3) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, HostCallNamedArgsReordered) {
	// keys(amount: 3, sc: SCENE_SPIRIT_TEMPLE)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: keys(amount: 3, sc: SCENE_SPIRIT_TEMPLE) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, HostCallUnknownNamedArg) {
	// has(itm: RG_HOOKSHOT) — unknown named arg.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(itm: RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown named argument 'itm'"), std::string::npos);
}

TEST(ResolveTypes, HostCallDuplicateNamedArg) {
	// has(item: RG_HOOKSHOT, item: RG_FAIRY_BOW) — duplicate named arg.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(item: RG_HOOKSHOT, item: RG_FAIRY_BOW) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate argument for parameter 'item'"), std::string::npos);
}

// -- Enemy built-in calls -----------------------------------------------------

TEST(ResolveTypes, EnemyBuiltinOk) {
	// can_kill(RE_ARMOS)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_kill(RE_ARMOS) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, EnemyBuiltinWithOptionalArgs) {
	// can_kill(RE_ARMOS, ED_CLOSE) — optional distance arg.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_kill(RE_ARMOS, ED_CLOSE) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, EnemyBuiltinAllArgs) {
	// can_kill(RE_ARMOS, ED_CLOSE, true, 1, false, false)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_kill(RE_ARMOS, ED_CLOSE, true, 1, false, false) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, EnemyBuiltinNotEnemy) {
	// can_kill(RG_HOOKSHOT) — Item, not Enemy.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_kill(RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("argument 1 expected Enemy"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinNoArgs) {
	// can_kill() — missing Enemy arg.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_kill() }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1-6 argument(s), got 0"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinTooManyArgs) {
	// can_kill with 7 args — one too many.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_kill(RE_ARMOS, false, false, false, false, false, false) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1-6 argument(s), got 7"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinWrongOptionalArgType) {
	// can_kill(RE_ARMOS, RG_HOOKSHOT) — second arg should be Distance.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_kill(RE_ARMOS, RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("argument 2 expected Distance"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinCanPassSignature) {
	// can_pass(RE_ARMOS, ED_CLOSE, true) — all 3 args.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_pass(RE_ARMOS, ED_CLOSE, true) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ResolveTypes, EnemyBuiltinCanAvoidSignature) {
	// can_avoid(RE_ARMOS, false, 2) — all 3 args.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_avoid(RE_ARMOS, false, 2) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ResolveTypes, EnemyBuiltinCanGetDropSignature) {
	// can_get_drop(RE_ARMOS, ED_CLOSE, false) — all 3 args.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_get_drop(RE_ARMOS, ED_CLOSE, false) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

// -- Unknown function ---------------------------------------------------------

TEST(ResolveTypes, UnknownFunction) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: nonexistent_func() }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown function 'nonexistent_func'"), std::string::npos);
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Error);
}

// -- User define calls --------------------------------------------------------

TEST(ResolveTypes, UserDefineCallResolvesReturnType) {
	auto [project, diags] = resolveFromSource(
		"define has_explosives(): true\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has_explosives() }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineCallArgTypeMatch) {
	// define foo(x: Item): has(x)
	// Call: foo(RG_HOOKSHOT) — correct arg type.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ResolveTypes, DefineCallArgTypeMismatch) {
	// define foo(x: Item): has(x)
	// Call: foo(RE_ARMOS) — Enemy where Item expected.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(RE_ARMOS) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("argument 1 expected Item, got Enemy"),
		std::string::npos);
}

TEST(ResolveTypes, DefineCallTooFewArgs) {
	// define foo(x: Item): has(x)
	// Call: foo() — missing required arg.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo() }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1 argument(s), got 0"),
		std::string::npos);
}

TEST(ResolveTypes, DefineCallTooManyArgs) {
	// define foo(x: Item): has(x)
	// Call: foo(RG_HOOKSHOT, RG_FAIRY_BOW) — too many.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(RG_HOOKSHOT, RG_FAIRY_BOW) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1 argument(s), got 2"),
		std::string::npos);
}

TEST(ResolveTypes, DefineCallWithDefaultOmitted) {
	// define foo(x: Item, d = ED_CLOSE): has(x)
	// Call: foo(RG_HOOKSHOT) — optional d omitted.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item, d = ED_CLOSE): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ResolveTypes, DefineCallWithDefaultProvided) {
	// define foo(x: Item, d = ED_CLOSE): has(x)
	// Call: foo(RG_HOOKSHOT, ED_FAR) — optional d provided.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item, d = ED_CLOSE): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(RG_HOOKSHOT, ED_FAR) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ResolveTypes, DefineCallDefaultArgWrongType) {
	// define foo(x: Item, d = ED_CLOSE): has(x)
	// Call: foo(RG_HOOKSHOT, true) — Bool where Distance expected.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item, d = ED_CLOSE): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(RG_HOOKSHOT, true) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("argument 2 expected Distance, got Bool"),
		std::string::npos);
}

TEST(ResolveTypes, DefineCallErrorArgNoCascade) {
	// define foo(x: Item): has(x)
	// Call: foo(unknown_id) — only "unknown identifier" error.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(unknown_id) }\n"
		"}\n");
	// Only the "unknown identifier" error, not a type mismatch.
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown identifier"),
		std::string::npos);
}

TEST(ResolveTypes, DefineCallWithRangeArgCount) {
	// define foo(x: Item, d = ED_CLOSE): has(x)
	// Call: foo() — too few for range (needs 1-2, got 0).
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item, d = ED_CLOSE): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo() }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1-2 argument(s), got 0"),
		std::string::npos);
}

TEST(ResolveTypes, DefineCallNamedArgsReordered) {
	// define foo(x: Item, d = ED_CLOSE): has(x)
	// Call: foo(d: ED_FAR, x: RG_HOOKSHOT) — named + reordered.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item, d = ED_CLOSE): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(d: ED_FAR, x: RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineCallUnknownNamedArg) {
	// define foo(x: Item): has(x)
	// Call: foo(y: RG_HOOKSHOT) — unknown named arg.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(y: RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown named argument 'y'"), std::string::npos);
}

TEST(ResolveTypes, DefineCallDuplicateNamedArg) {
	// define foo(x: Item): has(x)
	// Call: foo(x: RG_HOOKSHOT, x: RG_FAIRY_BOW) — duplicate named arg.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(x: RG_HOOKSHOT, x: RG_FAIRY_BOW) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate argument for parameter 'x'"), std::string::npos);
}

TEST(ResolveTypes, DefineCallMissingRequiredNamedArg) {
	// define foo(x: Item, d = ED_CLOSE): has(x)
	// Call: foo(d: ED_FAR) — missing required x.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item, d = ED_CLOSE): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(d: ED_FAR) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("missing required argument(s): x"), std::string::npos);
}

// -- Define ordering (Step 7) -------------------------------------------------

TEST(ResolveTypes, DefineOrderingCalleeFirst) {
	// Topo sort ensures bar is resolved before foo regardless of decl order.
	auto [project, diags] = resolveFromSource(
		"define foo(): bar()\n"
		"define bar(): true\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo() }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineOrderingTransitive) {
	// Must be processed c → b → a.
	auto [project, diags] = resolveFromSource(
		"define a(): b()\n"
		"define b(): c()\n"
		"define c(): RG_HOOKSHOT\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: a() }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Item);
}

TEST(ResolveTypes, DefineOrderingIndependentDefines) {
	// Both independent — no dependencies, both should resolve.
	auto [project, diags] = resolveFromSource(
		"define foo(): true\n"
		"define bar(): 42\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Bool);
	EXPECT_EQ(project.getType(project.DefineDecls.at("bar")->body.get()), Type::Int);
}

TEST(ResolveTypes, DefineCycleDetected) {
	// Mutual recursion foo ↔ bar → cycle error.
	auto [project, diags] = resolveFromSource(
		"define foo(): bar()\n"
		"define bar(): foo()\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true }\n"
		"}\n");
	EXPECT_GE(countErrors(diags), 1u);
	bool foundCycle = false;
	for (const auto& d : diags) {
		if (d.level == DiagnosticLevel::Error
			&& d.message.find("cycle") != std::string::npos) {
			foundCycle = true;
			// The message should name the involved defines.
			EXPECT_NE(d.message.find("foo"), std::string::npos);
			EXPECT_NE(d.message.find("bar"), std::string::npos);
		}
	}
	EXPECT_TRUE(foundCycle);
}

TEST(ResolveTypes, DefineOrderingDefaultValueDep) {
	// Default value of foo's param calls bar → bar processed first.
	auto [project, diags] = resolveFromSource(
		"define foo(x = bar()): x\n"
		"define bar(): RG_HOOKSHOT\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	auto* foo = project.DefineDecls.at("foo");
	// foo's param x gets type Item from bar()'s return type.
	EXPECT_EQ(project.getType(&foo->params[0]), Type::Item);
	EXPECT_EQ(project.getType(foo->body.get()), Type::Item);
}

// -- Shared block -------------------------------------------------------------

TEST(ResolveTypes, SharedBlockBool) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations {\n"
		"        TEST_LOC: shared {\n"
		"            from here: true\n"
		"            from RR_OTHER: false\n"
		"        }\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, SharedBlockFromHereResolvesRegionName) {
	auto [project, diags] = resolveFromSource(
		"region RR_SPIRIT_TEMPLE_STATUE_ROOM {\n"
		"    name: \"Spirit Temple Statue Room\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_TEST_LOC: shared {\n"
		"            from here: true\n"
		"            from RR_OTHER: false\n"
		"        }\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());

	const auto* expr = findRegionEntry(project, "RR_SPIRIT_TEMPLE_STATUE_ROOM");
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(expr->node));
	const auto& shared = std::get<SharedBlock>(expr->node);
	ASSERT_EQ(shared.branches.size(), 2u);

	// "from here:" should be resolved to the enclosing region name
	ASSERT_TRUE(shared.branches[0].region.has_value());
	EXPECT_EQ(shared.branches[0].region.value(), "RR_SPIRIT_TEMPLE_STATUE_ROOM");

	// "from RR_OTHER:" should remain unchanged
	ASSERT_TRUE(shared.branches[1].region.has_value());
	EXPECT_EQ(shared.branches[1].region.value(), "RR_OTHER");
}

TEST(ResolveTypes, SharedBlockFromHereMultipleBranches) {
	auto [project, diags] = resolveFromSource(
		"region RR_MY_REGION {\n"
		"    name: \"My Region\"\n"
		"    scene: SCENE_TEST\n"
		"    locations {\n"
		"        RC_LOC: shared {\n"
		"            from here: true\n"
		"            from RR_ROOM_A: false\n"
		"            from RR_ROOM_B: true\n"
		"        }\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());

	const auto* expr = findRegionEntry(project, "RR_MY_REGION");
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(expr->node));
	const auto& shared = std::get<SharedBlock>(expr->node);
	ASSERT_EQ(shared.branches.size(), 3u);

	// Only the "from here:" branch should be resolved
	ASSERT_TRUE(shared.branches[0].region.has_value());
	EXPECT_EQ(shared.branches[0].region.value(), "RR_MY_REGION");
	EXPECT_EQ(shared.branches[1].region.value(), "RR_ROOM_A");
	EXPECT_EQ(shared.branches[2].region.value(), "RR_ROOM_B");
}

TEST(ResolveTypes, SharedBlockFromHereInExtendRegion) {
	auto [project, diags] = resolveFromSource(
		"region RR_BASE {\n"
		"    name: \"Base\"\n"
		"    scene: SCENE_TEST\n"
		"}\n"
		"extend region RR_BASE {\n"
		"    locations {\n"
		"        RC_LOC: shared {\n"
		"            from here: true\n"
		"        }\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());

	// Find the entry in the extend region's sections
	auto range = project.ExtendRegionDecls.equal_range("RR_BASE");
	ASSERT_NE(range.first, range.second);
	const auto& sections = range.first->second->sections;
	ASSERT_FALSE(sections.empty());
	ASSERT_FALSE(sections[0].entries.empty());
	const auto* expr = sections[0].entries[0].condition.get();
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(expr->node));
	const auto& shared = std::get<SharedBlock>(expr->node);
	ASSERT_EQ(shared.branches.size(), 1u);

	// "from here:" in an extend region should resolve to that region's name
	ASSERT_TRUE(shared.branches[0].region.has_value());
	EXPECT_EQ(shared.branches[0].region.value(), "RR_BASE");
}

TEST(ResolveTypes, SharedBlockFromHereAnyAge) {
	auto [project, diags] = resolveFromSource(
		"region RR_SUN_BLOCK {\n"
		"    name: \"Sun Block\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    events {\n"
		"        LOGIC_SUN_TORCH: shared any_age {\n"
		"            from here: true\n"
		"        }\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());

	const auto* expr = findRegionEntry(project, "RR_SUN_BLOCK");
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(expr->node));
	const auto& shared = std::get<SharedBlock>(expr->node);
	EXPECT_TRUE(shared.anyAge);
	ASSERT_EQ(shared.branches.size(), 1u);
	ASSERT_TRUE(shared.branches[0].region.has_value());
	EXPECT_EQ(shared.branches[0].region.value(), "RR_SUN_BLOCK");
}

TEST(ResolveTypes, SharedBlockFromHereInExit) {
	auto [project, diags] = resolveFromSource(
		"region RR_SPIRIT_TEMPLE_CHILD {\n"
		"    name: \"Spirit Temple Child\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_DESERT_COLOSSUS: shared {\n"
		"            from here: can_use(RG_HOOKSHOT)\n"
		"            from RR_SPIRIT_TEMPLE_ADULT: true\n"
		"        }\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());

	const auto* expr = findRegionEntry(project, "RR_SPIRIT_TEMPLE_CHILD");
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(expr->node));
	const auto& shared = std::get<SharedBlock>(expr->node);
	ASSERT_EQ(shared.branches.size(), 2u);

	ASSERT_TRUE(shared.branches[0].region.has_value());
	EXPECT_EQ(shared.branches[0].region.value(), "RR_SPIRIT_TEMPLE_CHILD");
	EXPECT_EQ(shared.branches[1].region.value(), "RR_SPIRIT_TEMPLE_ADULT");
}

// -- Any-age block ------------------------------------------------------------

TEST(ResolveTypes, AnyAgeBlockBool) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: any_age { true } }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, AnyAgeBlockNonBool) {
	// any_age { RG_HOOKSHOT } — body is Item, not Bool.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: any_age { RG_HOOKSHOT } }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("any_age body must be Bool"), std::string::npos);
}

// -- Match expression ---------------------------------------------------------

TEST(ResolveTypes, MatchExprBasic) {
	// match distance { ED_CLOSE: true, ED_FAR: false }
	auto [project, diags] = resolveFromSource(
		"define test_fn(distance):\n"
		"    match distance {\n"
		"        ED_CLOSE: true\n"
		"        ED_FAR: false\n"
		"    }\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("test_fn")->body.get()), Type::Bool);
}

TEST(ResolveTypes, MatchPatternTypeMismatch) {
	// match x { ED_CLOSE: true, RG_HOOKSHOT: false } — Distance vs Item.
	auto [project, diags] = resolveFromSource(
		"define test_fn(x):\n"
		"    match x {\n"
		"        ED_CLOSE: true\n"
		"        RG_HOOKSHOT: false\n"
		"    }\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("match pattern 'RG_HOOKSHOT' is Item but expected Distance"),
		std::string::npos);
}

TEST(ResolveTypes, MatchMultiPatternArm) {
	// match x { ED_CLOSE or ED_SHORT_JUMPSLASH: true }
	auto [project, diags] = resolveFromSource(
		"define test_fn(x):\n"
		"    match x {\n"
		"        ED_CLOSE or ED_SHORT_JUMPSLASH: true\n"
		"    }\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("test_fn")->body.get()), Type::Bool);
}

TEST(ResolveTypes, MatchBoolCompatibleArmsUnifyWithWarning) {
	// match x { RSK_A: 1 | RSK_B: true } → Bool + Int → unify to Bool with warning
	auto [project, diags] = resolveFromSource(
		"define test_fn(x):\n"
		"    match x {\n"
		"        RSK_A: 1\n"
		"        RSK_B: true\n"
		"    }\n");
	EXPECT_EQ(countErrors(diags), 0u);
	ASSERT_EQ(diags.size(), 1u);
	EXPECT_EQ(diags[0].level, DiagnosticLevel::Warning);
	EXPECT_NE(diags[0].message.find("implicitly converted to Bool"),
		std::string::npos);
	EXPECT_EQ(project.getType(project.DefineDecls.at("test_fn")->body.get()), Type::Bool);
}

TEST(ResolveTypes, MatchArmsSameNonBoolType) {
	// match x { ED_CLOSE: ED_FAR, ED_FAR: ED_CLOSE } — all Distance.
	auto [project, diags] = resolveFromSource(
		"define test_fn(x):\n"
		"    match x {\n"
		"        ED_CLOSE: ED_FAR\n"
		"        ED_FAR: ED_CLOSE\n"
		"    }\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("test_fn")->body.get()), Type::Distance);
}

TEST(ResolveTypes, MatchArmBodyTypeMismatch) {
	// match x { ED_CLOSE: RG_HOOKSHOT, ED_FAR: true } — Item vs Bool.
	auto [project, diags] = resolveFromSource(
		"define test_fn(x):\n"
		"    match x {\n"
		"        ED_CLOSE: RG_HOOKSHOT\n"
		"        ED_FAR: true\n"
		"    }\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("doesn't match previous arms"),
		std::string::npos);
}

TEST(ResolveTypes, MatchDiscriminantTyped) {
	// Discriminant 'd' is Distance; patterns are Distance — OK.
	auto [project, diags] = resolveFromSource(
		"define foo(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: true\n"
		"        ED_FAR: false\n"
		"    }\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Bool);
}

TEST(ResolveTypes, MatchDiscriminantTypeMismatch) {
	// Discriminant 'd' is Item but pattern ED_CLOSE is Distance — error.
	auto [project, diags] = resolveFromSource(
		"define foo(d: Item):\n"
		"    match d {\n"
		"        ED_CLOSE: true\n"
		"    }\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find(
		"match discriminant 'd' is Item but patterns are Distance"),
		std::string::npos);
}

TEST(ResolveTypes, MatchDiscriminantInferred) {
	// Discriminant 'd' has no type — infer Distance from patterns.
	auto [project, diags] = resolveFromSource(
		"define foo(d):\n"
		"    match d {\n"
		"        ED_CLOSE: true\n"
		"        ED_FAR: false\n"
		"    }\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Bool);
}

// -- Error poisoning ----------------------------------------------------------

TEST(ResolveTypes, ErrorPoisonSuppressesCascade) {
	// unknown_id and true — the 'and' should not report an extra type error
	// for the left side being Error; only the "unknown identifier" diagnostic.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: unknown_id and true }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown identifier 'unknown_id'"), std::string::npos);
	// Result is still Bool — Error doesn't propagate upward from 'and'.
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, ErrorArgDoesNotCascadeInCall) {
	// has(unknown_id) — the unknown identifier is diagnosed once,
	// but the arg type check (Item vs Error) is suppressed.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(unknown_id) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u); // Only the "unknown identifier" error.
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

// -- Sub-expression types populated -------------------------------------------

TEST(ResolveTypes, SubExprTypesPopulated) {
	// has(RG_HOOKSHOT) and can_use(RG_FAIRY_BOW)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(RG_HOOKSHOT) and can_use(RG_FAIRY_BOW) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());

	// Root is Bool.
	auto* root = findRegionEntry(project);
	EXPECT_EQ(project.getType(root), Type::Bool);

	// Left call is Bool.
	auto& bin = std::get<BinaryExpr>(root->node);
	EXPECT_EQ(project.getType(bin.left.get()), Type::Bool);
	EXPECT_EQ(project.getType(bin.right.get()), Type::Bool);

	// Arguments are Item.
	auto& hasCall = std::get<CallExpr>(bin.left->node);
	EXPECT_EQ(project.getType(hasCall.args[0].value.get()), Type::Item);
}

// -- Setting comparison -------------------------------------------------------

TEST(ResolveTypes, SettingIsComparison) {
	// setting(RSK_FOREST) is RO_CLOSED_FOREST_ON
	// → setting() returns Setting, RO_ is Setting, == both Setting → Bool.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: setting(RSK_FOREST) is RO_CLOSED_FOREST_ON }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, SettingBoolTruthiness) {
	// setting(RSK_SUNLIGHT_ARROWS) and true — Setting is bool-compatible.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: setting(RSK_SUNLIGHT_ARROWS) and true }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

// -- Composite expression -----------------------------------------------------

TEST(ResolveTypes, CompositeExpression) {
	// has(RG_HOOKSHOT) or (hearts() >= 3 and trick(RT_SPIRIT_CHILD_CHU))
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: has(RG_HOOKSHOT) or (hearts() >= 3 and trick(RT_SPIRIT_CHILD_CHU)) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

// -- typeFromAnnotation (Step 4) ----------------------------------------------

TEST(TypeAnnotation, AllTypes) {
	EXPECT_EQ(typeFromAnnotation("Bool"),       Type::Bool);
	EXPECT_EQ(typeFromAnnotation("Int"),        Type::Int);
	EXPECT_EQ(typeFromAnnotation("Item"),       Type::Item);
	EXPECT_EQ(typeFromAnnotation("Enemy"),      Type::Enemy);
	EXPECT_EQ(typeFromAnnotation("Distance"),   Type::Distance);
	EXPECT_EQ(typeFromAnnotation("Trick"),      Type::Trick);
	EXPECT_EQ(typeFromAnnotation("Setting"),    Type::Setting);
	EXPECT_EQ(typeFromAnnotation("Region"),     Type::Region);
	EXPECT_EQ(typeFromAnnotation("Check"),      Type::Check);
	EXPECT_EQ(typeFromAnnotation("Logic"),      Type::Logic);
	EXPECT_EQ(typeFromAnnotation("Scene"),      Type::Scene);
	EXPECT_EQ(typeFromAnnotation("Dungeon"),    Type::Dungeon);
	EXPECT_EQ(typeFromAnnotation("Area"),       Type::Area);
	EXPECT_EQ(typeFromAnnotation("Trial"),      Type::Trial);
	EXPECT_EQ(typeFromAnnotation("WaterLevel"), Type::WaterLevel);
}

TEST(TypeAnnotation, Unknown) {
	EXPECT_FALSE(typeFromAnnotation("Foo").has_value());
	EXPECT_FALSE(typeFromAnnotation("").has_value());
	EXPECT_FALSE(typeFromAnnotation("bool").has_value()); // case-sensitive
}

// -- Parameter scope (Step 4) -------------------------------------------------

TEST(ResolveTypes, DefineParamWithAnnotation) {
	// define foo(d: Distance): d
	auto [project, diags] = resolveFromSource(
		"define foo(d: Distance): d\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Distance);
}

TEST(ResolveTypes, DefineParamWithDefault) {
	// define foo(d = ED_CLOSE): d
	auto [project, diags] = resolveFromSource(
		"define foo(d = ED_CLOSE): d\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Distance);
}

TEST(ResolveTypes, DefineParamUntyped) {
	// define foo(x): x — type unknown, no error for the identifier itself
	auto [project, diags] = resolveFromSource(
		"define foo(x): x\n");
	// No "unknown identifier" error — x is a known parameter.
	EXPECT_EQ(countErrors(diags), 0u);
	// But the body type is Error since x's type is unknown.
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Error);
}

TEST(ResolveTypes, DefineParamBadAnnotation) {
	// define foo(x: Foo): x — unknown type annotation
	auto [project, diags] = resolveFromSource(
		"define foo(x: Foo): x\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown type annotation 'Foo'"),
		std::string::npos);
}

TEST(ResolveTypes, DefineParamUsedInExpression) {
	// define foo(d: Distance): d == ED_CLOSE
	auto [project, diags] = resolveFromSource(
		"define foo(d: Distance): d == ED_CLOSE\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Bool);
}

TEST(ResolveTypes, EnemyFieldKillParamScope) {
	// can_kill signature: Enemy, Distance?, Bool?, Int?, Bool?, Bool?
	// Field params (skip Enemy): distance → Distance, wallOrFloor → Bool
	auto [project, diags] = resolveFromSource(
		"enemy RE_TEST {\n"
		"    kill(distance, wallOrFloor): wallOrFloor\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	auto* enemy = project.EnemyDecls.at("RE_TEST");
	EXPECT_EQ(project.getType(enemy->fields[0].body.get()), Type::Bool);
}

TEST(ResolveTypes, EnemyFieldAvoidParamScope) {
	// can_avoid signature: Enemy, Bool?, Int?
	// Field params (skip Enemy): grounded → Bool, quantity → Int
	auto [project, diags] = resolveFromSource(
		"enemy RE_TEST {\n"
		"    avoid(grounded, quantity): quantity\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	auto* enemy = project.EnemyDecls.at("RE_TEST");
	EXPECT_EQ(project.getType(enemy->fields[0].body.get()), Type::Int);
}

TEST(ResolveTypes, EnemyFieldBodyNotBoolCompatible) {
	// Body is Distance — not bool-compatible.
	auto [project, diags] = resolveFromSource(
		"enemy RE_TEST {\n"
		"    kill(distance): distance\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("enemy field"), std::string::npos);
	EXPECT_NE(diags[0].message.find("must be Bool"), std::string::npos);
}

TEST(ResolveTypes, EnemyFieldDropParamScope) {
	// can_get_drop signature: Enemy, Distance?, Bool?
	// Field params (skip Enemy): distance → Distance
	auto [project, diags] = resolveFromSource(
		"enemy RE_TEST {\n"
		"    drop(distance): distance is ED_CLOSE\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	auto* enemy = project.EnemyDecls.at("RE_TEST");
	EXPECT_EQ(project.getType(enemy->fields[0].body.get()), Type::Bool);
}
