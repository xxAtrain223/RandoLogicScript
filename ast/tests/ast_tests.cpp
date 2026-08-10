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
	auto expr = makeExpr(Identifier{Name("RG_HOOKSHOT")});
	ASSERT_TRUE(std::holds_alternative<Identifier>(expr->node));
	EXPECT_EQ(std::get<Identifier>(expr->node).name, "RG_HOOKSHOT");
}

TEST(ExprTests, MemberExpr) {
	auto expr = makeExpr(MemberExpr(Name("Color"), Name("RED")));
	ASSERT_TRUE(std::holds_alternative<MemberExpr>(expr->node));
	const auto& member = std::get<MemberExpr>(expr->node);
	EXPECT_EQ(member.object, "Color");
	EXPECT_EQ(member.member, "RED");
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
		makeExpr(BoolLiteral{true}),
		makeExpr(Identifier{Name("RG_HOOKSHOT")})
	));
	ASSERT_TRUE(std::holds_alternative<BinaryExpr>(expr->node));
	const auto& bin = std::get<BinaryExpr>(expr->node);
	EXPECT_EQ(bin.op, BinaryOp::And);
	EXPECT_TRUE(std::holds_alternative<BoolLiteral>(bin.left->node));
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
		makeExpr(CallExpr(Name("fire_timer"), {})),
		makeExpr(IntLiteral{48})
	));
	EXPECT_EQ(std::get<BinaryExpr>(expr->node).op, BinaryOp::GtEq);
}

TEST(ExprTests, Ternary) {
	// condition ? ED_BOMB_THROW : ED_BOOMERANG
	auto expr = makeExpr(TernaryExpr(
		makeExpr(BoolLiteral{true}),
		makeExpr(Identifier{Name("ED_BOMB_THROW")}),
		makeExpr(Identifier{Name("ED_BOOMERANG")})
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
	args.emplace_back(std::nullopt, makeExpr(Identifier{Name("SCENE_SPIRIT_TEMPLE")}));
	args.emplace_back(std::nullopt, makeExpr(IntLiteral{3}));

	auto expr = makeExpr(CallExpr(Name("keys"), std::move(args)));
	ASSERT_TRUE(std::holds_alternative<CallExpr>(expr->node));
	const auto& call = std::get<CallExpr>(expr->node);
	EXPECT_EQ(call.callee, "keys");
	ASSERT_EQ(call.args.size(), 2u);
	EXPECT_FALSE(call.args[0].name.has_value());
	EXPECT_FALSE(call.args[1].name.has_value());
}

TEST(ExprTests, CallWithNamedArgs) {
	// can_kill(RE_STALFOS, quantity: 2)
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(Identifier{Name("RE_STALFOS")}));
	args.emplace_back(std::make_optional<Name>("quantity"), makeExpr(IntLiteral{2}));

	auto expr = makeExpr(CallExpr(Name("can_kill"), std::move(args)));
	const auto& call = std::get<CallExpr>(expr->node);
	ASSERT_EQ(call.args.size(), 2u);
	EXPECT_FALSE(call.args[0].name.has_value());
	ASSERT_TRUE(call.args[1].name.has_value());
	EXPECT_EQ(call.args[1].name.value(), "quantity");
}

TEST(ExprTests, CallWithNoArgs) {
	auto expr = makeExpr(CallExpr(Name("has_explosives"), {}));
	const auto& call = std::get<CallExpr>(expr->node);
	EXPECT_EQ(call.callee, "has_explosives");
	EXPECT_TRUE(call.args.empty());
}

TEST(ExprTests, AnyAgeHostCall) {
	// any_age(can_kill(RE_ARMOS))
	std::vector<Arg> args;
	args.emplace_back(std::nullopt, makeExpr(CallExpr(Name("can_kill"), {})));
	auto expr = makeExpr(CallExpr(Name("any_age"), std::move(args)));
	ASSERT_TRUE(std::holds_alternative<CallExpr>(expr->node));
	const auto& call = std::get<CallExpr>(expr->node);
	EXPECT_EQ(call.callee, "any_age");
	ASSERT_EQ(call.args.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<CallExpr>(call.args[0].value->node));
}

TEST(ExprTests, HereRef) {
	auto expr = makeExpr(HereRef{});
	ASSERT_TRUE(std::holds_alternative<HereRef>(expr->node));
	// resolvedRegion is empty at construction — sema fills it in.
	EXPECT_EQ(std::get<HereRef>(expr->node).resolvedRegion.text, "");
}

TEST(ExprTests, MatchExpr) {
	// match distance { ED_CLOSE: expr or, ED_FAR: expr }
	std::vector<MatchArm> arms;
	std::vector<ExprPtr> closePatterns;
	closePatterns.push_back(makeExpr(Identifier{Name("ED_CLOSE")}));
	std::vector<ExprPtr> farPatterns;
	farPatterns.push_back(makeExpr(Identifier{Name("ED_FAR")}));
	arms.emplace_back(std::move(closePatterns), false, makeExpr(BoolLiteral{true}), true);
	arms.emplace_back(std::move(farPatterns), false, makeExpr(BoolLiteral{false}), false);

	auto expr = makeExpr(MatchExpr(makeExpr(Identifier{Name("distance")}), std::move(arms)));
	ASSERT_TRUE(std::holds_alternative<MatchExpr>(expr->node));
	const auto& m = std::get<MatchExpr>(expr->node);
	ASSERT_TRUE(std::holds_alternative<Identifier>(m.discriminant->node));
	EXPECT_EQ(std::get<Identifier>(m.discriminant->node).name, "distance");
	ASSERT_EQ(m.arms.size(), 2u);
	EXPECT_FALSE(m.arms[0].isDefault);
	EXPECT_FALSE(m.arms[1].isDefault);
	EXPECT_TRUE(m.arms[0].fallthrough);
	EXPECT_FALSE(m.arms[1].fallthrough);
}

TEST(ExprTests, MatchArmMultiplePatterns) {
	// ED_CLOSE or ED_SHORT_JUMPSLASH: body
	std::vector<ExprPtr> patterns;
	patterns.push_back(makeExpr(Identifier{Name("ED_CLOSE")}));
	patterns.push_back(makeExpr(Identifier{Name("ED_SHORT_JUMPSLASH")}));
	MatchArm arm(std::move(patterns), false, makeExpr(BoolLiteral{true}), false);
	EXPECT_EQ(arm.patterns.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<Identifier>(arm.patterns[0]->node));
	ASSERT_TRUE(std::holds_alternative<Identifier>(arm.patterns[1]->node));
	EXPECT_EQ(std::get<Identifier>(arm.patterns[0]->node).name, "ED_CLOSE");
	EXPECT_EQ(std::get<Identifier>(arm.patterns[1]->node).name, "ED_SHORT_JUMPSLASH");
	EXPECT_FALSE(arm.isDefault);
}

TEST(ExprTests, MatchArmDefault) {
	MatchArm arm({}, true, makeExpr(BoolLiteral{true}), false);
	EXPECT_TRUE(arm.isDefault);
	EXPECT_TRUE(arm.patterns.empty());
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

// == Source text =============================================================

TEST(SourceTextTests, PreservesCrlfAndConvertsUtf8Offsets) {
	const auto source = SourceText::FromUtf8("one\r\ntwo");
	ASSERT_TRUE(source);
	ASSERT_EQ(source->lineStarts().size(), 2u);
	EXPECT_EQ(source->lineStarts()[0], 0u);
	EXPECT_EQ(source->lineStarts()[1], 5u);
	EXPECT_EQ(source->byteOffsetFromUtf8Position({1, 4}), 3u);
	EXPECT_EQ(source->byteOffsetFromUtf8Position({2, 1}), 5u);

	const auto position = source->utf8PositionAtByteOffset(7);
	ASSERT_TRUE(position);
	EXPECT_EQ(position->line, 2u);
	EXPECT_EQ(position->column, 3u);
}

TEST(SourceTextTests, ConvertsUtf16PositionsForMultibyteCharacters) {
	const auto source = SourceText::FromUtf8("a\xF0\x9F\x98\x80" "b");
	ASSERT_TRUE(source);
	const auto afterEmoji = source->utf16PositionAtByteOffset(5);
	ASSERT_TRUE(afterEmoji);
	EXPECT_EQ(afterEmoji->line, 1u);
	EXPECT_EQ(afterEmoji->column, 4u);
	EXPECT_EQ(source->byteOffsetFromUtf16Position({1, 4}), 5u);
	EXPECT_FALSE(source->byteOffsetFromUtf16Position({1, 3}));
}

TEST(SourceTextTests, AppliesImmutableRangedAndFullDocumentEdits) {
	const auto source = SourceText::FromUtf8("hello world");
	ASSERT_TRUE(source);
	const auto edited = source->replace({{1, 7}, {1, 12}}, "RLS");
	ASSERT_TRUE(edited);
	EXPECT_EQ(source->content(), "hello world");
	EXPECT_EQ(edited->content(), "hello RLS");

	const auto replaced = edited->replaceAll("new document");
	ASSERT_TRUE(replaced);
	EXPECT_EQ(replaced->content(), "new document");
}

TEST(SourceTextTests, RejectsInvalidUtf8AndFindsLexicalTokenRange) {
	EXPECT_FALSE(SourceText::FromUtf8("\xC3\x28"));
	const auto source = SourceText::FromUtf8("call unfinished_name");
	ASSERT_TRUE(source);
	const auto range = source->incompleteTokenRangeAt({1, 14});
	ASSERT_TRUE(range);
	EXPECT_EQ(range->start.column, 6u);
	EXPECT_EQ(range->end.column, 21u);
}

// == Nested expressions =======================================================

TEST(ExprTests, NestedBinaryExpressions) {
	// (a and b) or c
	auto expr = makeExpr(BinaryExpr(
		BinaryOp::Or,
		makeExpr(BinaryExpr(
			BinaryOp::And,
			makeExpr(Identifier{Name("a")}),
			makeExpr(Identifier{Name("b")})
		)),
		makeExpr(Identifier{Name("c")})
	));

	const auto& outerOr = std::get<BinaryExpr>(expr->node);
	EXPECT_EQ(outerOr.op, BinaryOp::Or);
	const auto& innerAnd = std::get<BinaryExpr>(outerOr.left->node);
	EXPECT_EQ(innerAnd.op, BinaryOp::And);
	EXPECT_EQ(std::get<Identifier>(innerAnd.left->node).name, "a");
}

// == Top-level declarations ===================================================

TEST(DeclTests, RegionDecl) {
	std::vector<Entry> exits;
	exits.emplace_back(
		Name("RR_SPIRIT_TEMPLE_ENTRYWAY"),
		makeExpr(BoolLiteral{true})
	);

	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Exits, std::move(exits));
	std::vector<RegionDataEntry> data;
	data.emplace_back(Name("name"), makeExpr(StringLiteral{"Spirit Temple Foyer"}));
	data.emplace_back(Name("scene"), makeExpr(Identifier{Name("SCENE_SPIRIT_TEMPLE")}));

	RegionDecl region(
		Name("RR_SPIRIT_TEMPLE_FOYER"),
		RegionBody(
			std::move(data),
			std::move(sections)
		)
	);

	EXPECT_EQ(region.key, "RR_SPIRIT_TEMPLE_FOYER");
	ASSERT_EQ(region.body.data.size(), 2u);
	ASSERT_NE(region.body.findData("name"), nullptr);
	EXPECT_EQ(std::get<StringLiteral>(region.body.findData("name")->value->node).value, "Spirit Temple Foyer");
	ASSERT_NE(region.body.findData("scene"), nullptr);
	EXPECT_EQ(std::get<Identifier>(region.body.findData("scene")->value->node).name, "SCENE_SPIRIT_TEMPLE");
	ASSERT_EQ(region.body.sections.size(), 1u);
	EXPECT_EQ(region.body.sections[0].kind, SectionKind::Exits);
	ASSERT_EQ(region.body.sections[0].entries.size(), 1u);
	EXPECT_EQ(region.body.sections[0].entries[0].name, "RR_SPIRIT_TEMPLE_ENTRYWAY");
}

TEST(DeclTests, RegionDataPreservesLists) {
	std::vector<ExprPtr> areas;
	areas.push_back(makeExpr(Identifier{Name("RA_CASTLE_GROUNDS")}));
	std::vector<RegionDataEntry> data;
	data.emplace_back(Name("areas"), makeExpr(ListExpr(std::move(areas))));

	RegionDecl region(
		Name("RR_HC_GARDEN"),
		RegionBody(
			std::move(data),
			{}
		)
	);

	const auto* areasEntry = region.body.findData("areas");
	ASSERT_NE(areasEntry, nullptr);
	const auto& list = std::get<ListExpr>(areasEntry->value->node);
	ASSERT_EQ(list.elements.size(), 1u);
	EXPECT_EQ(std::get<Identifier>(list.elements[0]->node).name, "RA_CASTLE_GROUNDS");
}

TEST(DeclTests, ExtendRegionDecl) {
	std::vector<Entry> locations;
	locations.emplace_back(
		Name("RC_SPIRIT_TEMPLE_LOBBY_POT_1"),
		makeExpr(CallExpr(Name("can_break_pots"), {}))
	);

	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Locations, std::move(locations));

	ExtendRegionDecl extend(
		Name("RR_SPIRIT_TEMPLE_FOYER"),
		std::move(sections)
	);

	EXPECT_EQ(extend.name, "RR_SPIRIT_TEMPLE_FOYER");
	ASSERT_EQ(extend.sections.size(), 1u);
	EXPECT_EQ(extend.sections[0].kind, SectionKind::Locations);
}

TEST(DeclTests, DefineDecl) {
	// define spirit_explosive_key_logic(): <body>
	DefineDecl define(
		Name("spirit_explosive_key_logic"),
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
	params.emplace_back(Name("distance"), std::nullopt, makeExpr(Identifier{Name("ED_CLOSE")}));
	params.emplace_back(Name("inWater"), std::nullopt, makeExpr(BoolLiteral{false}));

	DefineDecl define(
		Name("can_hit_switch"),
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
	params.emplace_back(Name("d"), std::make_optional<TypeRef>(Name("Distance")), nullptr);

	DefineDecl define(
		Name("foo"),
		std::move(params),
		makeExpr(BoolLiteral{true})
	);

	ASSERT_TRUE(define.params[0].type.has_value());
	EXPECT_EQ(define.params[0].type.value(), "Distance");
	EXPECT_EQ(define.params[0].defaultValue, nullptr);
}

TEST(DeclTests, ExternDefineDecl) {
	// extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater = false) -> Bool
	std::vector<Param> params;
	params.emplace_back(Name("distance"), std::make_optional<TypeRef>(Name("Distance")), makeExpr(Identifier{Name("ED_CLOSE")}));
	params.emplace_back(Name("inWater"), std::nullopt, makeExpr(BoolLiteral{false}));

	ExternDefineDecl ext(
		Name("can_hit_switch"),
		std::move(params),
		std::make_optional<TypeRef>(Name("Bool"))
	);

	EXPECT_EQ(ext.name, "can_hit_switch");
	ASSERT_TRUE(ext.returnType.has_value());
	EXPECT_EQ(*ext.returnType, "Bool");
	ASSERT_EQ(ext.params.size(), 2u);
	ASSERT_TRUE(ext.params[0].type.has_value());
	EXPECT_EQ(*ext.params[0].type, "Distance");
	ASSERT_NE(ext.params[0].defaultValue, nullptr);
	EXPECT_TRUE(std::holds_alternative<Identifier>(ext.params[0].defaultValue->node));
	ASSERT_NE(ext.params[1].defaultValue, nullptr);
	EXPECT_TRUE(std::holds_alternative<BoolLiteral>(ext.params[1].defaultValue->node));
}

TEST(DeclTests, EnumDecl) {
	std::vector<EnumMemberDecl> members;
	members.emplace_back(Name("RED"));
	members.emplace_back(Name("GREEN"), 3);

	EnumDecl decl(Name("Color"), std::move(members));

	EXPECT_EQ(decl.name, "Color");
	ASSERT_EQ(decl.members.size(), 2u);
	EXPECT_EQ(decl.members[0].name, "RED");
	EXPECT_FALSE(decl.members[0].explicitValue.has_value());
	EXPECT_EQ(decl.members[1].name, "GREEN");
	ASSERT_TRUE(decl.members[1].explicitValue.has_value());
	EXPECT_EQ(*decl.members[1].explicitValue, 3);
}

TEST(DeclTests, ExternEnumDecl) {
	std::vector<ExternEnumEntryDecl> entries;
	entries.emplace_back(EnumMemberDecl(Name("RG_HOOKSHOT")));
	entries.emplace_back(EnumPatternDecl("RG_*"));

	ExternEnumDecl decl(Name("Item"), std::move(entries));

	EXPECT_EQ(decl.name, "Item");
	ASSERT_EQ(decl.entries.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<EnumMemberDecl>(decl.entries[0]));
	ASSERT_TRUE(std::holds_alternative<EnumPatternDecl>(decl.entries[1]));
	EXPECT_EQ(std::get<EnumMemberDecl>(decl.entries[0]).name, "RG_HOOKSHOT");
	EXPECT_EQ(std::get<EnumPatternDecl>(decl.entries[1]).pattern, "RG_*");
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
	std::vector<RegionDataEntry> regionData;
	regionData.emplace_back(Name("name"), makeExpr(StringLiteral{"Test Region"}));
	regionData.emplace_back(Name("scene"), makeExpr(Identifier{Name("SCENE_TEST")}));
	file.declarations.emplace_back(RegionDecl(
		Name("RR_TEST_REGION"),
		RegionBody(std::move(regionData), {})
	));

	// Add a define
	file.declarations.emplace_back(DefineDecl(
		Name("test_helper"),
		std::vector<Param>{},
		makeExpr(BoolLiteral{true})
	));

	// Add an extern define
	std::vector<Param> externParams;
	externParams.emplace_back(Name("item"), std::make_optional<TypeRef>(Name("Item")), nullptr);
	file.declarations.emplace_back(ExternDefineDecl(
		Name("can_use"),
		std::move(externParams),
		std::make_optional<TypeRef>(Name("Bool"))
	));

	ASSERT_EQ(file.declarations.size(), 3u);
	EXPECT_TRUE(std::holds_alternative<RegionDecl>(file.declarations[0]));
	EXPECT_TRUE(std::holds_alternative<DefineDecl>(file.declarations[1]));
	EXPECT_TRUE(std::holds_alternative<ExternDefineDecl>(file.declarations[2]));
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
	std::vector<RegionDataEntry> regionData;
	regionData.emplace_back(Name("name"), makeExpr(StringLiteral{"Spirit Temple Foyer"}));
	regionData.emplace_back(Name("scene"), makeExpr(Identifier{Name("SCENE_SPIRIT_TEMPLE")}));
	file1.declarations.emplace_back(RegionDecl(
		Name("RR_SPIRIT_TEMPLE_FOYER"),
		RegionBody(std::move(regionData), {})
	));

	// File 2: one define + one extern define
	File file2;
	file2.path = "enemies.rls";
	file2.declarations.emplace_back(DefineDecl(
		Name("helper"), {}, makeExpr(BoolLiteral{true})
	));
	file2.declarations.emplace_back(ExternDefineDecl(
		Name("host_helper"),
		std::vector<Param>{},
		std::make_optional<TypeRef>(Name("Bool"))
	));

	project.files.push_back(std::move(file1));
	project.files.push_back(std::move(file2));

	ASSERT_EQ(project.files.size(), 2u);
	EXPECT_EQ(project.files[0].path, "spirit_temple.rls");
	EXPECT_EQ(project.files[1].path, "enemies.rls");
}

TEST(ProjectTests, ExternDefineMapStoresMetadata) {
	Project project;

	File file;
	file.path = "externs.rls";

	std::vector<Param> params;
	params.emplace_back(Name("distance"), std::make_optional<TypeRef>(Name("Distance")), makeExpr(Identifier{Name("ED_CLOSE")}));
	params.emplace_back(Name("inWater"), std::nullopt, makeExpr(BoolLiteral{false}));

	Span declSpan{"externs.rls", {2, 1}, {2, 64}};
	file.declarations.emplace_back(ExternDefineDecl(Name("can_hit_switch"), std::move(params), std::make_optional<TypeRef>(Name("Bool")), declSpan));
	project.files.push_back(std::move(file));

	const auto& ext = std::get<ExternDefineDecl>(project.files[0].declarations[0]);
	project.ExternDefineDecls.emplace(ext.name.text, &ext);

	ASSERT_TRUE(project.ExternDefineDecls.contains("can_hit_switch"));
	const auto* mapped = project.ExternDefineDecls.at("can_hit_switch");
	ASSERT_NE(mapped, nullptr);
	ASSERT_TRUE(mapped->returnType.has_value());
	EXPECT_EQ(*mapped->returnType, "Bool");
	EXPECT_EQ(mapped->span.file, "externs.rls");
	EXPECT_EQ(mapped->span.start.line, 2u);
	ASSERT_EQ(mapped->params.size(), 2u);
	ASSERT_TRUE(mapped->params[0].type.has_value());
	EXPECT_EQ(*mapped->params[0].type, "Distance");
	ASSERT_NE(mapped->params[0].defaultValue, nullptr);
	EXPECT_TRUE(std::holds_alternative<Identifier>(mapped->params[0].defaultValue->node));
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
	Param param(Name("distance"), std::nullopt, makeExpr(Identifier{Name("ED_CLOSE")}));
	Project project;
	project.setType(&param, Type::Enum);
	project.setEnumType(&param, "Distance");

	auto result = project.getType(&param);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), Type::Enum);
}

TEST(TypeTableTests, OverwriteType) {
	Project project;
	auto expr = makeExpr(Identifier{Name("x")});
	project.setType(expr.get(), Type::Error);
	project.setType(expr.get(), Type::Int);

	EXPECT_EQ(project.getType(expr.get()).value(), Type::Int);
}

TEST(TypeTableTests, DistinctExprsHaveDistinctTypes) {
	Project project;
	auto boolExpr = makeExpr(BoolLiteral{true});
	auto intExpr = makeExpr(IntLiteral{42});
	auto identExpr = makeExpr(Identifier{Name("RG_HOOKSHOT")});

	project.setType(boolExpr.get(), Type::Bool);
	project.setType(intExpr.get(), Type::Int);
	project.setType(identExpr.get(), Type::Enum);
	project.setEnumType(identExpr.get(), "Item");

	EXPECT_EQ(project.getType(boolExpr.get()).value(), Type::Bool);
	EXPECT_EQ(project.getType(intExpr.get()).value(), Type::Int);
	EXPECT_EQ(project.getType(identExpr.get()).value(), Type::Enum);
}

TEST(TypeTableTests, MixedExprAndParamKeys) {
	Project project;
	auto expr = makeExpr(IntLiteral{3});
	Param param(Name("scene"), std::make_optional<TypeRef>(Name("Scene")), nullptr);

	project.setType(expr.get(), Type::Int);
	project.setType(&param, Type::Enum);
	project.setEnumType(&param, "Scene");

	EXPECT_EQ(project.getType(expr.get()).value(), Type::Int);
	EXPECT_EQ(project.getType(&param).value(), Type::Enum);
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
	params.emplace_back(Name("d"), std::nullopt, nullptr);
	file.declarations.emplace_back(DefineDecl(
		Name("foo"), std::move(params), makeExpr(Identifier{Name("d")})
	));
	project.files.push_back(std::move(file));

	// Grab pointers into the AST owned by the project.
	auto& decl = std::get<DefineDecl>(project.files[0].declarations[0]);
	const Param* paramPtr = &decl.params[0];
	const Expr* bodyPtr = decl.body.get();

	project.setType(paramPtr, Type::Enum);
	project.setEnumType(paramPtr, "Distance");
	project.setType(bodyPtr, Type::Enum);
	project.setEnumType(bodyPtr, "Distance");

	// Add more files — vector may reallocate File storage, but
	// the Decl/Param/Expr objects are heap-allocated and stable.
	for (int i = 0; i < 100; ++i) {
		File extra;
		extra.path = "extra_" + std::to_string(i) + ".rls";
		extra.declarations.emplace_back(DefineDecl(
			Name("bar" + std::to_string(i)), std::vector<Param>{},
			makeExpr(BoolLiteral{true})
		));
		project.files.push_back(std::move(extra));
	}

	// Original pointers still resolve correctly.
	EXPECT_EQ(project.getType(paramPtr).value(), Type::Enum);
	EXPECT_EQ(project.getType(bodyPtr).value(), Type::Enum);
}

// == Enum type side table ====================================================

TEST(EnumTypeTableTests, EmptyByDefault) {
	Project project;
	auto expr = makeExpr(Identifier{Name("RG_HOOKSHOT")});
	EXPECT_FALSE(project.getEnumType(expr.get()).has_value());
}

TEST(EnumTypeTableTests, SetAndGetExprEnumType) {
	Project project;
	auto expr = makeExpr(Identifier{Name("RG_HOOKSHOT")});
	project.setType(expr.get(), Type::Enum);
	project.setEnumType(expr.get(), "Item");

	auto enumType = project.getEnumType(expr.get());
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Item");
}

TEST(EnumTypeTableTests, SetAndGetParamEnumType) {
	Project project;
	Param param(Name("x"), std::nullopt, nullptr);
	project.setType(&param, Type::Enum);
	project.setEnumType(&param, "Distance");

	auto enumType = project.getEnumType(&param);
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "Distance");
}

TEST(EnumTypeTableTests, OverwriteEnumType) {
	Project project;
	auto expr = makeExpr(Identifier{Name("x")});
	project.setEnumType(expr.get(), "Item");
	project.setEnumType(expr.get(), "CustomItem");

	auto enumType = project.getEnumType(expr.get());
	ASSERT_TRUE(enumType.has_value());
	EXPECT_EQ(*enumType, "CustomItem");
}

// == Enum metadata registry ==================================================

TEST(EnumRegistryTests, RegisterAndLookupEnum) {
	Project project;
	EnumInfo info(
		Name("CustomEnum"),
		EnumKind::Normal,
		Type::Int,
		{
			EnumMemberInfo{Name("A"), 0, {}},
			EnumMemberInfo{Name("B"), 1, {}},
		}
	);

	project.registerEnum(std::move(info));

	const auto* resolved = project.getEnumInfo("CustomEnum");
	ASSERT_NE(resolved, nullptr);
	EXPECT_EQ(resolved->kind, EnumKind::Normal);
	EXPECT_EQ(resolved->underlyingType, Type::Int);
	ASSERT_EQ(resolved->entries.size(), 2u);
	ASSERT_TRUE(isEnumMemberEntry(resolved->entries[0]));
	ASSERT_TRUE(isEnumMemberEntry(resolved->entries[1]));

	const auto& memberA = std::get<EnumMemberInfo>(resolved->entries[0]);
	const auto& memberB = std::get<EnumMemberInfo>(resolved->entries[1]);

	EXPECT_EQ(memberA.name, "A");
	ASSERT_TRUE(memberA.value.has_value());
	EXPECT_EQ(*memberA.value, 0);
	EXPECT_EQ(memberB.name, "B");
	ASSERT_TRUE(memberB.value.has_value());
	EXPECT_EQ(*memberB.value, 1);
}

TEST(EnumRegistryTests, RegisterExternEnumMemberFromPattern) {
	Project project;
	EnumInfo info(
		Name("Item"),
		EnumKind::Extern,
		Type::Int,
		{
			EnumPatternInfo{"RG_*", {}},
		}
	);

	project.registerEnum(std::move(info));

	const auto* resolved = project.getEnumInfo("Item");
	ASSERT_NE(resolved, nullptr);
	EXPECT_EQ(resolved->kind, EnumKind::Extern);
	ASSERT_EQ(resolved->entries.size(), 1u);
	ASSERT_TRUE(isEnumPatternEntry(resolved->entries[0]));

	const auto& pattern = std::get<EnumPatternInfo>(resolved->entries[0]);
	EXPECT_EQ(pattern.pattern, "RG_*");
}

TEST(EnumRegistryTests, MissingEnumReturnsNull) {
	Project project;
	EXPECT_EQ(project.getEnumInfo("DoesNotExist"), nullptr);
}