#include <gtest/gtest.h>

#include "ast.h"
#include "parser.h"
#include "sema.h"

// Internal header for direct unit testing of individual passes.
#include "collect_declarations.h"

using namespace rls::ast;
using namespace rls::sema;

// == Helpers ==================================================================

/// Build a minimal File containing a single RegionDecl.
static File makeRegionFile(const std::string& path, const std::string& regionName,
                           const std::string& scene, Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(RegionDecl(
		regionName,
		RegionBody("Test", scene, TimePasses::Auto, {}, {}),
		span
	));
	return f;
}

/// Build a minimal File containing a single DefineDecl.
static File makeDefineFile(const std::string& path, const std::string& name,
                           Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(DefineDecl(
		name, {}, makeExpr(BoolLiteral{true}), span
	));
	return f;
}

/// Build a minimal File containing a single ExternDefineDecl.
static File makeExternDefineFile(const std::string& path, const std::string& name,
                                 Span span = {}) {
	File f;
	f.path = path;
	f.declarations.emplace_back(ExternDefineDecl(name, {}, span));
	return f;
}

/// Build a minimal File containing a single EnemyDecl.
static File makeEnemyFile(const std::string& path, const std::string& name,
                          Span span = {}) {
	File f;
	f.path = path;
	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Kill, std::vector<Param>{},
	                     makeExpr(BoolLiteral{true}));
	f.declarations.emplace_back(EnemyDecl(name, std::move(fields), span));
	return f;
}

/// Build a minimal File containing a single ExtendRegionDecl.
static File makeExtendFile(const std::string& path, const std::string& regionName,
                           SectionKind sectionKind, Span span = {}) {
	File f;
	f.path = path;
	std::vector<Entry> entries;
	entries.emplace_back("TEST_ENTRY", makeExpr(BoolLiteral{true}));
	std::vector<Section> sections;
	sections.emplace_back(sectionKind, std::move(entries));
	f.declarations.emplace_back(ExtendRegionDecl(
		regionName, std::move(sections), span
	));
	return f;
}

/// Count diagnostics of a given level.
static size_t countErrors(const std::vector<Diagnostic>& diags) {
	size_t n = 0;
	for (const auto& d : diags)
		if (d.level == DiagnosticLevel::Error) ++n;
	return n;
}

static size_t countWarnings(const std::vector<Diagnostic>& diags) {
	size_t n = 0;
	for (const auto& d : diags)
		if (d.level == DiagnosticLevel::Warning) ++n;
	return n;
}

// == Empty project ============================================================

TEST(CollectDeclarations, EmptyProject) {
	Project project;
	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_TRUE(project.RegionDecls.empty());
	EXPECT_TRUE(project.ExtendRegionDecls.empty());
	EXPECT_TRUE(project.DefineDecls.empty());
	EXPECT_TRUE(project.ExternDefineDecls.empty());
	EXPECT_TRUE(project.EnemyDecls.empty());
}

TEST(CollectDeclarations, EmptyFile) {
	Project project;
	File f;
	f.path = "empty.rls";
	project.files.push_back(std::move(f));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_TRUE(project.RegionDecls.empty());
}

// == Single declarations ======================================================

TEST(CollectDeclarations, SingleRegion) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_TEST"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.RegionDecls.size(), 1u);
	ASSERT_TRUE(project.RegionDecls.contains("RR_TEST"));
	EXPECT_EQ(project.RegionDecls.at("RR_TEST")->key, "RR_TEST");
}

TEST(CollectDeclarations, SingleDefine) {
	Project project;
	project.files.push_back(makeDefineFile("a.rls", "has_explosives"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.DefineDecls.size(), 1u);
	ASSERT_TRUE(project.DefineDecls.contains("has_explosives"));
	EXPECT_EQ(project.DefineDecls.at("has_explosives")->name, "has_explosives");
}

TEST(CollectDeclarations, SingleExternDefine) {
	Project project;
	project.files.push_back(makeExternDefineFile("a.rls", "can_hit_switch"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.ExternDefineDecls.size(), 1u);
	ASSERT_TRUE(project.ExternDefineDecls.contains("can_hit_switch"));
	EXPECT_EQ(project.ExternDefineDecls.at("can_hit_switch")->name, "can_hit_switch");
}

TEST(CollectDeclarations, SingleEnemy) {
	Project project;
	project.files.push_back(makeEnemyFile("a.rls", "RE_ARMOS"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.EnemyDecls.size(), 1u);
	ASSERT_TRUE(project.EnemyDecls.contains("RE_ARMOS"));
	EXPECT_EQ(project.EnemyDecls.at("RE_ARMOS")->name, "RE_ARMOS");
}

TEST(CollectDeclarations, SingleExtendRegion) {
	Project project;
	project.files.push_back(makeExtendFile("a.rls", "RR_TEST", SectionKind::Locations));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.ExtendRegionDecls.count("RR_TEST"), 1u);
}

// == Multiple declarations across files =======================================

TEST(CollectDeclarations, MultipleRegionsAcrossFiles) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_FOYER", "SCENE_SPIRIT"));
	project.files.push_back(makeRegionFile("b.rls", "RR_STATUE", "SCENE_SPIRIT"));
	project.files.push_back(makeRegionFile("c.rls", "RR_FIELD", "SCENE_FIELD"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 3u);
	EXPECT_TRUE(project.RegionDecls.contains("RR_FOYER"));
	EXPECT_TRUE(project.RegionDecls.contains("RR_STATUE"));
	EXPECT_TRUE(project.RegionDecls.contains("RR_FIELD"));
}

TEST(CollectDeclarations, MixedDeclsInOneFile) {
	Project project;
	File f;
	f.path = "mixed.rls";

	f.declarations.emplace_back(RegionDecl(
		"RR_TEST", RegionBody("Test", "SCENE_TEST", TimePasses::Auto, {}, {})
	));
	f.declarations.emplace_back(DefineDecl(
		"helper", {}, makeExpr(BoolLiteral{true})
	));
	std::vector<EnemyField> fields;
	fields.emplace_back(EnemyFieldKind::Kill, std::vector<Param>{},
	                     makeExpr(BoolLiteral{true}));
	f.declarations.emplace_back(EnemyDecl("RE_FOO", std::move(fields)));

	std::vector<Entry> entries;
	entries.emplace_back("RC_POT", makeExpr(BoolLiteral{true}));
	std::vector<Section> sections;
	sections.emplace_back(SectionKind::Locations, std::move(entries));
	f.declarations.emplace_back(ExtendRegionDecl("RR_TEST", std::move(sections)));

	project.files.push_back(std::move(f));
	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.EnemyDecls.size(), 1u);
	EXPECT_EQ(project.ExtendRegionDecls.count("RR_TEST"), 1u);
}

TEST(CollectDeclarations, MultipleExtendsForSameRegion) {
	Project project;
	project.files.push_back(
		makeExtendFile("pots.rls", "RR_FOYER", SectionKind::Locations));
	project.files.push_back(
		makeExtendFile("crates.rls", "RR_FOYER", SectionKind::Locations));
	project.files.push_back(
		makeExtendFile("grass.rls", "RR_FOYER", SectionKind::Locations));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.ExtendRegionDecls.count("RR_FOYER"), 3u);
}

// == Duplicate declarations ===================================================

TEST(CollectDeclarations, DuplicateRegionError) {
	Project project;
	Span span1{"a.rls", {1, 1}, {3, 1}};
	Span span2{"b.rls", {5, 1}, {7, 1}};
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_A", span1));
	project.files.push_back(makeRegionFile("b.rls", "RR_TEST", "SCENE_B", span2));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate region 'RR_TEST'"), std::string::npos);
	// The first one wins
	ASSERT_TRUE(project.RegionDecls.contains("RR_TEST"));
	EXPECT_EQ(project.RegionDecls.at("RR_TEST")->body.scene.value(), "SCENE_A");
}

TEST(CollectDeclarations, DuplicateDefineError) {
	Project project;
	Span span1{"helpers.rls", {1, 1}, {1, 30}};
	Span span2{"other.rls", {10, 1}, {10, 30}};
	project.files.push_back(makeDefineFile("helpers.rls", "my_func", span1));
	project.files.push_back(makeDefineFile("other.rls", "my_func", span2));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate define 'my_func'"), std::string::npos);
	// The span on the diagnostic should point to the duplicate (second one)
	EXPECT_EQ(diags[0].span.file, "other.rls");
}

TEST(CollectDeclarations, DuplicateEnemyError) {
	Project project;
	Span span1{"enemies.rls", {1, 1}, {5, 1}};
	Span span2{"enemies2.rls", {1, 1}, {5, 1}};
	project.files.push_back(makeEnemyFile("enemies.rls", "RE_ARMOS", span1));
	project.files.push_back(makeEnemyFile("enemies2.rls", "RE_ARMOS", span2));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate enemy 'RE_ARMOS'"), std::string::npos);
}

TEST(CollectDeclarations, DuplicateExternDefineError) {
	Project project;
	Span span1{"externs.rls", {1, 1}, {1, 40}};
	Span span2{"externs2.rls", {3, 1}, {3, 40}};
	project.files.push_back(makeExternDefineFile("externs.rls", "can_hit_switch", span1));
	project.files.push_back(makeExternDefineFile("externs2.rls", "can_hit_switch", span2));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate extern define 'can_hit_switch'"), std::string::npos);
	EXPECT_EQ(diags[0].span.file, "externs2.rls");
}

TEST(CollectDeclarations, DefineExternCollisionError) {
	Project project;
	project.files.push_back(makeDefineFile("helpers.rls", "can_hit_switch"));
	project.files.push_back(makeExternDefineFile("externs.rls", "can_hit_switch"));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate function 'can_hit_switch'"), std::string::npos);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 0u);
}

TEST(CollectDeclarations, ExternDefineCollisionError) {
	Project project;
	project.files.push_back(makeExternDefineFile("externs.rls", "can_hit_switch"));
	project.files.push_back(makeDefineFile("helpers.rls", "can_hit_switch"));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate function 'can_hit_switch'"), std::string::npos);
	EXPECT_EQ(project.DefineDecls.size(), 0u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 1u);
}

TEST(CollectDeclarations, DuplicateRegionInSameFile) {
	Project project;
	File f;
	f.path = "bad.rls";
	Span span1{"bad.rls", {1, 1}, {3, 1}};
	Span span2{"bad.rls", {5, 1}, {7, 1}};
	f.declarations.emplace_back(RegionDecl(
		"RR_DUP", RegionBody("Dup A", "SCENE_A", TimePasses::Auto, {}, {}), span1
	));
	f.declarations.emplace_back(RegionDecl(
		"RR_DUP", RegionBody("Dup B", "SCENE_B", TimePasses::Auto, {}, {}), span2
	));
	project.files.push_back(std::move(f));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	// First wins
	EXPECT_EQ(project.RegionDecls.at("RR_DUP")->body.scene.value(), "SCENE_A");
}

TEST(CollectDeclarations, MultipleDuplicateErrors) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_A", "SCENE_A"));
	project.files.push_back(makeRegionFile("b.rls", "RR_A", "SCENE_A"));
	project.files.push_back(makeDefineFile("c.rls", "helper"));
	project.files.push_back(makeDefineFile("d.rls", "helper"));
	project.files.push_back(makeEnemyFile("e.rls", "RE_X"));
	project.files.push_back(makeEnemyFile("f.rls", "RE_X"));

	auto diags = collectDeclarations(project);

	EXPECT_EQ(countErrors(diags), 3u);
}

// == Different decl types can share names =====================================

TEST(CollectDeclarations, SameNameDifferentDeclTypes) {
	// A region, define, and enemy can all be called "SAME_NAME" — they live
	// in separate namespaces.
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "SAME_NAME", "SCENE_A"));
	project.files.push_back(makeDefineFile("b.rls", "SAME_NAME"));
	project.files.push_back(makeEnemyFile("c.rls", "SAME_NAME"));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.EnemyDecls.size(), 1u);
}

// == Idempotency ==============================================================

TEST(CollectDeclarations, IdempotentOnRerun) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_TEST"));
	project.files.push_back(makeDefineFile("b.rls", "helper"));
	project.files.push_back(makeExternDefineFile("c.rls", "has"));

	auto diags1 = collectDeclarations(project);
	EXPECT_TRUE(diags1.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 1u);

	// Run again — should produce the same result, not accumulate.
	auto diags2 = collectDeclarations(project);
	EXPECT_TRUE(diags2.empty());
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.ExternDefineDecls.size(), 1u);
}

// == Pointer stability ========================================================

TEST(CollectDeclarations, PointersRefToOriginalDecl) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_TEST", "SCENE_TEST"));
	project.files.push_back(makeDefineFile("b.rls", "helper"));
	project.files.push_back(makeExternDefineFile("c.rls", "has"));
	project.files.push_back(makeEnemyFile("c.rls", "RE_FOO"));

	collectDeclarations(project);

	// Pointers should point into the File's declaration vector.
	const auto* regionPtr = project.RegionDecls.at("RR_TEST");
	const auto& fileDecl = std::get<RegionDecl>(project.files[0].declarations[0]);
	EXPECT_EQ(regionPtr, &fileDecl);

	const auto* definePtr = project.DefineDecls.at("helper");
	const auto& fileDefine = std::get<DefineDecl>(project.files[1].declarations[0]);
	EXPECT_EQ(definePtr, &fileDefine);

	const auto* externDefinePtr = project.ExternDefineDecls.at("has");
	const auto& fileExternDefine = std::get<ExternDefineDecl>(project.files[2].declarations[0]);
	EXPECT_EQ(externDefinePtr, &fileExternDefine);

	const auto* enemyPtr = project.EnemyDecls.at("RE_FOO");
	const auto& fileEnemy = std::get<EnemyDecl>(project.files[3].declarations[0]);
	EXPECT_EQ(enemyPtr, &fileEnemy);
}

// == Integration with parser ==================================================

TEST(CollectDeclarations, ParsedRegion) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"region RR_SPIRIT_TEMPLE_FOYER {\n"
		"    name: \"Spirit Temple Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_SPIRIT_TEMPLE_ENTRYWAY: always\n"
		"    }\n"
		"}\n",
		"spirit_temple.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.RegionDecls.size(), 1u);
	const auto* r = project.RegionDecls.at("RR_SPIRIT_TEMPLE_FOYER");
	EXPECT_EQ(r->key, "RR_SPIRIT_TEMPLE_FOYER");
	EXPECT_EQ(r->body.scene.value(), "SCENE_SPIRIT_TEMPLE");
	ASSERT_EQ(r->body.sections.size(), 1u);
	EXPECT_EQ(r->body.sections[0].kind, SectionKind::Exits);
}

TEST(CollectDeclarations, ParsedDefine) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"define has_explosives():\n"
		"    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n",
		"helpers.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_TRUE(project.DefineDecls.contains("has_explosives"));
}

TEST(CollectDeclarations, ParsedExternDefine) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater = false) -> Bool\n",
		"externs.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.ExternDefineDecls.size(), 1u);
	EXPECT_TRUE(project.ExternDefineDecls.contains("can_hit_switch"));
}

TEST(CollectDeclarations, ParsedEnemy) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"enemy RE_ARMOS {\n"
		"    kill: blast_or_smash() or can_use(RG_MASTER_SWORD)\n"
		"}\n",
		"enemies.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.EnemyDecls.size(), 1u);
	EXPECT_TRUE(project.EnemyDecls.contains("RE_ARMOS"));
}

TEST(CollectDeclarations, ParsedExtendRegion) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extend region RR_SPIRIT_TEMPLE_FOYER {\n"
		"    locations {\n"
		"        RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()\n"
		"    }\n"
		"}\n",
		"pots.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	ASSERT_EQ(project.ExtendRegionDecls.count("RR_SPIRIT_TEMPLE_FOYER"), 1u);
}

TEST(CollectDeclarations, ParsedMultiFileProject) {
	Project project;

	project.files.push_back(rls::parser::ParseString(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_ENTRYWAY: always\n"
		"    }\n"
		"}\n"
		"region RR_STATUE {\n"
		"    name: \"Statue\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n",
		"spirit_temple.rls"
	));

	project.files.push_back(rls::parser::ParseString(
		"define has_explosives():\n"
		"    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n"
		"define blast_or_smash():\n"
		"    has_explosives() or can_use(RG_MEGATON_HAMMER)\n",
		"helpers.rls"
	));

	project.files.push_back(rls::parser::ParseString(
		"enemy RE_ARMOS {\n"
		"    kill: blast_or_smash()\n"
		"}\n"
		"enemy RE_STALFOS {\n"
		"    kill: can_use(RG_MASTER_SWORD)\n"
		"}\n",
		"enemies.rls"
	));

	project.files.push_back(rls::parser::ParseString(
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT_1: can_break_pots()\n"
		"        RC_POT_2: can_break_pots()\n"
		"    }\n"
		"}\n",
		"pots.rls"
	));

	auto diags = collectDeclarations(project);

	EXPECT_TRUE(diags.empty());
	EXPECT_EQ(project.RegionDecls.size(), 2u);
	EXPECT_EQ(project.DefineDecls.size(), 2u);
	EXPECT_EQ(project.EnemyDecls.size(), 2u);
	EXPECT_EQ(project.ExtendRegionDecls.count("RR_FOYER"), 1u);

	EXPECT_TRUE(project.RegionDecls.contains("RR_FOYER"));
	EXPECT_TRUE(project.RegionDecls.contains("RR_STATUE"));
	EXPECT_TRUE(project.DefineDecls.contains("has_explosives"));
	EXPECT_TRUE(project.DefineDecls.contains("blast_or_smash"));
	EXPECT_TRUE(project.EnemyDecls.contains("RE_ARMOS"));
	EXPECT_TRUE(project.EnemyDecls.contains("RE_STALFOS"));
}

TEST(CollectDeclarations, ParsedDuplicateRegionAcrossFiles) {
	Project project;

	project.files.push_back(rls::parser::ParseString(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n",
		"a.rls"
	));

	project.files.push_back(rls::parser::ParseString(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_FOREST_TEMPLE\n"
		"}\n",
		"b.rls"
	));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate region 'RR_FOYER'"), std::string::npos);
	// First declaration wins
	EXPECT_EQ(project.RegionDecls.at("RR_FOYER")->body.scene.value(),
	          "SCENE_SPIRIT_TEMPLE");
}

// == Diagnostic span quality ==================================================

TEST(CollectDeclarations, DiagnosticSpanPointsToDuplicate) {
	Project project;
	Span firstSpan{"first.rls", {1, 1}, {3, 1}};
	Span dupSpan{"dup.rls", {10, 5}, {12, 1}};
	project.files.push_back(makeRegionFile("first.rls", "RR_X", "SCENE_A", firstSpan));
	project.files.push_back(makeRegionFile("dup.rls", "RR_X", "SCENE_B", dupSpan));

	auto diags = collectDeclarations(project);

	ASSERT_EQ(diags.size(), 1u);
	// The diagnostic's span should be the *duplicate* (second declaration)
	EXPECT_EQ(diags[0].span.file, "dup.rls");
	EXPECT_EQ(diags[0].span.start.line, 10u);
	EXPECT_EQ(diags[0].span.start.column, 5u);
	// The message should reference the *first* declaration's location
	EXPECT_NE(diags[0].message.find("first.rls"), std::string::npos);
	EXPECT_NE(diags[0].message.find("1"), std::string::npos);
}

// == analyze() entry point ====================================================

TEST(Analyze, PopulatesDeclMaps) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extern define has(item: Item) -> Bool\n"
		"extern define can_use(item: Item) -> Bool\n"
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_ENTRYWAY: always\n"
		"    }\n"
		"}\n"
		"define has_explosives():\n"
		"    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)\n"
		"enemy RE_ARMOS {\n"
		"    kill: can_use(RG_BOMB_BAG)\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: has(RG_POWER_BRACELET)\n"
		"    }\n"
		"}\n",
		"all.rls"
	));

	auto diags = analyze(project);

	// Filter out validation warnings (e.g. unused defines) — only check for errors.
	size_t errors = 0;
	for (const auto& d : diags)
		if (d.level == DiagnosticLevel::Error) ++errors;
	EXPECT_EQ(errors, 0u);
	EXPECT_EQ(project.RegionDecls.size(), 1u);
	EXPECT_EQ(project.DefineDecls.size(), 1u);
	EXPECT_EQ(project.EnemyDecls.size(), 1u);
	EXPECT_EQ(project.ExtendRegionDecls.count("RR_FOYER"), 1u);
}

TEST(Analyze, ReturnsDiagnosticsFromAllPasses) {
	Project project;
	project.files.push_back(makeRegionFile("a.rls", "RR_DUP", "SCENE_A"));
	project.files.push_back(makeRegionFile("b.rls", "RR_DUP", "SCENE_B"));

	auto diags = analyze(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate region 'RR_DUP'"), std::string::npos);
}
