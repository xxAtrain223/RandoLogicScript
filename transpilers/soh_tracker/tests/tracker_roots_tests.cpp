#include <gtest/gtest.h>

#include <string>
#include <tuple>

#include "parser.h"
#include "sema.h"
#include "tracker_roots.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

class SohTrackerRoots : public testing::Test {
protected:
	const std::filesystem::path sourceRoot =
		std::filesystem::temp_directory_path() / "rls_tracker_roots";
	ast::Project project;
	std::vector<std::pair<std::string, std::string>> sources;

	void addSource(const std::string& path, const std::string& text) {
		sources.emplace_back(path, text);
		project.files.push_back(parser::ParseString(text, (sourceRoot / path).string()));
	}

	void analyze() {
		for (const auto& file : project.files) {
			for (const auto& diagnostic : file.diagnostics) {
				ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
			}
		}
		for (const auto& diagnostic : sema::analyze(project)) {
			ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
		}
	}

	using Identity = std::tuple<ast::SectionKind, std::string, std::string, uint32_t>;

	static std::vector<Identity> identities(const std::vector<RootSource>& roots) {
		std::vector<Identity> result;
		for (const auto& root : roots) {
			result.emplace_back(root.kind, root.owner->key.text,
				root.entry->name.text, root.occurrence);
		}
		return result;
	}
};

TEST_F(SohTrackerRoots, CollectsAllEightShopEntriesWithoutGameplayMetadata) {
	std::string source =
		"define unused(): true\n"
		"region RR_KF_KOKIRI_SHOP {\n"
		"    note: \"metadata is not executable\"\n"
		"    locations {\n";
	for (int i = 1; i <= 8; ++i) {
		source += "        RC_KF_SHOP_ITEM_" + std::to_string(i) +
			(i % 2 == 0 ? ": never\n" : ": always\n");
	}
	source += "    }\n}\n";
	addSource("shop.rls", source);
	analyze();
	ASSERT_FALSE(HasFatalFailure());

	const auto result = CollectRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<RootSource>>(result));
	const auto& roots = std::get<std::vector<RootSource>>(result);
	ASSERT_EQ(roots.size(), 8);
	const auto* owner = project.RegionDecls.at("RR_KF_KOKIRI_SHOP");
	for (size_t i = 0; i < roots.size(); ++i) {
		EXPECT_EQ(roots[i].kind, ast::SectionKind::Locations);
		EXPECT_EQ(roots[i].owner, owner);
		EXPECT_EQ(roots[i].entry, &owner->body.sections[0].entries[i]);
		EXPECT_EQ(roots[i].entry->name.text, "RC_KF_SHOP_ITEM_" + std::to_string(i + 1));
		EXPECT_EQ(roots[i].occurrence, 0);
	}
}

TEST_F(SohTrackerRoots, RetainsFiveBossRootsWithSetterAndOriginalExitConditions) {
	addSource("boss.rls",
		"extern define can_kill() -> Bool\n"
		"extern define flag(event: Event) -> Bool\n"
		"region RR_DEKU_TREE_BOSS_EXIT {}\n"
		"region RR_KF_OUTSIDE_DEKU_TREE {}\n"
		"region RR_DEKU_TREE_BOSS_ROOM {\n"
		"    events { LOGIC_DEKU_TREE_CLEAR: can_kill() }\n"
		"    locations {\n"
		"        RC_QUEEN_GOHMA: flag(LOGIC_DEKU_TREE_CLEAR)\n"
		"        RC_DEKU_TREE_QUEEN_GOHMA_HEART: flag(LOGIC_DEKU_TREE_CLEAR)\n"
		"    }\n"
		"    exits {\n"
		"        RR_DEKU_TREE_BOSS_EXIT: always\n"
		"        RR_KF_OUTSIDE_DEKU_TREE: flag(LOGIC_DEKU_TREE_CLEAR)\n"
		"    }\n"
		"}\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());

	const auto result = CollectRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<RootSource>>(result));
	const auto& roots = std::get<std::vector<RootSource>>(result);
	const auto* owner = project.RegionDecls.at("RR_DEKU_TREE_BOSS_ROOM");
	EXPECT_EQ(identities(roots), (std::vector<Identity>{
		{ast::SectionKind::Events, owner->key.text, "LOGIC_DEKU_TREE_CLEAR", 0},
		{ast::SectionKind::Locations, owner->key.text, "RC_QUEEN_GOHMA", 0},
		{ast::SectionKind::Locations, owner->key.text, "RC_DEKU_TREE_QUEEN_GOHMA_HEART", 0},
		{ast::SectionKind::Exits, owner->key.text, "RR_DEKU_TREE_BOSS_EXIT", 0},
		{ast::SectionKind::Exits, owner->key.text, "RR_KF_OUTSIDE_DEKU_TREE", 0}}));
	ASSERT_EQ(roots.size(), 5);
	EXPECT_EQ(roots[0].entry, &owner->body.sections[0].entries[0]);
	EXPECT_EQ(std::get<ast::CallExpr>(roots[0].entry->condition->node).callee.text, "can_kill");
	EXPECT_EQ(std::get<ast::CallExpr>(roots[1].entry->condition->node).callee.text, "flag");
}

TEST_F(SohTrackerRoots, ComposesVariantsAndAllExtensionsInOrderedSourceView) {
	addSource("z_mq.rls",
		"region RR_MQ { locations { RC_BASE: never } }\n");
	addSource("b_vanilla.rls",
		"region RR_VANILLA {\n"
		"    data: [\"ignored\"]\n"
		"    locations { RC_BASE: always }\n"
		"}\n"
		"extern define uses_region(r: Region) -> Bool\n");
	addSource("a_shuffle.rls",
		"extend region RR_MQ {\n"
		"    locations { RC_SHARED: uses_region(here) }\n"
		"    events { LOGIC_SHARED: never }\n"
		"}\n"
		"extend region RR_VANILLA {\n"
		"    events { LOGIC_SHARED: always }\n"
		"    locations { RC_SHARED: always }\n"
		"}\n");
	addSource("c_shuffle.rls",
		"extend region RR_MQ { exits { RR_VANILLA: never } }\n"
		"extend region RR_VANILLA { exits { RR_MQ: always } }\n");
	analyze();
	ASSERT_FALSE(HasFatalFailure());

	const auto result = CollectRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<RootSource>>(result));
	const auto& roots = std::get<std::vector<RootSource>>(result);
	const std::vector<Identity> expected{
		{ast::SectionKind::Locations, "RR_MQ", "RC_SHARED", 0},
		{ast::SectionKind::Events, "RR_MQ", "LOGIC_SHARED", 0},
		{ast::SectionKind::Events, "RR_VANILLA", "LOGIC_SHARED", 0},
		{ast::SectionKind::Locations, "RR_VANILLA", "RC_SHARED", 0},
		{ast::SectionKind::Locations, "RR_VANILLA", "RC_BASE", 0},
		{ast::SectionKind::Exits, "RR_MQ", "RR_VANILLA", 0},
		{ast::SectionKind::Exits, "RR_VANILLA", "RR_MQ", 0},
		{ast::SectionKind::Locations, "RR_MQ", "RC_BASE", 0}};
	EXPECT_EQ(identities(roots), expected);
	ASSERT_EQ(roots.size(), 8);
	const auto& extension = std::get<ast::ExtendRegionDecl>(project.files[2].declarations[0]);
	EXPECT_EQ(roots[0].entry, &extension.sections[0].entries[0]);
	EXPECT_EQ(roots[0].owner, project.RegionDecls.at("RR_MQ"));
	const auto& call = std::get<ast::CallExpr>(roots[0].entry->condition->node);
	const auto* arguments = project.getResolvedCallArgs(&call);
	ASSERT_NE(arguments, nullptr);
	ASSERT_EQ(arguments->size(), 1);
	EXPECT_EQ(std::get<ast::HereRef>((*arguments)[0]->node).resolvedRegion.text, "RR_MQ");
	EXPECT_EQ(roots[0].entry->span.file, (sourceRoot / "a_shuffle.rls").string());

	// Reparse in a different input order; never move an already analyzed AST.
	ast::Project reordered;
	for (const auto& path : {"a_shuffle.rls", "z_mq.rls", "c_shuffle.rls", "b_vanilla.rls"}) {
		for (const auto& [name, text] : sources) {
			if (name == path) {
				reordered.files.push_back(parser::ParseString(text, (sourceRoot / name).string()));
			}
		}
	}
	for (const auto& diagnostic : sema::analyze(reordered)) {
		ASSERT_NE(diagnostic.level, ast::DiagnosticLevel::Error) << diagnostic.message;
	}
	const auto reorderedResult = CollectRoots(reordered, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<RootSource>>(reorderedResult));
	EXPECT_EQ(identities(std::get<std::vector<RootSource>>(reorderedResult)), expected);
	EXPECT_EQ(project.files[0].path, (sourceRoot / "z_mq.rls").string());
}

TEST_F(SohTrackerRoots, ReportsMissingOwnerMetadataRatherThanReturningPartialRoots) {
	addSource("missing.rls", "extend region RR_MISSING { locations { RC_TEST: always } }\n");
	const auto result = CollectRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(result));
	const auto& diagnostics = std::get<std::vector<ast::Diagnostic>>(result);
	ASSERT_EQ(diagnostics.size(), 1);
	EXPECT_EQ(diagnostics[0].level, ast::DiagnosticLevel::Error);
	EXPECT_NE(diagnostics[0].message.find("RR_MISSING"), std::string::npos);
	EXPECT_EQ(diagnostics[0].span.file, (sourceRoot / "missing.rls").string());
}

TEST_F(SohTrackerRoots, PropagatesSourceOrderingErrorsAndAcceptsEmptyProjects) {
	auto result = CollectRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<RootSource>>(result));
	EXPECT_TRUE(std::get<std::vector<RootSource>>(result).empty());
	addSource("same.rls", "");
	addSource("same.rls", "");
	result = CollectRoots(project, sourceRoot);
	ASSERT_TRUE(std::holds_alternative<std::vector<ast::Diagnostic>>(result));
	EXPECT_NE(std::get<std::vector<ast::Diagnostic>>(result)[0].message.find(
		"duplicate normalized path"), std::string::npos);
}

} // namespace
} // namespace rls::transpilers::soh_tracker::ir
