#include <gtest/gtest.h>

#include "ast.h"
#include "parser.h"
#include "sema.h"

// Internal headers for direct unit testing of individual passes.
#include "collect_declarations.h"
#include "resolve_types.h"
#include "validate_declarations.h"

using namespace rls::ast;
using namespace rls::sema;

// == Helpers ==================================================================

/// Parse RLS source, collect declarations, resolve types, and validate.
static std::pair<Project, std::vector<Diagnostic>> validateFromSource(
	const std::string& source)
{
	Project project;
	const std::string hostItemEnum = source.find("enum Item") == std::string::npos
		? "extern enum Item { RG_* }\n"
		: "";
	const std::string hostSettingEnum = source.find("enum Setting") == std::string::npos
		? "extern enum Setting { RSK_*, RO_* }\n"
		: "";
	const std::string hostRegionEnum = source.find("enum Region") == std::string::npos
		? "extern enum Region { RR_* }\n"
		: "";
	const std::string hostLocationEnum = source.find("enum Location") == std::string::npos
		? "extern enum Location { RC_* }\n"
		: "";
	project.files.push_back(rls::parser::ParseString(
		hostItemEnum + hostSettingEnum + hostRegionEnum + hostLocationEnum
		+ "extern enum Distance { ED_* }\n"
		+ "extern define setting(key: Setting) -> Int\n"
		+ source));
	collectDeclarations(project);
	resolveTypes(project);
	auto diags = validateDeclarations(project);
	return {std::move(project), std::move(diags)};
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

TEST(ValidateDeclarations, DuplicateRegionDataKey) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"First\"\n"
		"    name: \"Second\"\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_EQ(diags[0].message, "duplicate data key 'name' in region 'RR_FOYER'");
	EXPECT_EQ(diags[0].span.start.line, 9u);
	EXPECT_EQ(diags[0].span.start.column, 5u);
}

// == Extend-region target exists ==============================================

TEST(ValidateDeclarations, ExtendRegionTargetExists_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ValidateDeclarations, ExtendRegionTargetMissing) {
	auto [project, diags] = validateFromSource(
		"extend region RR_NONEXISTENT {\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown region 'RR_NONEXISTENT'"), std::string::npos);
}

TEST(ValidateDeclarations, ExtendRegionMultipleMissing) {
	auto [project, diags] = validateFromSource(
		"extend region RR_MISSING_A {\n"
		"    exits { RR_OTHER: always }\n"
		"}\n"
		"extend region RR_MISSING_B {\n"
		"    locations { RC_POT: always }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 2u);
}

TEST(ValidateDeclarations, ExtendRegionSomeValidSomeMissing) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations { RC_POT: always }\n"
		"}\n"
		"extend region RR_NONEXISTENT {\n"
		"    exits { RR_OTHER: always }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("RR_NONEXISTENT"), std::string::npos);
}

TEST(ValidateDeclarations, ExtendRegionMultipleExtendsOnSameValidRegion) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n"
		"region RR_OTHER {\n"
		"    name: \"Other\"\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations { RC_POT: always }\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    events { LOGIC_TEST: always }\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    exits { RR_OTHER: always }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

// == Integration via analyze() ================================================

TEST(ValidateDeclarations, AnalyzeReportsExtendTargetMissing) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extend region RR_GHOST {\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n",
		"ghost.rls"
	));

	auto diags = analyze(project);

	bool found = false;
	for (const auto& d : diags) {
		if (d.message.find("unknown region 'RR_GHOST'") != std::string::npos) {
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found) << "Expected diagnostic about unknown region 'RR_GHOST'";
}

TEST(ValidateDeclarations, AnalyzeExtendTargetExists_Ok) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n",
		"temple.rls"
	));

	auto diags = analyze(project);

	for (const auto& d : diags) {
		EXPECT_EQ(d.message.find("unknown region"), std::string::npos)
			<< "Unexpected: " << d.message;
	}
}

// == Duplicate entries in merged region =======================================

TEST(ValidateDeclarations, NoDuplicateEntries_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT_A: always\n"
		"        RC_POT_B: always\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ValidateDeclarations, DuplicateLocationInBaseRegion) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate location 'RC_POT'"), std::string::npos);
	EXPECT_NE(diags[0].message.find("RR_FOYER"), std::string::npos);
}

TEST(ValidateDeclarations, DuplicateAcrossBaseAndExtend) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate location 'RC_POT'"), std::string::npos);
}

TEST(ValidateDeclarations, DuplicateAcrossTwoExtends) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate location 'RC_POT'"), std::string::npos);
}

TEST(ValidateDeclarations, DuplicateExitInRegion) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_LOBBY: always\n"
		"        RR_LOBBY: always\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate exit 'RR_LOBBY'"), std::string::npos);
}

TEST(ValidateDeclarations, DuplicateEventInRegion) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    events {\n"
		"        LOGIC_FLAG: always\n"
		"        LOGIC_FLAG: always\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate event 'LOGIC_FLAG'"), std::string::npos);
}

TEST(ValidateDeclarations, SameNameInDifferentRegions_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n"
		"region RR_LOBBY {\n"
		"    name: \"Lobby\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ValidateDeclarations, MultipleDuplicatesInOneRegion) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT_A: always\n"
		"        RC_POT_A: always\n"
		"        RC_POT_B: always\n"
		"        RC_POT_B: always\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 2u);
}

// == Entry conditions must be Bool-compatible =================================

TEST(ValidateDeclarations, EntryConditionBool_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"    exits {\n"
		"        RR_LOBBY: has(RG_HOOKSHOT)\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 0u);
}

// == Exit targets have region declarations ===================================

TEST(ValidateDeclarations, ExitTargetMissingRegionWarnsAtExitKey) {
	auto [project, diags] = validateFromSource(
		"region RR_ROOT {\n"
		"    exits {\n"
		"        RR_MISSING: always\n"
		"    }\n"
		"}\n");

	ASSERT_EQ(countWarnings(diags), 1u);
	const auto& diagnostic = diags[0];
	EXPECT_EQ(diagnostic.code, "RLS-V020");
	EXPECT_EQ(diagnostic.level, DiagnosticLevel::Warning);
	EXPECT_EQ(diagnostic.message,
		"exit targets region 'RR_MISSING' without a region declaration");
	const auto& exit = project.RegionDecls.at("RR_ROOT")->body.sections[0].entries[0];
	EXPECT_EQ(diagnostic.span.file, exit.name.span.file);
	EXPECT_EQ(diagnostic.span.start.line, exit.name.span.start.line);
	EXPECT_EQ(diagnostic.span.start.column, exit.name.span.start.column);
	EXPECT_EQ(diagnostic.span.end.line, exit.name.span.end.line);
	EXPECT_EQ(diagnostic.span.end.column, exit.name.span.end.column);
}

TEST(ValidateDeclarations, ExitTargetMissingRegionInExtensionWarns) {
	auto [project, diags] = validateFromSource(
		"region RR_ROOT {}\n"
		"extend region RR_ROOT {\n"
		"    exits { RR_MISSING: always }\n"
		"}\n");

	ASSERT_EQ(countWarnings(diags), 1u);
	EXPECT_EQ(diags[0].code, "RLS-V020");
	EXPECT_NE(diags[0].message.find("RR_MISSING"), std::string::npos);
}

TEST(ValidateDeclarations, EntryConditionInt_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: hearts()\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 0u);
}

TEST(ValidateDeclarations, EntryConditionNonBool) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: RG_HOOKSHOT\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("RC_POT"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Bool"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Enum"), std::string::npos);
}

TEST(ValidateDeclarations, EntryConditionNonBoolInExtend) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"}\n"
		"extend region RR_FOYER {\n"
		"    locations {\n"
		"        RC_POT: RG_HOOKSHOT\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("RC_POT"), std::string::npos);
	EXPECT_NE(diags[0].message.find("Enum"), std::string::npos);
}

TEST(ValidateDeclarations, EntryConditionNonBoolExit) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    exits {\n"
		"        RR_LOBBY: RG_HOOKSHOT\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("exit condition"), std::string::npos);
}

TEST(ValidateDeclarations, EntryConditionNonBoolEvent) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    events {\n"
		"        LOGIC_FLAG: RG_HOOKSHOT\n"
		"    }\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("event condition"), std::string::npos);
}

TEST(ValidateDeclarations, EntryConditionSettingBoolCompatible_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: setting(RSK_SHUFFLE_POTS)\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 0u);
}

TEST(ValidateDeclarations, EntryConditionSettingKeyIsNotBool) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: RSK_SHUFFLE_POTS\n"
		"    }\n"
		"}\n");

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("must be Bool, got Enum"), std::string::npos);
}

TEST(ValidateDeclarations, MultipleNonBoolConditions) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
		"    name: \"Foyer\"\n"
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT_A: RG_HOOKSHOT\n"
		"        RC_POT_B: RG_SWORD\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 2u);
}

// == Unreachable regions ======================================================

TEST(ValidateDeclarations, AllRegionsReachable_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"    exits {\n"
		"        RR_FIELD: always\n"
		"    }\n"
		"}\n"
		"region RR_FIELD {\n"
		"    name: \"Field\"\n"
		"    scene: SCENE_HYRULE_FIELD\n"
		"}\n");
	EXPECT_EQ(countWarnings(diags), 0u);
}

TEST(ValidateDeclarations, UnreachableRegion) {
	auto [project, diags] = validateFromSource(
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n"
		"region RR_ISLAND {\n"
		"    name: \"Island\"\n"
		"    scene: SCENE_UNKNOWN\n"
		"}\n");
	ASSERT_EQ(countWarnings(diags), 1u);
	EXPECT_NE(diags[0].message.find("RR_ISLAND"), std::string::npos);
	EXPECT_NE(diags[0].message.find("not reachable"), std::string::npos);
}

TEST(ValidateDeclarations, ReachableThroughChain) {
	auto [project, diags] = validateFromSource(
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"    exits {\n"
		"        RR_FIELD: always\n"
		"    }\n"
		"}\n"
		"region RR_FIELD {\n"
		"    name: \"Field\"\n"
		"    scene: SCENE_HYRULE_FIELD\n"
		"    exits {\n"
		"        RR_VILLAGE: always\n"
		"    }\n"
		"}\n"
		"region RR_VILLAGE {\n"
		"    name: \"Village\"\n"
		"    scene: SCENE_KAKARIKO\n"
		"}\n");
	EXPECT_EQ(countWarnings(diags), 0u);
}

TEST(ValidateDeclarations, ReachableViaExtendExit) {
	auto [project, diags] = validateFromSource(
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n"
		"extend region RR_ROOT {\n"
		"    exits {\n"
		"        RR_FIELD: always\n"
		"    }\n"
		"}\n"
		"region RR_FIELD {\n"
		"    name: \"Field\"\n"
		"    scene: SCENE_HYRULE_FIELD\n"
		"}\n");
	EXPECT_EQ(countWarnings(diags), 0u);
}

TEST(ValidateDeclarations, MultipleUnreachableRegions) {
	auto [project, diags] = validateFromSource(
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n"
		"region RR_ISLAND_A {\n"
		"    name: \"Island A\"\n"
		"    scene: SCENE_UNKNOWN\n"
		"}\n"
		"region RR_ISLAND_B {\n"
		"    name: \"Island B\"\n"
		"    scene: SCENE_UNKNOWN\n"
		"}\n");
	EXPECT_EQ(countWarnings(diags), 2u);
}

TEST(ValidateDeclarations, NoRootRegion_SkipsCheck) {
	auto [project, diags] = validateFromSource(
		"region RR_FIELD {\n"
		"    name: \"Field\"\n"
		"    scene: SCENE_HYRULE_FIELD\n"
		"}\n");
	// No RR_ROOT → check is skipped, no unreachable warnings.
	EXPECT_EQ(countWarnings(diags), 0u);
}

// == Unused defines ===========================================================

TEST(ValidateDeclarations, DefineUsedInRegion_Ok) {
	auto [project, diags] = validateFromSource(
		"define can_smash():\n"
		"    has(RG_MEGATON_HAMMER) or has(RG_HOOKSHOT)\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"    locations {\n"
		"        RC_POT: can_smash()\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countWarnings(diags), 0u);
}

TEST(ValidateDeclarations, UnusedDefine) {
	auto [project, diags] = validateFromSource(
		"define can_smash():\n"
		"    has(RG_MEGATON_HAMMER)\n");
	bool found = false;
	for (const auto& d : diags) {
		if (d.level == DiagnosticLevel::Info
			&& d.message.find("can_smash") != std::string::npos
			&& d.message.find("never used") != std::string::npos) {
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST(ValidateDeclarations, DefineUsedByAnotherDefine_Ok) {
	auto [project, diags] = validateFromSource(
		"define inner():\n"
		"    has(RG_HOOKSHOT)\n"
		"define outer():\n"
		"    inner()\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"    locations {\n"
		"        RC_POT: outer()\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countWarnings(diags), 0u);
}

TEST(ValidateDeclarations, MultipleUnusedDefines) {
	auto [project, diags] = validateFromSource(
		"define unused_a():\n"
		"    always\n"
		"define unused_b():\n"
		"    never\n");
	size_t unusedInfos = 0;
	for (const auto& d : diags) {
		if (d.level == DiagnosticLevel::Info
			&& d.message.find("never used") != std::string::npos) {
			++unusedInfos;
		}
	}
	EXPECT_EQ(unusedInfos, 2u);
}

TEST(ValidateDeclarations, DefineUsedInExtendRegion_Ok) {
	auto [project, diags] = validateFromSource(
		"define can_smash():\n"
		"    has(RG_MEGATON_HAMMER)\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n"
		"extend region RR_ROOT {\n"
		"    locations {\n"
		"        RC_POT: can_smash()\n"
		"    }\n"
		"}\n");
	EXPECT_EQ(countWarnings(diags), 0u);
}

// == Function signature validation ============================================

TEST(ValidateDeclarations, ExternDefineTypedDefaults_Ok) {
	auto [project, diags] = validateFromSource(
		"extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater = false) -> Bool\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 0u);
}

TEST(ValidateDeclarations, DomainTypedLegacyEnumDefaults_Ok) {
	auto [project, diags] = validateFromSource(
		"extern enum Event { LOGIC_* }\n"
		"extern define values(reg: Region = RR_NONE, event: Event = LOGIC_NONE, "
		"location: Location = RC_UNKNOWN_CHECK) -> Bool\n");

	EXPECT_EQ(countErrors(diags), 0u);
}

TEST(ValidateDeclarations, ExternDefineTypedDefaultMismatch) {
	auto [project, diags] = validateFromSource(
		"extern define can_hit_switch(distance: Distance = false) -> Bool\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("default value for parameter 'distance'"), std::string::npos);
	EXPECT_NE(diags[0].message.find("expected enum 'Distance'"), std::string::npos);
}

TEST(ValidateDeclarations, DefineTypedDefaultMismatch) {
	auto [project, diags] = validateFromSource(
		"define foo(distance: Distance = false):\n"
		"    distance == ED_CLOSE\n");
	ASSERT_EQ(countErrors(diags), 1u);
	bool found = false;
	for (const auto& d : diags) {
		if (d.level != DiagnosticLevel::Error) continue;
		if (d.message.find("default value for parameter 'distance'") != std::string::npos
			&& d.message.find("expected enum 'Distance'") != std::string::npos) {
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST(ValidateDeclarations, DefineEnumDefaultIdentityMismatch) {
	auto [project, diags] = validateFromSource(
		"enum Color { RED }\n"
		"enum Status { ACTIVE }\n"
		"define select(color: Color = Status.ACTIVE): true\n");

	ASSERT_EQ(countErrors(diags), 1u);
	auto error = std::find_if(diags.begin(), diags.end(), [](const Diagnostic& diagnostic) {
		return diagnostic.level == DiagnosticLevel::Error;
	});
	ASSERT_NE(error, diags.end());
	EXPECT_NE(error->message.find("has type enum 'Status'"), std::string::npos);
	EXPECT_NE(error->message.find("expected enum 'Color'"), std::string::npos);
}

TEST(ValidateDeclarations, ExternDefineDuplicateParameterName) {
	auto [project, diags] = validateFromSource(
		"extern define can_hit_switch(distance: Distance, distance: Distance) -> Bool\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate parameter 'distance'"), std::string::npos);
}

TEST(ValidateDeclarations, ExternDefineRequiredAfterOptionalParam) {
	auto [project, diags] = validateFromSource(
		"extern define can_hit_switch(distance: Distance = ED_CLOSE, inWater: Bool) -> Bool\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("cannot follow optional parameters"), std::string::npos);
}

TEST(ValidateDeclarations, ExternDefineMissingReturnTypeManualAst) {
	Project project;
	File file;
	file.path = "externs.rls";
	file.declarations.emplace_back(ExternDefineDecl(
		Name("can_hit_switch"),
		std::vector<Param>{}
	));
	project.files.push_back(std::move(file));

	collectDeclarations(project);
	resolveTypes(project);
	auto diags = validateDeclarations(project);

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("must declare a return type"), std::string::npos);
}

TEST(ValidateDeclarations, ExternDefineUnknownReturnType) {
	auto [project, diags] = validateFromSource(
		"extern define can_hit_switch(distance = ED_CLOSE) -> NotAType\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("unknown return type annotation 'NotAType'"), std::string::npos);
}

TEST(ValidateDeclarations, AnalyzeReportsDuplicateExternDefineDeclaration) {
	Project project;
	project.files.push_back(rls::parser::ParseString(
		"extern define can_hit_switch(distance: Distance = ED_CLOSE) -> Bool\n",
		"externs_a.rls"
	));
	project.files.push_back(rls::parser::ParseString(
		"extern define can_hit_switch(distance: Distance = ED_FAR) -> Bool\n",
		"externs_b.rls"
	));

	auto diags = analyze(project);

	bool found = false;
	for (const auto& d : diags) {
		if (d.level == DiagnosticLevel::Error
			&& d.message.find("duplicate extern define 'can_hit_switch'") != std::string::npos) {
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST(ValidateDeclarations, ExternDefineUntypedParamWithoutDefault) {
	auto [project, diags] = validateFromSource(
		"extern define can_hit_switch(distance) -> Bool\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("must have a type annotation or a default value"), std::string::npos);
}

// == Enum validation =========================================================

TEST(ValidateDeclarations, EnumAutoIncrementAndExplicitValues) {
	auto [project, diags] = validateFromSource(
		"enum Item { RG_A, RG_B = 4, RG_C }\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 0u);
	ASSERT_TRUE(project.EnumInfos.contains("Item"));
	const auto& info = project.EnumInfos.at("Item");
	ASSERT_EQ(info.entries.size(), 3u);

	auto valueAt = [&](size_t i) {
		return std::get<EnumMemberInfo>(info.entries[i]).value;
	};
	ASSERT_TRUE(valueAt(0).has_value());
	ASSERT_TRUE(valueAt(1).has_value());
	ASSERT_TRUE(valueAt(2).has_value());
	EXPECT_EQ(*valueAt(0), 0);
	EXPECT_EQ(*valueAt(1), 4);
	EXPECT_EQ(*valueAt(2), 5);
}

TEST(ValidateDeclarations, EnumDuplicateMemberName) {
	auto [project, diags] = validateFromSource(
		"enum Item { RG_A, RG_A }\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate enum member 'RG_A'"), std::string::npos);
}

TEST(ValidateDeclarations, EnumDuplicateComputedValue) {
	auto [project, diags] = validateFromSource(
		"enum Item { RG_A = 3, RG_B = 3 }\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate enum value 3"), std::string::npos);
}

TEST(ValidateDeclarations, ExternEnumMustHaveAtLeastOneEntry) {
	auto [project, diags] = validateFromSource(
		"extern enum Item {}\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("must declare at least one member or wildcard pattern"), std::string::npos);
}

TEST(ValidateDeclarations, ExternEnumDuplicateExplicitMemberName) {
	auto [project, diags] = validateFromSource(
		"extern enum Item { RG_A, RG_A }\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");

	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("duplicate enum member 'RG_A' in extern enum 'Item'"), std::string::npos);
}

TEST(ValidateDeclarations, ExternEnumWildcardOverlapsExplicitMemberWarning) {
	auto [project, diags] = validateFromSource(
		"extern enum Item { RG_A, RG_* }\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 0u);
	bool foundWarning = false;
	for (const auto& d : diags) {
		if (d.level == DiagnosticLevel::Warning &&
			d.message.find("wildcard 'RG_*' overlaps explicit member 'RG_A'") != std::string::npos) {
			foundWarning = true;
			break;
		}
	}
	EXPECT_TRUE(foundWarning);
}

TEST(ValidateDeclarations, EnumValueNameCollisionAcrossEnumsWarns) {
	auto [project, diags] = validateFromSource(
		"enum Item { SHARED }\n"
		"enum Reward { SHARED }\n"
		"region RR_ROOT {\n"
		"    name: \"Root\"\n"
		"    scene: SCENE_LINKS_HOUSE\n"
		"}\n");

	EXPECT_EQ(countErrors(diags), 0u);
	bool foundWarning = false;
	for (const auto& d : diags) {
		if (d.level == DiagnosticLevel::Warning &&
			d.message.find("enum value 'SHARED' appears in multiple enums") != std::string::npos) {
			foundWarning = true;
			break;
		}
	}
	EXPECT_TRUE(foundWarning);
}
