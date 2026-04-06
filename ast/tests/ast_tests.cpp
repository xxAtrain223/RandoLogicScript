#include <gtest/gtest.h>

#include "ast.h"

using namespace rls::ast;

// == Leaf expression nodes ====================================================

TEST(ExprTests, BoolLiteralTrue) {
	auto expr = makeExpr(BoolLiteral{true});
	ASSERT_TRUE(std::holds_alternative<BoolLiteral>(expr->node));
	EXPECT_TRUE(std::get<BoolLiteral>(expr->node).value);
}

TEST(ExprTests, BoolLiteralFalse) {
	auto expr = makeExpr(BoolLiteral{false});
	ASSERT_TRUE(std::holds_alternative<BoolLiteral>(expr->node));
	EXPECT_FALSE(std::get<BoolLiteral>(expr->node).value);
}

TEST(ExprTests, IntLiteral) {
	auto expr = makeExpr(IntLiteral{42});
	ASSERT_TRUE(std::holds_alternative<IntLiteral>(expr->node));
	EXPECT_EQ(std::get<IntLiteral>(expr->node).value, 42);
}

TEST(ExprTests, Identifier) {
	auto expr = makeExpr(Identifier{"RG_HOOKSHOT"});
	ASSERT_TRUE(std::holds_alternative<Identifier>(expr->node));
	EXPECT_EQ(std::get<Identifier>(expr->node).name, "RG_HOOKSHOT");
}

TEST(ExprTests, Keyword) {
	auto expr = makeExpr(KeywordExpr{Keyword::IsAdult});
	ASSERT_TRUE(std::holds_alternative<KeywordExpr>(expr->node));
	EXPECT_EQ(std::get<KeywordExpr>(expr->node).keyword, Keyword::IsAdult);
}

// == Compound expression nodes ================================================

TEST(ExprTests, UnaryNot) {
	// not true
	auto expr = makeExpr(UnaryExpr(
		UnaryOp::Not,
		makeExpr(BoolLiteral{true})
	));
	ASSERT_TRUE(std::holds_alternative<UnaryExpr>(expr->node));
	const auto& unary = std::get<UnaryExpr>(expr->node);
	EXPECT_EQ(unary.op, UnaryOp::Not);
	ASSERT_TRUE(std::holds_alternative<BoolLiteral>(unary.operand->node));
}

TEST(ExprTests, BinaryAnd) {
	// is_child and RG_HOOKSHOT
	auto expr = makeExpr(BinaryExpr(
		BinaryOp::And,
		makeExpr(KeywordExpr{Keyword::IsChild}),
		makeExpr(Identifier{"RG_HOOKSHOT"})
	));
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(expr->node));
	const auto& bin = std::get<BinaryExpr>(expr->node);
	EXPECT_EQ(bin.op, BinaryOp::And);
	EXPECT_TRUE(std::holds_alternative<KeywordExpr>(bin.left->node));
	EXPECT_TRUE(std::holds_alternative<Identifier>(bin.right->node));
}

TEST(ExprTests, BinaryOr) {
	auto expr = makeExpr(BinaryExpr(
		BinaryOp::Or,
		makeExpr(BoolLiteral{true}),
		makeExpr(BoolLiteral{false})
	));
	const auto& bin = std::get<BinaryExpr>(expr->node);
	EXPECT_EQ(bin.op, BinaryOp::Or);
}

TEST(ExprTests, Comparison) {
	// fire_timer() >= 48
	auto expr = makeExpr(BinaryExpr(
		BinaryOp::GtEq,
		makeExpr(CallExpr("fire_timer", {})),
		makeExpr(IntLiteral{48})
	));
	EXPECT_EQ(std::get<BinaryExpr>(expr->node).op, BinaryOp::GtEq);
}

TEST(ExprTests, Ternary) {
	// condition ? ED_BOMB_THROW : ED_BOOMERANG
	auto expr = makeExpr(TernaryExpr(
		makeExpr(BoolLiteral{true}),
		makeExpr(Identifier{"ED_BOMB_THROW"}),
		makeExpr(Identifier{"ED_BOOMERANG"})
	));
	ASSERT_TRUE(std::holds_alternative<TernaryExpr>(expr->node));
	const auto& tern = std::get<TernaryExpr>(expr->node);
	EXPECT_TRUE(std::holds_alternative<BoolLiteral>(tern.condition->node));
	EXPECT_EQ(std::get<Identifier>(tern.thenBranch->node).name, "ED_BOMB_THROW");
	EXPECT_EQ(std::get<Identifier>(tern.elseBranch->node).name, "ED_BOOMERANG");
}

TEST(ExprTests, CallWithPositionalArgs) {
	// keys(SCENE_SPIRIT_TEMPLE, 3)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"SCENE_SPIRIT_TEMPLE"}));
	args.emplace_back(std::nullopt, makeExpr(IntLiteral{3}));

	auto expr = makeExpr(CallExpr("keys", std::move(args)));
	ASSERT_TRUE(std::holds_alternative<CallExpr>(expr->node));
	const auto& call = std::get<CallExpr>(expr->node);
	EXPECT_EQ(call.function, "keys");
	ASSERT_EQ(call.args.size(), 2u);
	EXPECT_FALSE(call.args[0].name.has_value());
	EXPECT_FALSE(call.args[1].name.has_value());
}

TEST(ExprTests, CallWithNamedArgs) {
	// can_kill(RE_STALFOS, quantity: 2)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{"RE_STALFOS"}));
	args.emplace_back(std::optional<std::string>{"quantity"}, makeExpr(IntLiteral{2}));

	auto expr = makeExpr(CallExpr("can_kill", std::move(args)));
	const auto& call = std::get<CallExpr>(expr->node);
	ASSERT_EQ(call.args.size(), 2u);
	EXPECT_FALSE(call.args[0].name.has_value());
	ASSERT_TRUE(call.args[1].name.has_value());
	EXPECT_EQ(call.args[1].name.value(), "quantity");
}

TEST(ExprTests, CallWithNoArgs) {
	auto expr = makeExpr(CallExpr("has_explosives", {}));
	const auto& call = std::get<CallExpr>(expr->node);
	EXPECT_EQ(call.function, "has_explosives");
	EXPECT_TRUE(call.args.empty());
}

TEST(ExprTests, SharedBlock) {
	// shared { from here: always, from RR_OTHER: condition }
	std::vector<SharedBranch> branches;
	branches.emplace_back(std::nullopt, makeExpr(BoolLiteral{true}));
	branches.emplace_back(std::optional<std::string>{"RR_OTHER"}, makeExpr(Identifier{"some_cond"}));

	auto expr = makeExpr(SharedBlock(false, std::move(branches)));
	ASSERT_TRUE(std::holds_alternative<SharedBlock>(expr->node));
	const auto& shared = std::get<SharedBlock>(expr->node);
	EXPECT_FALSE(shared.anyAge);
	ASSERT_EQ(shared.branches.size(), 2u);
	EXPECT_FALSE(shared.branches[0].region.has_value());
	ASSERT_TRUE(shared.branches[1].region.has_value());
	EXPECT_EQ(shared.branches[1].region.value(), "RR_OTHER");
}

TEST(ExprTests, SharedBlockAnyAge) {
	std::vector<SharedBranch> branches;
	branches.emplace_back(std::nullopt, makeExpr(BoolLiteral{true}));

	auto expr = makeExpr(SharedBlock(true, std::move(branches)));
	EXPECT_TRUE(std::get<SharedBlock>(expr->node).anyAge);
}

TEST(ExprTests, AnyAgeBlock) {
	// any_age { can_kill(RE_ARMOS) }
	auto expr = makeExpr(AnyAgeBlock(
		makeExpr(CallExpr("can_kill", {}))
	));
	ASSERT_TRUE(std::holds_alternative<AnyAgeBlock>(expr->node));
	const auto& inner = std::get<AnyAgeBlock>(expr->node).body;
	EXPECT_TRUE(std::holds_alternative<CallExpr>(inner->node));
}

TEST(ExprTests, MatchExpr) {
	// match distance { ED_CLOSE: expr or, ED_FAR: expr }
	std::vector<MatchArm> arms;
	arms.emplace_back(std::vector<std::string>{"ED_CLOSE"}, makeExpr(BoolLiteral{true}), true);
	arms.emplace_back(std::vector<std::string>{"ED_FAR"}, makeExpr(BoolLiteral{false}), false);

	auto expr = makeExpr(MatchExpr("distance", std::move(arms)));
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(expr->node));
	const auto& m = std::get<MatchExpr>(expr->node);
	EXPECT_EQ(m.discriminant, "distance");
	ASSERT_EQ(m.arms.size(), 2u);
	EXPECT_TRUE(m.arms[0].fallthrough);
	EXPECT_FALSE(m.arms[1].fallthrough);
}

TEST(ExprTests, MatchArmMultiplePatterns) {
	// ED_CLOSE or ED_SHORT_JUMPSLASH: body
	MatchArm arm({"ED_CLOSE", "ED_SHORT_JUMPSLASH"}, makeExpr(BoolLiteral{true}), false);
	EXPECT_EQ(arm.patterns.size(), 2u);
	EXPECT_EQ(arm.patterns[0], "ED_CLOSE");
	EXPECT_EQ(arm.patterns[1], "ED_SHORT_JUMPSLASH");
}

// == Span tracking ============================================================

TEST(ExprTests, SpanPreserved) {
	Span span{"test.rls", {10, 5}, {10, 20}};
	auto expr = makeExpr(BoolLiteral{true}, span);
	EXPECT_EQ(expr->span.file, "test.rls");
	EXPECT_EQ(expr->span.start.line, 10u);
	EXPECT_EQ(expr->span.start.column, 5u);
	EXPECT_EQ(expr->span.end.line, 10u);
	EXPECT_EQ(expr->span.end.column, 20u);
}

TEST(ExprTests, DefaultSpanIsZero) {
	auto expr = makeExpr(BoolLiteral{true});
	EXPECT_TRUE(expr->span.file.empty());
	EXPECT_EQ(expr->span.start.line, 0u);
	EXPECT_EQ(expr->span.start.column, 0u);
	EXPECT_EQ(expr->span.end.line, 0u);
	EXPECT_EQ(expr->span.end.column, 0u);
}

// == Nested expressions =======================================================

TEST(ExprTests, NestedBinaryExpressions) {
	// (a and b) or c
	auto expr = makeExpr(BinaryExpr(
		BinaryOp::Or,
		makeExpr(BinaryExpr(
			BinaryOp::And,
			makeExpr(Identifier{"a"}),
			makeExpr(Identifier{"b"})
		)),
		makeExpr(Identifier{"c"})
	));

	const auto& outerOr = std::get<BinaryExpr>(expr->node);
	EXPECT_EQ(outerOr.op, BinaryOp::Or);
	const auto& innerAnd = std::get<BinaryExpr>(outerOr.left->node);
	EXPECT_EQ(innerAnd.op, BinaryOp::And);
	EXPECT_EQ(std::get<Identifier>(innerAnd.left->node).name, "a");
}

// == Top-level declarations ===================================================

TEST(DeclTests, RegionDecl) {
	// region RR_SPIRIT_TEMPLE_FOYER { scene: SCENE_SPIRIT_TEMPLE, exits { ... } }
	std::vector<Entry> exits;
	exits.emplace_back(
		"RR_SPIRIT_TEMPLE_ENTRYWAY",
		makeExpr(BoolLiteral{true})
	);

	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Exits, std::move(exits));

	RegionDecl region(
		"RR_SPIRIT_TEMPLE_FOYER",
		RegionBody(
			"SCENE_SPIRIT_TEMPLE",
			TimePasses::Auto,
			{},
			std::move(sections)
		)
	);

	EXPECT_EQ(region.name, "RR_SPIRIT_TEMPLE_FOYER");
	ASSERT_TRUE(region.body.scene.has_value());
	EXPECT_EQ(region.body.scene.value(), "SCENE_SPIRIT_TEMPLE");
	EXPECT_EQ(region.body.timePasses, TimePasses::Auto);
	ASSERT_EQ(region.body.sections.size(), 1u);
	EXPECT_EQ(region.body.sections[0].kind, SectionKind::Exits);
	ASSERT_EQ(region.body.sections[0].entries.size(), 1u);
	EXPECT_EQ(region.body.sections[0].entries[0].name, "RR_SPIRIT_TEMPLE_ENTRYWAY");
}

TEST(DeclTests, RegionWithTimePasses) {
	RegionDecl region(
		"RR_HC_GARDEN",
		RegionBody(
			"SCENE_CASTLE_COURTYARD_GUARDS_DAY",
			TimePasses::No,
			{"RA_CASTLE_GROUNDS"},
			{}
		)
	);

	EXPECT_EQ(region.body.timePasses, TimePasses::No);
	ASSERT_EQ(region.body.areas.size(), 1u);
	EXPECT_EQ(region.body.areas[0], "RA_CASTLE_GROUNDS");
}

TEST(DeclTests, ExtendRegionDecl) {
	std::vector<Entry> locations;
	locations.emplace_back(
		"RC_SPIRIT_TEMPLE_LOBBY_POT_1",
		makeExpr(CallExpr("can_break_pots", {}))
	);

	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Locations, std::move(locations));

	ExtendRegionDecl extend(
		"RR_SPIRIT_TEMPLE_FOYER",
		std::move(sections)
	);

	EXPECT_EQ(extend.name, "RR_SPIRIT_TEMPLE_FOYER");
	ASSERT_EQ(extend.sections.size(), 1u);
	EXPECT_EQ(extend.sections[0].kind, SectionKind::Locations);
}

TEST(DeclTests, DefineDecl) {
	// define spirit_explosive_key_logic(): <body>
	DefineDecl define(
		"spirit_explosive_key_logic",
		{},
		makeExpr(BoolLiteral{true})
	);

	EXPECT_EQ(define.name, "spirit_explosive_key_logic");
	EXPECT_TRUE(define.params.empty());
	EXPECT_TRUE(std::holds_alternative<BoolLiteral>(define.body->node));
}

TEST(DeclTests, DefineWithParams) {
	// define can_hit_switch(distance = ED_CLOSE, inWater = false): ...
	std::vector<Param> params;
	params.emplace_back("distance", std::nullopt, makeExpr(Identifier{"ED_CLOSE"}));
	params.emplace_back("inWater", std::nullopt, makeExpr(BoolLiteral{false}));

	DefineDecl define(
		"can_hit_switch",
		std::move(params),
		makeExpr(BoolLiteral{true})
	);

	ASSERT_EQ(define.params.size(), 2u);
	EXPECT_EQ(define.params[0].name, "distance");
	EXPECT_FALSE(define.params[0].type.has_value());
	ASSERT_NE(define.params[0].defaultValue, nullptr);
	EXPECT_EQ(std::get<Identifier>(define.params[0].defaultValue->node).name, "ED_CLOSE");
}

TEST(DeclTests, DefineWithTypedParam) {
	// define foo(d: Distance): ...
	std::vector<Param> params;
	params.emplace_back("d", std::optional<std::string>{"Distance"}, nullptr);

	DefineDecl define(
		"foo",
		std::move(params),
		makeExpr(BoolLiteral{true})
	);

	ASSERT_TRUE(define.params[0].type.has_value());
	EXPECT_EQ(define.params[0].type.value(), "Distance");
	EXPECT_EQ(define.params[0].defaultValue, nullptr);
}

TEST(DeclTests, EnemyDecl) {
	// enemy RE_IRON_KNUCKLE { kill: ..., pass: never }
	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Kill, std::vector<Param>{}, makeExpr(BoolLiteral{true}));
	fields.emplace_back(EnemyFieldKind::Pass, std::vector<Param>{}, makeExpr(BoolLiteral{false}));

	EnemyDecl enemy(
		"RE_IRON_KNUCKLE",
		std::move(fields)
	);

	EXPECT_EQ(enemy.name, "RE_IRON_KNUCKLE");
	ASSERT_EQ(enemy.fields.size(), 2u);
	EXPECT_EQ(enemy.fields[0].kind, EnemyFieldKind::Kill);
	EXPECT_EQ(enemy.fields[1].kind, EnemyFieldKind::Pass);
}

TEST(DeclTests, EnemyFieldWithParams) {
	// kill(wallOrFloor = true): ...
	std::vector<Param> params;
	params.emplace_back("wallOrFloor", std::nullopt, makeExpr(BoolLiteral{true}));

	EnemyField field(
		EnemyFieldKind::Kill,
		std::move(params),
		makeExpr(BoolLiteral{true})
	);

	ASSERT_EQ(field.params.size(), 1u);
	EXPECT_EQ(field.params[0].name, "wallOrFloor");
}

// == File =====================================================================

TEST(FileTests, EmptyFile) {
	File file;
	EXPECT_TRUE(file.path.empty());
	EXPECT_TRUE(file.declarations.empty());
}

TEST(FileTests, FileWithPath) {
	File file;
	file.path = "dungeons/spirit_temple.rls";
	EXPECT_EQ(file.path, "dungeons/spirit_temple.rls");
}

TEST(FileTests, FileWithMixedDeclarations) {
	File file;
	file.path = "test.rls";

	// Add a region
	file.declarations.emplace_back(RegionDecl(
		"RR_TEST_REGION",
		RegionBody("SCENE_TEST", TimePasses::Auto, {}, {})
	));

	// Add a define
	file.declarations.emplace_back(DefineDecl(
		"test_helper",
		std::vector<Param>{},
		makeExpr(BoolLiteral{true})
	));

	// Add an enemy
	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Kill, std::vector<Param>{}, makeExpr(BoolLiteral{true}));
	file.declarations.emplace_back(EnemyDecl("RE_TEST", std::move(fields)));

	ASSERT_EQ(file.declarations.size(), 3u);
	EXPECT_TRUE(std::holds_alternative<RegionDecl>(file.declarations[0]));
	EXPECT_TRUE(std::holds_alternative<DefineDecl>(file.declarations[1]));
	EXPECT_TRUE(std::holds_alternative<EnemyDecl>(file.declarations[2]));
}

// == Project ==================================================================

TEST(ProjectTests, EmptyProject) {
	Project project;
	EXPECT_TRUE(project.files.empty());
}

TEST(ProjectTests, AllDeclarationsAcrossFiles) {
	Project project;

	// File 1: one region
	File file1;
	file1.path = "spirit_temple.rls";
	file1.declarations.emplace_back(RegionDecl(
		"RR_SPIRIT_TEMPLE_FOYER",
		RegionBody("SCENE_SPIRIT_TEMPLE", TimePasses::Auto, {}, {})
	));

	// File 2: one define + one enemy
	File file2;
	file2.path = "enemies.rls";
	file2.declarations.emplace_back(DefineDecl(
		"helper", {}, makeExpr(BoolLiteral{true})
	));
	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Kill, std::vector<Param>{}, makeExpr(BoolLiteral{true}));
	file2.declarations.emplace_back(EnemyDecl("RE_TEST", std::move(fields)));

	project.files.push_back(std::move(file1));
	project.files.push_back(std::move(file2));

	ASSERT_EQ(project.files.size(), 2u);
	EXPECT_EQ(project.files[0].path, "spirit_temple.rls");
	EXPECT_EQ(project.files[1].path, "enemies.rls");
}

// == Type side table ==========================================================

TEST(TypeTableTests, EmptyByDefault) {
	Project project;
	auto expr = makeExpr(BoolLiteral{true});
	EXPECT_FALSE(project.getType(expr.get()).has_value());
}

TEST(TypeTableTests, SetAndGetExprType) {
	Project project;
	auto expr = makeExpr(BoolLiteral{true});
	project.setType(expr.get(), Type::Bool);

	auto result = project.getType(expr.get());
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), Type::Bool);
}

TEST(TypeTableTests, SetAndGetParamType) {
	Param param("distance", std::nullopt, makeExpr(Identifier{"ED_CLOSE"}));
	Project project;
	project.setType(&param, Type::Distance);

	auto result = project.getType(&param);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), Type::Distance);
}

TEST(TypeTableTests, OverwriteType) {
	Project project;
	auto expr = makeExpr(Identifier{"x"});
	project.setType(expr.get(), Type::Error);
	project.setType(expr.get(), Type::Int);

	EXPECT_EQ(project.getType(expr.get()).value(), Type::Int);
}

TEST(TypeTableTests, DistinctExprsHaveDistinctTypes) {
	Project project;
	auto boolExpr = makeExpr(BoolLiteral{true});
	auto intExpr = makeExpr(IntLiteral{42});
	auto identExpr = makeExpr(Identifier{"RG_HOOKSHOT"});

	project.setType(boolExpr.get(), Type::Bool);
	project.setType(intExpr.get(), Type::Int);
	project.setType(identExpr.get(), Type::Item);

	EXPECT_EQ(project.getType(boolExpr.get()).value(), Type::Bool);
	EXPECT_EQ(project.getType(intExpr.get()).value(), Type::Int);
	EXPECT_EQ(project.getType(identExpr.get()).value(), Type::Item);
}

TEST(TypeTableTests, MixedExprAndParamKeys) {
	Project project;
	auto expr = makeExpr(IntLiteral{3});
	Param param("scene", "Scene", nullptr);

	project.setType(expr.get(), Type::Int);
	project.setType(&param, Type::Scene);

	EXPECT_EQ(project.getType(expr.get()).value(), Type::Int);
	EXPECT_EQ(project.getType(&param).value(), Type::Scene);
}

TEST(TypeTableTests, UnknownPointerReturnsNullopt) {
	Project project;
	auto expr1 = makeExpr(BoolLiteral{true});
	auto expr2 = makeExpr(BoolLiteral{false});

	project.setType(expr1.get(), Type::Bool);

	// expr2 was never registered
	EXPECT_FALSE(project.getType(expr2.get()).has_value());
}

TEST(TypeTableTests, PointerStabilityAfterProjectFilesGrow) {
	Project project;

	// Add a file with a define that has a param and body expr.
	File file;
	file.path = "test.rls";
	std::vector<Param> params;
	params.emplace_back("d", std::nullopt, nullptr);
	file.declarations.emplace_back(DefineDecl(
		"foo", std::move(params), makeExpr(Identifier{"d"})
	));
	project.files.push_back(std::move(file));

	// Grab pointers into the AST owned by the project.
	auto& decl = std::get<DefineDecl>(project.files[0].declarations[0]);
	const Param* paramPtr = &decl.params[0];
	const Expr* bodyPtr = decl.body.get();

	project.setType(paramPtr, Type::Distance);
	project.setType(bodyPtr, Type::Distance);

	// Add more files — vector may reallocate File storage, but
	// the Decl/Param/Expr objects are heap-allocated and stable.
	for (int i = 0; i < 100; ++i) {
		File extra;
		extra.path = "extra_" + std::to_string(i) + ".rls";
		extra.declarations.emplace_back(DefineDecl(
			"bar" + std::to_string(i), std::vector<Param>{},
			makeExpr(BoolLiteral{true})
		));
		project.files.push_back(std::move(extra));
	}

	// Original pointers still resolve correctly.
	EXPECT_EQ(project.getType(paramPtr).value(), Type::Distance);
	EXPECT_EQ(project.getType(bodyPtr).value(), Type::Distance);
}