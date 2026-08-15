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

// == resolveTypes (Steps 2-3) =================================================

static std::string withHostExterns(const std::string& source) {
	return
		"extern enum Item { RG_* }\n"
		"extern enum Enemy { RE_* }\n"
		"extern enum Distance { ED_* }\n"
		"extern enum Trick { RT_* }\n"
		"extern enum Event { LOGIC_* }\n"
		"extern enum Scene { SCENE_* }\n"
		"extern enum Dungeon { DUNGEON_* }\n"
		"extern enum Area { RA_* }\n"
		"extern enum Trial { TK_* }\n"
		"extern enum Setting { RSK_*, RO_* }\n"
		"extern enum Region { RR_* }\n"
		"extern enum Location { RC_* }\n"
		"extern define has(item: Item) -> Bool\n"
		"extern define can_use(item: Item) -> Bool\n"
		"extern define keys(sc: Scene, amount: Int) -> Bool\n"
		"extern define setting(opt: Setting) -> Int\n"
		"extern define trick(rule: Trick) -> Bool\n"
		"extern define any_age(condition: Condition) -> Bool\n"
		"extern define spirit_shared(first_region: Region, first_condition: Condition, any_age: Bool = false, second_region: Region = RR_NONE, second_condition: Condition = false, third_region: Region = RR_NONE, third_condition: Condition = false) -> Bool\n"
		"extern define hearts() -> Int\n"
		"extern define check_price(chk: Location = RC_UNKNOWN_CHECK) -> Int\n"
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

/// Same as resolveFromSource(), but returns collect + resolve diagnostics.
static std::pair<Project, std::vector<Diagnostic>> resolveFromSourceWithCollectDiags(
	const std::string& source)
{
	Project project;
	project.files.push_back(rls::parser::ParseString(withHostExterns(source)));
	auto diags = collectDeclarations(project);
	auto resolveDiags = resolveTypes(project);
	diags.insert(diags.end(), resolveDiags.begin(), resolveDiags.end());
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

static const Expr* findRegionData(
	const Project& project, std::string_view key,
	const std::string& regionName = "RR_TEST")
{
	auto it = project.RegionDecls.find(regionName);
	if (it == project.RegionDecls.end()) return nullptr;
	const auto* data = it->second->body.findData(key);
	return data ? data->value.get() : nullptr;
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

TEST(ResolveTypes, RegionDataStringAndList) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    areas: [RA_FOREST, RA_FIELD]\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	const auto* name = findRegionData(project, "name");
	ASSERT_NE(name, nullptr);
	EXPECT_EQ(project.getType(name), Type::String);
	const auto* areas = findRegionData(project, "areas");
	ASSERT_NE(areas, nullptr);
	EXPECT_EQ(project.getType(areas), Type::List);
	const auto& list = std::get<ListExpr>(areas->node);
	ASSERT_EQ(list.elements.size(), 2u);
	EXPECT_EQ(project.getType(list.elements[0].get()), Type::Enum);
	EXPECT_EQ(project.getType(list.elements[1].get()), Type::Enum);
}

TEST(ResolveTypes, IdentifierEnum) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<Identifier>(expr->node));
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Item");
	EXPECT_EQ(std::get<Identifier>(expr->node).kind, IdentifierKind::EnumValue);
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

// -- Two-Stage Lookup (Phase 4 Stage A + B) ==================================

TEST(ResolveTypes, EnumIdentifierStageAUserEnum) {
	// Stage A: User-defined enum member lookup
	auto [project, diags] = resolveFromSource(
		"enum Color { RED, GREEN, BLUE }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RED }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Color");
}

TEST(ResolveTypes, EnumIdentifierStageAExternEnumPattern) {
	// Stage A: Extern enum glob pattern member lookup
	auto [project, diags] = resolveFromSource(
		"extern enum Status { ST_ACTIVE, ST_* }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: ST_PENDING }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Status");
}

TEST(ResolveTypes, EnumIdentifierStageBAmbiguityError) {
	// Multiple enums claim the same identifier -> error
	auto [project, diags] = resolveFromSource(
		"enum Alpha { SHARED_VALUE }\n"
		"enum Beta { SHARED_VALUE }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: SHARED_VALUE }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("ambiguous identifier 'SHARED_VALUE'"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Alpha"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Beta"), std::string::npos);
	EXPECT_NE(diags[0].message.find("EnumName."), std::string::npos);
}

TEST(ResolveTypes, EnumIdentifierStageBAmbiguityErrorWithPatternMatch) {
	// Ambiguity also applies when one side is matched through an extern enum pattern.
	auto [project, diags] = resolveFromSource(
		"enum Alpha { SHARED_VALUE }\n"
		"extern enum Beta { SHARED_* }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: SHARED_VALUE }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("ambiguous identifier 'SHARED_VALUE'"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Alpha"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Beta"), std::string::npos);
	EXPECT_NE(diags[0].message.find("EnumName.SHARED_VALUE"), std::string::npos);
}

TEST(ResolveTypes, EnumIdentifierStageBFallbackPrefix) {
	// Stage B: Fallback to prefix map when not in any enum
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_HOOKSHOT }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	// Builtin prefix should still resolve correctly
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Item");
}

TEST(ResolveTypes, EnumIdentifierPreferStageAOverStageB) {
	// If identifier matches both an enum member and a prefix, enum takes precedence
	auto [project, diags] = resolveFromSource(
		"enum Item { RG_CUSTOM }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RG_CUSTOM }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Item");  // Should resolve to user enum, not builtin
}

// -- MemberExpr Resolution (Phase 4 Task E) ==================================

TEST(ResolveTypes, MemberExprResolvesUserEnumMember) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED, GREEN }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: Color.RED }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Color");
}

TEST(ResolveTypes, MemberExprDisambiguatesAmbiguousBareIdentifier) {
	// Bare SHARED_VALUE would be ambiguous; explicit EnumName.ValueName must succeed.
	auto [project, diags] = resolveFromSource(
		"enum Alpha { SHARED_VALUE }\n"
		"enum Beta { SHARED_VALUE }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: Alpha.SHARED_VALUE }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Alpha");
}

TEST(ResolveTypes, MemberExprResolvesExternPatternMember) {
	auto [project, diags] = resolveFromSource(
		"extern enum Status { ST_* }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: Status.ST_PENDING }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Status");
}

TEST(ResolveTypes, MemberExprUnknownEnumError) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: Unknown.RG_HOOKSHOT }\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown enum 'Unknown' in member access"), std::string::npos);
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Error);
}

TEST(ResolveTypes, MemberExprUnknownMemberError) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED }\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: Color.BLUE }\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("'BLUE' is not a member of enum 'Color'"), std::string::npos);
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Error);
}

TEST(ResolveTypes, MemberExprBuiltinEnumMember) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: Item.RG_HOOKSHOT }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	auto enumType = project.getEnumType(expr);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Item");
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
	EXPECT_NE(diags[0].message.find("comparison between enum 'Item' and enum 'Enemy'"),
		std::string::npos);
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
	// true > 1  — Bool is not Int-compatible.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true > 1 }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("comparison requires Int operands"), std::string::npos);
}

TEST(ResolveTypes, OrderingEnumToIntImplicit_DistanceThresholdStyle) {
	// can_get_drop-style threshold compare: distance <= ED_MASTER_SWORD_JUMPSLASH
	auto [project, diags] = resolveFromSource(
		"define can_get_drop(distance = ED_CLOSE):\n"
		"    distance <= ED_MASTER_SWORD_JUMPSLASH\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: can_get_drop(ED_BOOMERANG) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
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

TEST(ResolveTypes, ArithmeticEnumToIntImplicit_DistanceDeltaStyle) {
	// Distance arithmetic still type-checks through enum -> int compatibility.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: ED_HOOKSHOT + 1 }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

TEST(ResolveTypes, EqualityEnumToIntImplicit_DistanceRankStyle) {
	// distance_to_int-style equality path: enum-like and Int are comparable.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: ED_MASTER_SWORD_JUMPSLASH == 2 }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
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
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Enum);
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
	EXPECT_NE(diags[0].message.find("different enum types: 'Distance' and 'Item'"),
		std::string::npos);
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
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Int);
}

TEST(ResolveTypes, DeclaredSettingEnumValueUsesEnumType) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: RSK_SUNLIGHT_ARROWS }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	const auto* expr = findRegionEntry(project);
	EXPECT_EQ(project.getType(expr), Type::Enum);
	EXPECT_EQ(project.getEnumType(expr), "Setting");
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
	EXPECT_NE(diags[0].message.find("expected enum 'Item', got enum 'Enemy'"), std::string::npos);
	// Return type is still Bool.
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, HostCallIntParamAcceptsEnumImplicit_DistanceArgStyle) {
	// Call binding case mirroring distance rank usage: Int param accepts Distance enum value.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: keys(SCENE_TEST, ED_BOOMERANG) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineCallEnumParamIdentityMatchOk) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED }\n"
		"define takes_color(c: Enum = Color.RED):\n"
		"    true\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: takes_color(Color.RED) }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, UntypedForwardingParamInheritsEnumIdentity) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED }\n"
		"define takes_color(c: Color):\n"
		"    true\n"
		"define forwards_color(value):\n"
		"    takes_color(value)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: forwards_color(Color.RED) }\n"
		"}\n");

	ASSERT_TRUE(diags.empty()) << diags.front().message;
	const auto* decl = project.DefineDecls.at("forwards_color");
	EXPECT_EQ(project.getType(&decl->params[0]), Type::Enum);
	EXPECT_EQ(project.getEnumType(&decl->params[0]), "Color");
}

TEST(ResolveTypes, DefineCallEnumParamIdentityMismatchError) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED }\n"
		"enum Fruit { RED }\n"
		"define takes_color(c: Enum = Color.RED):\n"
		"    true\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: takes_color(Fruit.RED) }\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expected enum 'Color', got enum 'Fruit'"), std::string::npos);
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, ExternCallEnumParamIdentityMismatchError) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED }\n"
		"enum Fruit { RED }\n"
		"extern define accepts_color(c: Enum = Color.RED) -> Bool\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: accepts_color(Fruit.RED) }\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expected enum 'Color', got enum 'Fruit'"), std::string::npos);
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineCallIntToEnumWithExplicitContextOk) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED = 0, GREEN = 1 }\n"
		"define takes_color(c: Enum = Color.RED):\n"
		"    true\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: takes_color(1) }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, ExternCallIntToEnumWithExplicitContextOk) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED = 0, GREEN = 1 }\n"
		"extern define accepts_color(c: Enum = Color.RED) -> Bool\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: accepts_color(1) }\n"
		"}\n");

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineCallIntToEnumAmbiguousWithoutContextError) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED = 0 }\n"
		"enum Fruit { APPLE = 0 }\n"
		"define takes_any_enum(c: Enum):\n"
		"    true\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: takes_any_enum(0) }\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("ambiguous integer value 0"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Color"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Fruit"), std::string::npos);
	EXPECT_NE(diags[0].message.find("EnumName.ValueName"), std::string::npos);
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
	// check_price() — 0 args, optional Location param.
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

TEST(ResolveTypes, HostCallMixedArgs) {
	// keys(SCENE_SPIRIT_TEMPLE, amount: 3)
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: keys(SCENE_SPIRIT_TEMPLE, amount: 3) }\n"
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

TEST(ResolveTypes, HostCallMissingRequiredNamedArg) {
	// keys(amount: 3) — missing required sc.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: keys(amount: 3) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("missing required argument(s): sc"), std::string::npos);
}

TEST(ResolveTypes, HostCallConditionArgAcceptsBoolExpression) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: any_age(has(RG_HOOKSHOT) or can_use(RG_BOOMERANG)) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
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

TEST(ResolveTypes, DefineExternNameCollisionError) {
	auto [project, diags] = resolveFromSourceWithCollectDiags(
		"define can_hit_switch(): true\n"
		"extern define can_hit_switch(distance: Distance = ED_CLOSE) -> Bool\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: true }\n"
		"}\n");

	bool found = false;
	for (const auto& d : diags) {
		if (d.level == DiagnosticLevel::Error
			&& d.message.find("duplicate function 'can_hit_switch'") != std::string::npos) {
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
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

TEST(ResolveTypes, DefineCanEvaluateConditionParameter) {
	auto [project, diags] = resolveFromSource(
		"define gate(cond: Condition): cond() and has(RG_HOOKSHOT)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: gate(has(RG_BOOMERANG)) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineConditionRequiresInvocationParentheses) {
	auto [project, diags] = resolveFromSource(
		"define gate(cond: Condition): cond and has(RG_HOOKSHOT)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: gate(has(RG_BOOMERANG)) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("'and' requires Bool operands, left is Condition"), std::string::npos);
}

TEST(ResolveTypes, DefineCanReturnCallableAndInvokeResult) {
	auto [project, diags] = resolveFromSource(
		"define make_cond(cond: Condition): cond\n"
		"define run(cond: Condition): make_cond(cond)()\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: run(has(RG_BOOMERANG)) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineConditionParameterAcceptsCompoundExpression) {
	auto [project, diags] = resolveFromSource(
		"define gate(cond: Condition): cond() and has(RG_HOOKSHOT)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: gate(has(RG_BOOMERANG) or can_use(RG_HOOKSHOT)) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineConditionParametersMultipleArguments) {
	auto [project, diags] = resolveFromSource(
		"define both(left: Condition, right: Condition): left() and right()\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: both(has(RG_HOOKSHOT), can_use(RG_BOOMERANG)) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineConditionDefaultUsedWhenOmitted) {
	auto [project, diags] = resolveFromSource(
		"define gate(cond: Condition = has(RG_HOOKSHOT)): cond()\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: gate() }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineConditionDefaultCanBeOverridden) {
	auto [project, diags] = resolveFromSource(
		"define gate(cond: Condition = has(RG_HOOKSHOT)): cond()\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: gate(can_use(RG_BOOMERANG)) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, DefineCanReturnFunctionReferenceAsCondition) {
	auto [project, diags] = resolveFromSource(
		"define always_true(): true\n"
		"define return_cond(): always_true\n"
		"define test(): return_cond()\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: test()() }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("return_cond")->body.get()), Type::Condition);
	ASSERT_TRUE(std::holds_alternative<Identifier>(project.DefineDecls.at("return_cond")->body->node));
	EXPECT_EQ(std::get<Identifier>(project.DefineDecls.at("return_cond")->body->node).kind, IdentifierKind::FunctionRef);
	EXPECT_EQ(project.getType(project.DefineDecls.at("test")->body.get()), Type::Condition);
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
	EXPECT_NE(diags[0].message.find("argument 1 expected enum 'Item', got enum 'Enemy'"),
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
	EXPECT_NE(diags[0].message.find("argument 2 expected enum 'Distance', got Bool"),
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

TEST(ResolveTypes, DefineCallResolvedArgs) {
	// define foo(x: Item, d = ED_CLOSE): has(x)
	// Call: foo(d: ED_FAR, x: RG_HOOKSHOT) — named args should canonicalize.
	auto [project, diags] = resolveFromSource(
		"define foo(x: Item, d = ED_CLOSE): has(x)\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: foo(d: ED_FAR, x: RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());

	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<CallExpr>(expr->node));
	const auto& call = std::get<CallExpr>(expr->node);
	const auto* resolved = project.getResolvedCallArgs(&call);
	ASSERT_NE(resolved, nullptr);
	ASSERT_EQ(resolved->size(), 2u);
	ASSERT_TRUE(std::holds_alternative<Identifier>((*resolved)[0]->node));
	EXPECT_EQ(std::get<Identifier>((*resolved)[0]->node).name, "RG_HOOKSHOT");
	ASSERT_TRUE(std::holds_alternative<Identifier>((*resolved)[1]->node));
	EXPECT_EQ(std::get<Identifier>((*resolved)[1]->node).name, "ED_FAR");
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
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Enum);
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
	// foo's param x gets the Item enum type from bar()'s return type.
	EXPECT_EQ(project.getType(&foo->params[0]), Type::Enum);
	EXPECT_EQ(project.getType(foo->body.get()), Type::Enum);
}

// -- Any-age host function ----------------------------------------------------

TEST(ResolveTypes, AnyAgeCallBool) {
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: any_age(true) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);
}

TEST(ResolveTypes, AnyAgeCallNonBoolArg) {
	// any_age(RG_HOOKSHOT) — an enum value is not Condition-compatible.
	auto [project, diags] = resolveFromSource(
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    locations { TEST_LOC: any_age(RG_HOOKSHOT) }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("argument 1 expected Condition, got Enum"), std::string::npos);
}

// -- here keyword -------------------------------------------------------------

TEST(ResolveTypes, HereResolvesToCurrentRegion) {
	auto [project, diags] = resolveFromSource(
		"extern define uses_region(r: Region) -> Bool\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    exits { RR_OTHER: uses_region(here) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(findRegionEntry(project)), Type::Bool);

	const auto* expr = findRegionEntry(project);
	ASSERT_NE(expr, nullptr);
	ASSERT_TRUE(std::holds_alternative<CallExpr>(expr->node));
	const auto* resolved = project.getResolvedCallArgs(&std::get<CallExpr>(expr->node));
	ASSERT_NE(resolved, nullptr);
	ASSERT_EQ(resolved->size(), 1u);
	ASSERT_TRUE(std::holds_alternative<HereRef>((*resolved)[0]->node));
	EXPECT_EQ(std::get<HereRef>((*resolved)[0]->node).resolvedRegion, "RR_TEST");
	EXPECT_EQ(project.getType((*resolved)[0]), Type::Region);
	EXPECT_FALSE(project.getEnumType((*resolved)[0]));
}

TEST(ResolveTypes, HereInExtendRegionResolvesToTargetName) {
	auto [project, diags] = resolveFromSource(
		"extern define uses_region(r: Region) -> Bool\n"
		"region RR_BASE {\n"
		"    name: \"Base\"\n"
		"    scene: SCENE_TEST\n"
		"}\n"
		"extend region RR_BASE {\n"
		"    exits { RR_OTHER: uses_region(here) }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
	auto it = project.ExtendRegionDecls.find("RR_BASE");
	ASSERT_NE(it, project.ExtendRegionDecls.end());
	const auto* expr = it->second[0]->sections[0].entries[0].condition.get();
	const auto* resolved = project.getResolvedCallArgs(&std::get<CallExpr>(expr->node));
	ASSERT_NE(resolved, nullptr);
	ASSERT_TRUE(std::holds_alternative<HereRef>((*resolved)[0]->node));
	EXPECT_EQ(std::get<HereRef>((*resolved)[0]->node).resolvedRegion, "RR_BASE");
	EXPECT_EQ(project.getType((*resolved)[0]), Type::Region);
	EXPECT_FALSE(project.getEnumType((*resolved)[0]));
}

TEST(ResolveTypes, HereOutsideRegionIsError) {
	auto [project, diags] = resolveFromSource(
		"extern define uses_region(r: Region) -> Bool\n"
		"define test(): uses_region(here)\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("has type Region"), std::string::npos);
}

TEST(ResolveTypes, RegionParameterDiagnosticUsesEnumIdentity) {
	auto [project, diags] = resolveFromSource(
		"extern define uses_region(r: Region) -> Bool\n"
		"region RR_TEST {\n"
		"    name: \"Test\"\n"
		"    scene: SCENE_TEST\n"
		"    exits { RR_OTHER: uses_region(true) }\n"
		"}\n");

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expected Region, got Bool"), std::string::npos);
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
	EXPECT_NE(diags[0].message.find("match pattern 'RG_HOOKSHOT' is enum 'Item' but expected enum 'Distance'"),
		std::string::npos);
}

TEST(ResolveTypes, MatchPatternEnumIdentityMismatch) {
	// match x { RED: true, APPLE: false } — Enum identity mismatch.
	auto [project, diags] = resolveFromSource(
		"enum Color { RED }\n"
		"enum Fruit { APPLE }\n"
		"define test_fn(x):\n"
		"    match x {\n"
		"        RED: true\n"
		"        APPLE: false\n"
		"    }\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("match pattern 'APPLE' is enum 'Fruit' but expected enum 'Color'"),
		std::string::npos);
}

TEST(ResolveTypes, MatchDiscriminantEnumIdentityMismatch) {
	// Discriminant x defaults to Fruit.APPLE, but pattern is Color.RED.
	auto [project, diags] = resolveFromSource(
		"enum Color { RED }\n"
		"enum Fruit { APPLE }\n"
		"define test_fn(x = Fruit.APPLE):\n"
		"    match x {\n"
		"        RED: true\n"
		"    }\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("match discriminant 'x' is enum 'Fruit' but patterns are enum 'Color'"),
		std::string::npos);
}

TEST(ResolveTypes, MatchPatternAmbiguousBareIdentifierError) {
	// Bare SHARED is ambiguous across enums inside match patterns too.
	auto [project, diags] = resolveFromSource(
		"enum Alpha { SHARED }\n"
		"enum Beta { SHARED }\n"
		"define test_fn(x):\n"
		"    match x {\n"
		"        SHARED: true\n"
		"    }\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("ambiguous identifier 'SHARED'"), std::string::npos);
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
	EXPECT_EQ(project.getType(project.DefineDecls.at("test_fn")->body.get()), Type::Enum);
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
		"match discriminant 'd' is enum 'Item' but patterns are enum 'Distance'"),
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
	const auto* decl = project.DefineDecls.at("foo");
	ASSERT_EQ(decl->params.size(), 1u);
	EXPECT_EQ(project.getType(&decl->params[0]), Type::Enum);
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Bool);
}

TEST(ResolveTypes, MatchDefaultArmAllowed) {
	auto [project, diags] = resolveFromSource(
		"define foo(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE: true\n"
		"        _: false\n"
		"    }\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Bool);
}

TEST(ResolveTypes, MatchDefaultArmMustBeLast) {
	auto [project, diags] = resolveFromSource(
		"define foo(d: Distance):\n"
		"    match d {\n"
		"        _: true\n"
		"        ED_CLOSE: false\n"
		"    }\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("match wildcard '_' arm must be last"),
		std::string::npos);
}

TEST(ResolveTypes, MatchDefaultPatternMustBeStandalone) {
	auto [project, diags] = resolveFromSource(
		"define foo(d: Distance):\n"
		"    match d {\n"
		"        ED_CLOSE or _: true\n"
		"    }\n");
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("match wildcard '_' must be a standalone pattern"),
		std::string::npos);
}

TEST(ResolveTypes, MatchOnlyDefaultDoesNotInferDiscriminant) {
	auto [project, diags] = resolveFromSource(
		"define foo(d):\n"
		"    match d {\n"
		"        _: true\n"
		"    }\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Bool);
}

TEST(ResolveTypes, ParamInferredFromLogicalContext) {
	auto [project, diags] = resolveFromSource(
		"define foo(wall_or_floor):\n"
		"    wall_or_floor and true\n");
	EXPECT_TRUE(diags.empty());
	const auto* decl = project.DefineDecls.at("foo");
	ASSERT_EQ(decl->params.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(decl->body->node));
	const auto& body = std::get<BinaryExpr>(decl->body->node);
	ASSERT_TRUE(std::holds_alternative<Identifier>(body.left->node));
	EXPECT_EQ(project.getType(&decl->params[0]), Type::Bool);
	EXPECT_EQ(project.getType(decl->body.get()), Type::Bool);
	EXPECT_EQ(std::get<Identifier>(body.left->node).kind, IdentifierKind::Parameter);
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
	EXPECT_EQ(project.getType(hasCall.args[0].value.get()), Type::Enum);
}

// -- Setting comparison -------------------------------------------------------

TEST(ResolveTypes, SettingIsComparison) {
	// setting(RSK_FOREST) is RO_CLOSED_FOREST_ON
	// → setting() returns Int and RO_ is an enum value, so == is Bool.
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
	// setting(RSK_SUNLIGHT_ARROWS) and true — Int is bool-compatible.
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

TEST(TypeAnnotation, CoreTypes) {
	EXPECT_EQ(typeFromAnnotation("Bool"),       Type::Bool);
	EXPECT_EQ(typeFromAnnotation("Int"),        Type::Int);
	EXPECT_EQ(typeFromAnnotation("Callable"),   Type::Callable);
	EXPECT_EQ(typeFromAnnotation("Condition"),  Type::Condition);
	EXPECT_EQ(typeFromAnnotation("Enum"),       Type::Enum);
	EXPECT_EQ(typeFromAnnotation("Region"),     Type::Region);
	EXPECT_EQ(typeFromAnnotation("Event"),      Type::Event);
	EXPECT_EQ(typeFromAnnotation("Location"),   Type::Location);
	EXPECT_FALSE(typeFromAnnotation("Setting").has_value());
	EXPECT_FALSE(typeFromAnnotation("Check").has_value());
}

TEST(TypeAnnotation, DomainEnumCompatibilityUsesExactNames) {
	EXPECT_TRUE(isDomainEnumCompatible(Type::Region, "Region"));
	EXPECT_TRUE(isDomainEnumCompatible(Type::Event, "Event"));
	EXPECT_TRUE(isDomainEnumCompatible(Type::Location, "Location"));
	EXPECT_FALSE(isDomainEnumCompatible(Type::Event, "Logic"));
	EXPECT_FALSE(isDomainEnumCompatible(Type::Location, "Check"));
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
	const auto* body = project.DefineDecls.at("foo")->body.get();
	ASSERT_NE(body, nullptr);
	ASSERT_TRUE(std::holds_alternative<Identifier>(body->node));
	EXPECT_EQ(project.getType(body), Type::Enum);
	EXPECT_EQ(project.getEnumType(body), "Distance");
	EXPECT_EQ(std::get<Identifier>(body->node).kind, IdentifierKind::Parameter);
}

TEST(ResolveTypes, DefineParamWithProjectEnumAnnotation) {
	auto [project, diags] = resolveFromSource(
		"enum Color { RED, BLUE }\n"
		"define select(color: Color): color\n");

	EXPECT_TRUE(diags.empty());
	const auto* decl = project.DefineDecls.at("select");
	const auto* body = decl->body.get();
	ASSERT_NE(body, nullptr);
	EXPECT_EQ(project.getType(&decl->params[0]), Type::Enum);
	EXPECT_EQ(project.getEnumType(&decl->params[0]), "Color");
	EXPECT_EQ(project.getType(body), Type::Enum);
	EXPECT_EQ(project.getEnumType(body), "Color");
}

TEST(ResolveTypes, DeclaredRegionsEventsAndLocationsAreTypedValues) {
	auto [project, diags] = resolveFromSource(
		"region RR_TARGET {\n"
		"  events { EVENT_OPEN: true }\n"
		"  locations { RC_CHEST: true }\n"
		"}\n"
		"define region_value(): RR_TARGET\n"
		"define event_value(): EVENT_OPEN\n"
		"define location_value(): RC_CHEST\n"
		"extern define take_region(value: Region) -> Bool\n"
		"extern define take_event(value: Event) -> Bool\n"
		"extern define take_location(value: Location) -> Bool\n"
		"extern define flag(value: Event) -> Bool\n"
		"define use_values(): take_region(RR_TARGET) and take_region(RR_NONE)\n"
		"  and take_event(EVENT_OPEN) and flag(EVENT_OPEN)\n"
		"  and take_location(RC_CHEST) and (check_price(RC_CHEST) >= 0)\n");

	ASSERT_TRUE(diags.empty()) << diags.front().message;
	const auto expectValue = [&](std::string_view defineName, Type type) {
		const auto* body = project.DefineDecls.at(std::string(defineName))->body.get();
		EXPECT_EQ(project.getType(body), type);
		ASSERT_TRUE(std::holds_alternative<Identifier>(body->node));
		EXPECT_EQ(std::get<Identifier>(body->node).kind, IdentifierKind::DeclaredValue);
	};
	expectValue("region_value", Type::Region);
	expectValue("event_value", Type::Event);
	expectValue("location_value", Type::Location);
}

TEST(ResolveTypes, DeclaredDomainValuesRejectWrongCategories) {
	auto [project, diags] = resolveFromSource(
		"region RR_TARGET { events { EVENT_OPEN: true } }\n"
		"extern define take_event(value: Event) -> Bool\n"
		"define wrong(): take_event(RR_TARGET)\n");

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expected Event, got Region"), std::string::npos);
}

TEST(ResolveTypes, RepeatedLocationDeclarationsShareLocationType) {
	auto [project, diags] = resolveFromSource(
		"region RR_FIRST { locations { RC_SHARED: true } }\n"
		"region RR_SECOND { locations { RC_SHARED: true } }\n"
		"define location_value(): RC_SHARED\n");

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.LocationDecls.at("RC_SHARED").size(), 2u);
	EXPECT_EQ(project.getType(project.DefineDecls.at("location_value")->body.get()),
		Type::Location);
}

TEST(ResolveTypes, DeclaredDomainValuesCompareWithLegacyEnumSentinels) {
	auto [project, diags] = resolveFromSource(
		"region RR_TARGET {\n"
		"  events { LOGIC_OPEN: true }\n"
		"  locations { RC_CHEST: true }\n"
		"}\n"
		"define compare_region(): RR_TARGET != RR_NONE\n"
		"define compare_event(): LOGIC_OPEN != LOGIC_NONE\n"
		"define compare_location(): RC_CHEST != RC_UNKNOWN_CHECK\n");

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("compare_region")->body.get()), Type::Bool);
	EXPECT_EQ(project.getType(project.DefineDecls.at("compare_event")->body.get()), Type::Bool);
	EXPECT_EQ(project.getType(project.DefineDecls.at("compare_location")->body.get()), Type::Bool);
}

TEST(ResolveTypes, DefineParamWithDefault) {
	// define foo(d = ED_CLOSE): d
	auto [project, diags] = resolveFromSource(
		"define foo(d = ED_CLOSE): d\n");
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(project.DefineDecls.at("foo")->body.get()), Type::Enum);
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

