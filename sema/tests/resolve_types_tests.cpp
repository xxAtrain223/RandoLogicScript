#include <gtest/gtest.h>

#include "ast.h"
#include "parser.h"
#include "sema.h"

// Internal header for direct unit testing.
#include "collect_declarations.h"
#include "resolve_types.h"

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

// Helper: wrap an expression in a region entry so resolveTypes will visit it.
static Project makeProjectWithExpr(ExprPtr expr) {
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Entry> entries;
	entries.emplace_back("TEST_LOC", std::move(expr));

	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Locations, std::move(entries));

	file.declarations.emplace_back(RegionDecl(
		"RR_TEST",
		RegionBody("SCENE_TEST", TimePasses::Auto, {}, std::move(sections))
	));

	project.files.push_back(std::move(file));
	collectDeclarations(project);
	return project;
}

// Helper: get the Expr* of the first region entry's condition.
static const Expr* getEntryExpr(const Project& project) {
	auto& region = std::get<RegionDecl>(project.files[0].declarations[0]);
	return region.body.sections[0].entries[0].condition.get();
}

// -- Leaf types ---------------------------------------------------------------

TEST(ResolveTypes, BoolLiteral) {
	auto project = makeProjectWithExpr(makeExpr(BoolLiteral{true}));
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, IntLiteral) {
	auto project = makeProjectWithExpr(makeExpr(IntLiteral{42}));
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Int);
}

TEST(ResolveTypes, KeywordExpr) {
	auto project = makeProjectWithExpr(makeExpr(KeywordExpr{Keyword::IsChild}));
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, IdentifierEnum) {
	auto project = makeProjectWithExpr(makeExpr(Identifier{"RG_HOOKSHOT"}));
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Item);
}

TEST(ResolveTypes, IdentifierUnknown) {
	auto project = makeProjectWithExpr(makeExpr(Identifier{"distance"}));
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown identifier 'distance'"), std::string::npos);
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Error);
}

// -- Unary --------------------------------------------------------------------

TEST(ResolveTypes, UnaryNotBool) {
	auto project = makeProjectWithExpr(
		makeExpr(UnaryExpr(UnaryOp::Not, makeExpr(BoolLiteral{true})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, UnaryNotIntImplicitConvert) {
	// not 42 — Int is bool-compatible, so no error.
	auto project = makeProjectWithExpr(
		makeExpr(UnaryExpr(UnaryOp::Not, makeExpr(IntLiteral{42})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, UnaryNotTypeMismatch) {
	// not RG_HOOKSHOT — Item is not bool-compatible.
	auto project = makeProjectWithExpr(
		makeExpr(UnaryExpr(UnaryOp::Not, makeExpr(Identifier{"RG_HOOKSHOT"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("'not' requires a Bool operand"), std::string::npos);
}

// -- Binary logical -----------------------------------------------------------

TEST(ResolveTypes, BinaryAndBool) {
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::And,
			makeExpr(BoolLiteral{true}),
			makeExpr(BoolLiteral{false})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, BinaryOrIntImplicit) {
	// 42 or true — Int on left is bool-compatible.
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Or,
			makeExpr(IntLiteral{42}),
			makeExpr(BoolLiteral{true})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, BinaryAndTypeMismatch) {
	// RG_HOOKSHOT and true — Item is not bool-compatible.
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::And,
			makeExpr(Identifier{"RG_HOOKSHOT"}),
			makeExpr(BoolLiteral{true})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("'and' requires Bool operands"), std::string::npos);
}

// -- Comparison ---------------------------------------------------------------

TEST(ResolveTypes, EqualitySameType) {
	// RG_HOOKSHOT == RG_FAIRY_BOW  (both Item)
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Eq,
			makeExpr(Identifier{"RG_HOOKSHOT"}),
			makeExpr(Identifier{"RG_FAIRY_BOW"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, EqualityTypeMismatch) {
	// RG_HOOKSHOT == RE_ARMOS  (Item vs Enemy)
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Eq,
			makeExpr(Identifier{"RG_HOOKSHOT"}),
			makeExpr(Identifier{"RE_ARMOS"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("incompatible types"), std::string::npos);
}

TEST(ResolveTypes, OrderingInts) {
	// 3 >= 1
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::GtEq,
			makeExpr(IntLiteral{3}),
			makeExpr(IntLiteral{1})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, OrderingNonInt) {
	// RG_HOOKSHOT > RG_BOW  — Item is not Int.
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Gt,
			makeExpr(Identifier{"RG_HOOKSHOT"}),
			makeExpr(Identifier{"RG_FAIRY_BOW"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 2u); // both sides flagged
}

// -- Arithmetic ---------------------------------------------------------------

TEST(ResolveTypes, ArithmeticInts) {
	// 3 + 1
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Add,
			makeExpr(IntLiteral{3}),
			makeExpr(IntLiteral{1})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Int);
}

TEST(ResolveTypes, ArithmeticNonInt) {
	// true + 1
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Add,
			makeExpr(BoolLiteral{true}),
			makeExpr(IntLiteral{1})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("arithmetic requires Int"), std::string::npos);
}

// -- Ternary ------------------------------------------------------------------

TEST(ResolveTypes, TernaryOk) {
	// true ? ED_CLOSE : ED_FAR
	auto project = makeProjectWithExpr(
		makeExpr(TernaryExpr(
			makeExpr(BoolLiteral{true}),
			makeExpr(Identifier{"ED_CLOSE"}),
			makeExpr(Identifier{"ED_FAR"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Distance);
}

TEST(ResolveTypes, TernaryBranchMismatch) {
	// true ? ED_CLOSE : RG_HOOKSHOT
	auto project = makeProjectWithExpr(
		makeExpr(TernaryExpr(
			makeExpr(BoolLiteral{true}),
			makeExpr(Identifier{"ED_CLOSE"}),
			makeExpr(Identifier{"RG_HOOKSHOT"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("different types"), std::string::npos);
}

TEST(ResolveTypes, TernaryBoolCompatibleBranchesUnify) {
	// true ? 1 : false  →  Int + Bool both bool-compatible → Bool
	auto project = makeProjectWithExpr(
		makeExpr(TernaryExpr(
			makeExpr(BoolLiteral{true}),
			makeExpr(IntLiteral{1}),
			makeExpr(BoolLiteral{false})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 0u);
	ASSERT_EQ(diags.size(), 1u);
	EXPECT_EQ(diags[0].level, DiagnosticLevel::Warning);
	EXPECT_NE(diags[0].message.find("implicitly converted to Bool"), std::string::npos);
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, TernaryCondNotBool) {
	// RG_HOOKSHOT ? 1 : 2
	auto project = makeProjectWithExpr(
		makeExpr(TernaryExpr(
			makeExpr(Identifier{"RG_HOOKSHOT"}),
			makeExpr(IntLiteral{1}),
			makeExpr(IntLiteral{2})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("ternary condition must be Bool"), std::string::npos);
	// But the result type is still Int from the branches.
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Int);
}

// -- Host function calls ------------------------------------------------------

TEST(ResolveTypes, HostCallHas) {
	// has(RG_HOOKSHOT)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RG_HOOKSHOT"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("has", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, HostCallSetting) {
	// setting(RSK_SUNLIGHT_ARROWS)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RSK_SUNLIGHT_ARROWS"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("setting", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Setting);
}

TEST(ResolveTypes, HostCallReturnsInt) {
	// hearts()
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("hearts", {}))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Int);
}

TEST(ResolveTypes, HostCallWrongArgType) {
	// has(RE_ARMOS) — Enemy where Item expected.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("has", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expected Item, got Enemy"), std::string::npos);
	// Return type is still Bool.
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, HostCallTooFewArgs) {
	// has() — missing required arg.
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("has", {}))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1 argument(s), got 0"), std::string::npos);
}

TEST(ResolveTypes, HostCallTooManyArgs) {
	// has(RG_HOOKSHOT, RG_FAIRY_BOW) — too many.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RG_HOOKSHOT"}));
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RG_FAIRY_BOW"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("has", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1 argument(s), got 2"), std::string::npos);
}

TEST(ResolveTypes, HostCallOptionalArgOmitted) {
	// check_price() — 0 args, optional Check param.
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("check_price", {}))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Int);
}

TEST(ResolveTypes, HostCallOptionalArgProvided) {
	// check_price(RC_SPIRIT_CHEST)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RC_SPIRIT_CHEST"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("check_price", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Int);
}

TEST(ResolveTypes, HostCallMultipleArgs) {
	// keys(SCENE_SPIRIT_TEMPLE, 3)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"SCENE_SPIRIT_TEMPLE"}));
	args.emplace_back(std::nullopt, makeExpr(IntLiteral{3}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("keys", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

// -- Enemy built-in calls -----------------------------------------------------

TEST(ResolveTypes, EnemyBuiltinOk) {
	// can_kill(RE_ARMOS)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_kill", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, EnemyBuiltinWithOptionalArgs) {
	// can_kill(RE_ARMOS, ED_CLOSE) — optional distance arg.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	args.emplace_back(std::nullopt, makeExpr(Identifier{"ED_CLOSE"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_kill", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, EnemyBuiltinAllArgs) {
	// can_kill(RE_ARMOS, ED_CLOSE, true, 1, false, false)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	args.emplace_back(std::nullopt, makeExpr(Identifier{"ED_CLOSE"}));
	args.emplace_back(std::nullopt, makeExpr(BoolLiteral{true}));
	args.emplace_back(std::nullopt, makeExpr(IntLiteral{1}));
	args.emplace_back(std::nullopt, makeExpr(BoolLiteral{false}));
	args.emplace_back(std::nullopt, makeExpr(BoolLiteral{false}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_kill", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, EnemyBuiltinNotEnemy) {
	// can_kill(RG_HOOKSHOT) — Item, not Enemy.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RG_HOOKSHOT"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_kill", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("argument 1 expected Enemy"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinNoArgs) {
	// can_kill() — missing Enemy arg.
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_kill", {}))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1-6 argument(s), got 0"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinTooManyArgs) {
	// can_kill with 7 args — one too many.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	for (int i = 0; i < 6; ++i)
		args.emplace_back(std::nullopt, makeExpr(BoolLiteral{false}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_kill", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("expects 1-6 argument(s), got 7"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinWrongOptionalArgType) {
	// can_kill(RE_ARMOS, RG_HOOKSHOT) — second arg should be Distance.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RG_HOOKSHOT"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_kill", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("argument 2 expected Distance"), std::string::npos);
}

TEST(ResolveTypes, EnemyBuiltinCanPassSignature) {
	// can_pass(RE_ARMOS, ED_CLOSE, true) — all 3 args.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	args.emplace_back(std::nullopt, makeExpr(Identifier{"ED_CLOSE"}));
	args.emplace_back(std::nullopt, makeExpr(BoolLiteral{true}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_pass", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
}

TEST(ResolveTypes, EnemyBuiltinCanAvoidSignature) {
	// can_avoid(RE_ARMOS, false, 2) — all 3 args.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	args.emplace_back(std::nullopt, makeExpr(BoolLiteral{false}));
	args.emplace_back(std::nullopt, makeExpr(IntLiteral{2}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_avoid", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
}

TEST(ResolveTypes, EnemyBuiltinCanGetDropSignature) {
	// can_get_drop(RE_ARMOS, ED_CLOSE, false) — all 3 args.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_ARMOS"}));
	args.emplace_back(std::nullopt, makeExpr(Identifier{"ED_CLOSE"}));
	args.emplace_back(std::nullopt, makeExpr(BoolLiteral{false}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("can_get_drop", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
}

// -- Unknown function ---------------------------------------------------------

TEST(ResolveTypes, UnknownFunction) {
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("nonexistent_func", {}))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown function 'nonexistent_func'"), std::string::npos);
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Error);
}

// -- User define calls --------------------------------------------------------

TEST(ResolveTypes, UserDefineCallResolvesReturnType) {
	// A define whose body is typed first, then called from a region entry.
	Project project;
	File file;
	file.path = "test.rls";

	// Define: has_explosives(): true
	file.declarations.emplace_back(DefineDecl(
		"has_explosives", {}, makeExpr(BoolLiteral{true})
	));

	// Region calling it.
	std::vector<Entry> entries;
	entries.emplace_back("TEST_LOC",
		makeExpr(CallExpr("has_explosives", {})));
	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Locations, std::move(entries));
	file.declarations.emplace_back(RegionDecl(
		"RR_TEST",
		RegionBody("SCENE_TEST", TimePasses::Auto, {}, std::move(sections))
	));

	project.files.push_back(std::move(file));
	collectDeclarations(project);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	auto& region = std::get<RegionDecl>(project.files[0].declarations[1]);
	auto* callExpr = region.body.sections[0].entries[0].condition.get();
	EXPECT_EQ(project.getType(callExpr), Type::Bool);
}

// -- Shared block -------------------------------------------------------------

TEST(ResolveTypes, SharedBlockBool) {
	std::vector<SharedBranch> branches;
	branches.emplace_back(std::nullopt, makeExpr(BoolLiteral{true}));
	branches.emplace_back(
		std::optional<std::string>{"RR_OTHER"},
		makeExpr(BoolLiteral{false}));

	auto project = makeProjectWithExpr(
		makeExpr(SharedBlock(false, std::move(branches)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

// -- Any-age block ------------------------------------------------------------

TEST(ResolveTypes, AnyAgeBlockBool) {
	auto project = makeProjectWithExpr(
		makeExpr(AnyAgeBlock(makeExpr(BoolLiteral{true})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, AnyAgeBlockNonBool) {
	// any_age { RG_HOOKSHOT } — body is Item, not Bool.
	auto project = makeProjectWithExpr(
		makeExpr(AnyAgeBlock(makeExpr(Identifier{"RG_HOOKSHOT"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("any_age body must be Bool"), std::string::npos);
}

// -- Match expression ---------------------------------------------------------

TEST(ResolveTypes, MatchExprBasic) {
	// match distance { ED_CLOSE: true, ED_FAR: false }
	std::vector<MatchArm> arms;
	arms.emplace_back(
		std::vector<std::string>{"ED_CLOSE"},
		makeExpr(BoolLiteral{true}), true);
	arms.emplace_back(
		std::vector<std::string>{"ED_FAR"},
		makeExpr(BoolLiteral{false}), false);

	auto project = makeProjectWithExpr(
		makeExpr(MatchExpr("distance", std::move(arms)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, MatchPatternTypeMismatch) {
	// match x { ED_CLOSE: true, RG_HOOKSHOT: false } — Distance vs Item.
	std::vector<MatchArm> arms;
	arms.emplace_back(
		std::vector<std::string>{"ED_CLOSE"},
		makeExpr(BoolLiteral{true}), false);
	arms.emplace_back(
		std::vector<std::string>{"RG_HOOKSHOT"},
		makeExpr(BoolLiteral{false}), false);

	auto project = makeProjectWithExpr(
		makeExpr(MatchExpr("x", std::move(arms)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("match pattern 'RG_HOOKSHOT' is Item but expected Distance"),
		std::string::npos);
}

TEST(ResolveTypes, MatchMultiPatternArm) {
	// match x { ED_CLOSE or ED_SHORT_JUMPSLASH: true }
	std::vector<MatchArm> arms;
	arms.emplace_back(
		std::vector<std::string>{"ED_CLOSE", "ED_SHORT_JUMPSLASH"},
		makeExpr(BoolLiteral{true}), false);

	auto project = makeProjectWithExpr(
		makeExpr(MatchExpr("x", std::move(arms)))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, MatchBoolCompatibleArmsUnifyWithWarning) {
	// match x { RSK_A: 1 | RSK_B: true } → Bool + Int → unify to Bool with warning
	std::vector<MatchArm> arms;
	arms.emplace_back(
		std::vector<std::string>{"RSK_A"},
		makeExpr(IntLiteral{1}), false);
	arms.emplace_back(
		std::vector<std::string>{"RSK_B"},
		makeExpr(BoolLiteral{true}), false);

	auto project = makeProjectWithExpr(
		makeExpr(MatchExpr("x", std::move(arms)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 0u);
	ASSERT_EQ(diags.size(), 1u);
	EXPECT_EQ(diags[0].level, DiagnosticLevel::Warning);
	EXPECT_NE(diags[0].message.find("implicitly converted to Bool"),
		std::string::npos);
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

// -- Error poisoning ----------------------------------------------------------

TEST(ResolveTypes, ErrorPoisonSuppressesCascade) {
	// unknown_id and true — the 'and' should not report an extra type error
	// for the left side being Error; only the "unknown identifier" diagnostic.
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::And,
			makeExpr(Identifier{"unknown_id"}),
			makeExpr(BoolLiteral{true})))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown identifier 'unknown_id'"), std::string::npos);
	// Result is still Bool — Error doesn't propagate upward from 'and'.
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, ErrorArgDoesNotCascadeInCall) {
	// has(unknown_id) — the unknown identifier is diagnosed once,
	// but the arg type check (Item vs Error) is suppressed.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"unknown_id"}));
	auto project = makeProjectWithExpr(
		makeExpr(CallExpr("has", std::move(args)))
	);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u); // Only the "unknown identifier" error.
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

// -- Sub-expression types populated -------------------------------------------

TEST(ResolveTypes, SubExprTypesPopulated) {
	// has(RG_HOOKSHOT) and can_use(RG_FAIRY_BOW)
	std::vector<Arg> hasArgs;
	hasArgs.emplace_back(std::nullopt, makeExpr(Identifier{"RG_HOOKSHOT"}));
	std::vector<Arg> canUseArgs;
	canUseArgs.emplace_back(std::nullopt, makeExpr(Identifier{"RG_FAIRY_BOW"}));

	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::And,
			makeExpr(CallExpr("has", std::move(hasArgs))),
			makeExpr(CallExpr("can_use", std::move(canUseArgs)))))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	// Root is Bool.
	auto* root = getEntryExpr(project);
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
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RSK_FOREST"}));
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Eq,
			makeExpr(CallExpr("setting", std::move(args))),
			makeExpr(Identifier{"RO_CLOSED_FOREST_ON"})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

TEST(ResolveTypes, SettingBoolTruthiness) {
	// setting(RSK_SUNLIGHT_ARROWS) and true — Setting is bool-compatible.
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RSK_SUNLIGHT_ARROWS"}));
	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::And,
			makeExpr(CallExpr("setting", std::move(args))),
			makeExpr(BoolLiteral{true})))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
}

// -- Composite expression -----------------------------------------------------

TEST(ResolveTypes, CompositeExpression) {
	// has(RG_HOOKSHOT) or (hearts() >= 3 and trick(RT_SPIRIT_CHILD_CHU))
	std::vector<Arg> hasArgs;
	hasArgs.emplace_back(std::nullopt, makeExpr(Identifier{"RG_HOOKSHOT"}));
	std::vector<Arg> trickArgs;
	trickArgs.emplace_back(std::nullopt, makeExpr(Identifier{"RT_SPIRIT_CHILD_CHU"}));

	auto project = makeProjectWithExpr(
		makeExpr(BinaryExpr(BinaryOp::Or,
			makeExpr(CallExpr("has", std::move(hasArgs))),
			makeExpr(BinaryExpr(BinaryOp::And,
				makeExpr(BinaryExpr(BinaryOp::GtEq,
					makeExpr(CallExpr("hearts", {})),
					makeExpr(IntLiteral{3}))),
				makeExpr(CallExpr("trick", std::move(trickArgs)))))))
	);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.getType(getEntryExpr(project)), Type::Bool);
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
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("d", std::optional<std::string>{"Distance"}, nullptr);

	file.declarations.emplace_back(DefineDecl(
		"foo", std::move(params), makeExpr(Identifier{"d"})
	));

	project.files.push_back(std::move(file));
	collectDeclarations(project);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	auto& def = std::get<DefineDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(project.getType(def.body.get()), Type::Distance);
}

TEST(ResolveTypes, DefineParamWithDefault) {
	// define foo(d = ED_CLOSE): d
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("d", std::nullopt, makeExpr(Identifier{"ED_CLOSE"}));

	file.declarations.emplace_back(DefineDecl(
		"foo", std::move(params), makeExpr(Identifier{"d"})
	));

	project.files.push_back(std::move(file));
	collectDeclarations(project);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	auto& def = std::get<DefineDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(project.getType(def.body.get()), Type::Distance);
}

TEST(ResolveTypes, DefineParamUntyped) {
	// define foo(x): x — type unknown, no error for the identifier itself
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("x", std::nullopt, nullptr);

	file.declarations.emplace_back(DefineDecl(
		"foo", std::move(params), makeExpr(Identifier{"x"})
	));

	project.files.push_back(std::move(file));
	collectDeclarations(project);
	auto diags = resolveTypes(project);
	// No "unknown identifier" error — x is a known parameter.
	EXPECT_EQ(countErrors(diags), 0u);
	// But the body type is Error since x's type is unknown.
	auto& def = std::get<DefineDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(project.getType(def.body.get()), Type::Error);
}

TEST(ResolveTypes, DefineParamBadAnnotation) {
	// define foo(x: Foo): x — unknown type annotation
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("x", std::optional<std::string>{"Foo"}, nullptr);

	file.declarations.emplace_back(DefineDecl(
		"foo", std::move(params), makeExpr(Identifier{"x"})
	));

	project.files.push_back(std::move(file));
	collectDeclarations(project);
	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown type annotation 'Foo'"),
		std::string::npos);
}

TEST(ResolveTypes, DefineParamUsedInExpression) {
	// define foo(d: Distance): d == ED_CLOSE
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("d", std::optional<std::string>{"Distance"}, nullptr);

	file.declarations.emplace_back(DefineDecl(
		"foo", std::move(params),
		makeExpr(BinaryExpr(BinaryOp::Eq,
			makeExpr(Identifier{"d"}),
			makeExpr(Identifier{"ED_CLOSE"})))
	));

	project.files.push_back(std::move(file));
	collectDeclarations(project);
	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	auto& def = std::get<DefineDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(project.getType(def.body.get()), Type::Bool);
}

TEST(ResolveTypes, EnemyFieldKillParamScope) {
	// enemy RE_TEST { kill(distance, wallOrFloor): wallOrFloor }
	// can_kill signature: Enemy, Distance?, Bool?, Int?, Bool?, Bool?
	// Field params (skip Enemy): distance → Distance, wallOrFloor → Bool
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("distance", std::nullopt, nullptr);
	params.emplace_back("wallOrFloor", std::nullopt, nullptr);

	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Kill, std::move(params),
		makeExpr(Identifier{"wallOrFloor"}));

	file.declarations.emplace_back(EnemyDecl("RE_TEST", std::move(fields)));
	project.files.push_back(std::move(file));
	collectDeclarations(project);

	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	auto& enemy = std::get<EnemyDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(project.getType(enemy.fields[0].body.get()), Type::Bool);
}

TEST(ResolveTypes, EnemyFieldAvoidParamScope) {
	// enemy RE_TEST { avoid(grounded, quantity): quantity }
	// can_avoid signature: Enemy, Bool?, Int?
	// Field params (skip Enemy): grounded → Bool, quantity → Int
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("grounded", std::nullopt, nullptr);
	params.emplace_back("quantity", std::nullopt, nullptr);

	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Avoid, std::move(params),
		makeExpr(Identifier{"quantity"}));

	file.declarations.emplace_back(EnemyDecl("RE_TEST", std::move(fields)));
	project.files.push_back(std::move(file));
	collectDeclarations(project);

	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	auto& enemy = std::get<EnemyDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(project.getType(enemy.fields[0].body.get()), Type::Int);
}

TEST(ResolveTypes, EnemyFieldBodyNotBoolCompatible) {
	// enemy RE_TEST { kill(distance): distance }
	// Body is Distance — not bool-compatible.
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("distance", std::nullopt, nullptr);

	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Kill, std::move(params),
		makeExpr(Identifier{"distance"}));

	file.declarations.emplace_back(EnemyDecl("RE_TEST", std::move(fields)));
	project.files.push_back(std::move(file));
	collectDeclarations(project);

	auto diags = resolveTypes(project);
	EXPECT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("enemy field"), std::string::npos);
	EXPECT_NE(diags[0].message.find("must be Bool"), std::string::npos);
}

TEST(ResolveTypes, EnemyFieldDropParamScope) {
	// enemy RE_TEST { drop(distance): distance == ED_CLOSE }
	// can_get_drop signature: Enemy, Distance?, Bool?
	// Field params (skip Enemy): distance → Distance
	Project project;
	File file;
	file.path = "test.rls";

	std::vector<Param> params;
	params.emplace_back("distance", std::nullopt, nullptr);

	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Drop, std::move(params),
		makeExpr(BinaryExpr(BinaryOp::Eq,
			makeExpr(Identifier{"distance"}),
			makeExpr(Identifier{"ED_CLOSE"}))));

	file.declarations.emplace_back(EnemyDecl("RE_TEST", std::move(fields)));
	project.files.push_back(std::move(file));
	collectDeclarations(project);

	auto diags = resolveTypes(project);
	EXPECT_TRUE(diags.empty());

	auto& enemy = std::get<EnemyDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(project.getType(enemy.fields[0].body.get()), Type::Bool);
}
