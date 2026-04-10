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
	project.files.push_back(rls::parser::ParseString(source));
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

// == Extend-region target exists ==============================================

TEST(ValidateDeclarations, ExtendRegionTargetExists_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
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
		"    scene: SCENE_SPIRIT_TEMPLE\n"
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

// == Enemy must have 'kill' field =============================================

TEST(ValidateDeclarations, EnemyWithKill_Ok) {
	auto [project, diags] = validateFromSource(
		"enemy RE_WOLFOS {\n"
		"    kill: always\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ValidateDeclarations, EnemyWithAllFields_Ok) {
	auto [project, diags] = validateFromSource(
		"enemy RE_WOLFOS {\n"
		"    kill: always\n"
		"    pass: always\n"
		"    drop: always\n"
		"    avoid: always\n"
		"}\n");
	EXPECT_TRUE(diags.empty());
}

TEST(ValidateDeclarations, EnemyMissingKill) {
	auto [project, diags] = validateFromSource(
		"enemy RE_WOLFOS {\n"
		"    pass: always\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("RE_WOLFOS"), std::string::npos);
	EXPECT_NE(diags[0].message.find("'kill'"), std::string::npos);
}

TEST(ValidateDeclarations, EnemyOnlyPassDropAvoid_MissingKill) {
	auto [project, diags] = validateFromSource(
		"enemy RE_WOLFOS {\n"
		"    pass: always\n"
		"    drop: always\n"
		"    avoid: always\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("'kill'"), std::string::npos);
}

TEST(ValidateDeclarations, MultipleEnemiesMissingKill) {
	auto [project, diags] = validateFromSource(
		"enemy RE_WOLFOS {\n"
		"    pass: always\n"
		"}\n"
		"enemy RE_STALFOS {\n"
		"    drop: always\n"
		"}\n");
	EXPECT_EQ(countErrors(diags), 2u);
}

TEST(ValidateDeclarations, MixedEnemiesOneValid) {
	auto [project, diags] = validateFromSource(
		"enemy RE_WOLFOS {\n"
		"    kill: always\n"
		"}\n"
		"enemy RE_STALFOS {\n"
		"    pass: always\n"
		"}\n");
	ASSERT_EQ(countErrors(diags), 1u);
	EXPECT_NE(diags[0].message.find("RE_STALFOS"), std::string::npos);
}

// == Duplicate entries in merged region =======================================

TEST(ValidateDeclarations, NoDuplicateEntries_Ok) {
	auto [project, diags] = validateFromSource(
		"region RR_FOYER {\n"
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
		"    scene: SCENE_SPIRIT_TEMPLE\n"
		"    locations {\n"
		"        RC_POT: always\n"
		"    }\n"
		"}\n"
		"region RR_LOBBY {\n"
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
