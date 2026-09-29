// Enum tests: which classes reach the generated enums_gen.py, and how enum values render
// in expressions. Two families of enum reach the file:
//   - extern enums whose values this project declares (Region/Check/Logic), materialized
//     from the region walk into the Regions/Locations/Events StrEnums;
//   - normal RLS enums (`enum WaterLevel { ... }`), whose members live in the AST and are
//     generated directly as IntEnum classes.
// Extern enums declared only as glob patterns (`extern enum Item { RG_* }`) have no members
// RLS can enumerate, so the hand-written world supplies those classes and none is emitted.
// Setting-value rendering that asserts on OptionFilter output lives in option_filter_tests.cpp.
#include "helpers.h"

using namespace rls::transpilers::soh_ap_tests;

// Transpile `source` and hand back the generated enums file.
static std::string generateEnums(const std::string& source) {
	auto project = resolveFromSource(source);
	MemoryWriter writer;
	rls::transpilers::soh_ap::SohApTranspiler(project).Transpile(writer);
	return writer.content("enums_gen.py");
}

// Transpile several named sources and hand back the generated enums file.
static std::string generateEnumsFromFiles(const std::vector<SourceFile>& files) {
	auto project = resolveRawFiles(files);
	MemoryWriter writer;
	rls::transpilers::soh_ap::SohApTranspiler(project).Transpile(writer);
	return writer.content("enums_gen.py");
}

// Dotted enum access (`Item.RG_HOOKSHOT`) renders exactly like the bare identifier form:
// both route through renderEnumValue with the same enum name, so choosing the disambiguated
// spelling in RLS never changes the generated Python.
TEST(SohApEnums, DottedEnumAccessMatchesBareIdentifier) {
	const std::string dotted = GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(Item.RG_HOOKSHOT)\n",
		"test"));
	const std::string bare = GenerateExpression(sourceToExpression(
		"define test():\n"
		"    has(RG_HOOKSHOT)\n",
		"test"));

	EXPECT_EQ(dotted, "has_item(bundle, Items.RG_HOOKSHOT)");
	EXPECT_EQ(dotted, bare);
}

// A normal RLS enum is generated as an IntEnum carrying the values sema assigned.
TEST(SohApEnums, DeclaredEnumGeneratesIntEnumClass) {
	const std::string out = generateEnums(
		"enum Potion { Red, Green, Blue }\n");

	EXPECT_NE(out.find(
		"class Potion(IntEnum):\n"
		"    Red = 0\n"
		"    Green = 1\n"
		"    Blue = 2\n"), std::string::npos) << out;
}

// Explicit member values are preserved rather than renumbered.
TEST(SohApEnums, DeclaredEnumKeepsExplicitValues) {
	const std::string out = generateEnums(
		"enum Priority { Low = 10, High = 20 }\n");

	EXPECT_NE(out.find(
		"class Priority(IntEnum):\n"
		"    Low = 10\n"
		"    High = 20\n"), std::string::npos) << out;
}

// An extern enum is a glob pattern over host names, so RLS knows no members for it and must
// not emit a class -- doing so would shadow the world's real one with an empty stub.
TEST(SohApEnums, ExternEnumEmitsNoClass) {
	const std::string out = generateEnums(
		"extern enum Potion { RP_* }\n");

	EXPECT_EQ(out.find("class Potion"), std::string::npos) << out;
}

// The region walk still materializes Regions/Events, and a region's display name becomes its
// Regions value.
TEST(SohApEnums, RegionWalkMaterializesStrEnums) {
	const std::string out = generateEnums(
		"region RR_TEST_ROOM {\n"
		"    name: \"Test Room\"\n"
		"\n"
		"    locations {\n"
		"        RC_TEST_CHEST: always\n"
		"    }\n"
		"\n"
		"    events {\n"
		"        LOGIC_TEST_FLAG: always\n"
		"    }\n"
		"}\n");

	EXPECT_NE(out.find("class Regions(StrEnum):\n    RR_TEST_ROOM = \"Test Room\"\n"),
		std::string::npos) << out;
	// Locations are the host's: their values are the names the Ship client resolves checks by,
	// which a title-cased identifier cannot reproduce ("Kf ..." where Ship has "KF ...").
	EXPECT_EQ(out.find("class Locations"), std::string::npos) << out;
	EXPECT_EQ(out.find("RC_TEST_CHEST"), std::string::npos) << out;
	EXPECT_NE(out.find("    LOGIC_TEST_FLAG = auto()"), std::string::npos) << out;
	// Each region/event pair also gets an EventLocations member.
	EXPECT_NE(out.find("    RR_TEST_ROOM_LOGIC_TEST_FLAG = auto()"), std::string::npos) << out;
}
// REGION_SCENE maps every region to its `scene:` token. The host needs it to answer
// is_mq()/is_vanilla(), where a rule knows only the region it hangs off.
TEST(SohApEnums, RegionSceneMapIsGenerated) {
	const std::string out = generateEnums(
		"region RR_TEST_ROOM {\n"
		"    name: \"Test Room\"\n"
		"    scene: SCENE_TEST_DUNGEON\n"
		"}\n");

	EXPECT_NE(out.find("REGION_SCENE: dict[Regions, str] = {\n"
			"    Regions.RR_TEST_ROOM: \"SCENE_TEST_DUNGEON\",\n"),
		std::string::npos) << out;
}

// A region with no `scene:` still gets a row, with an empty value rather than a missing key,
// so the host can subscript REGION_SCENE without guarding every lookup.
TEST(SohApEnums, RegionWithoutSceneGetsEmptyValue) {
	const std::string out = generateEnums(
		"region RR_TEST_ROOM {\n"
		"    name: \"Test Room\"\n"
		"}\n");

	EXPECT_NE(out.find("    Regions.RR_TEST_ROOM: \"\","), std::string::npos) << out;
}

// RR_NONE is Ship's sentinel region: host.rls uses it as the default for spirit_shared's
// optional region parameters, so generated code references it even though no region declares it.
TEST(SohApEnums, RrNoneSentinelIsAlwaysEmitted) {
	const std::string out = generateEnums(
		"region RR_TEST_ROOM {\n"
		"    name: \"Test Room\"\n"
		"    scene: SCENE_TEST\n"
		"}\n");

	EXPECT_NE(out.find("    RR_TEST_ROOM = \"Test Room\"\n    RR_NONE = \"None\"\n"),
		std::string::npos) << out;
	// It is not a region, so it gets a scene row with no scene rather than being absent --
	// REGION_SCENE stays subscriptable for every Regions member.
	EXPECT_NE(out.find("    Regions.RR_NONE: \"\","), std::string::npos) << out;
}

// REGION_QUEST comes from the declaring file, not the region name: SoH splits the two dungeon
// layouts across <dungeon>_mq.rls and <dungeon>_vanilla.rls and keeps the regions both quests
// share in <dungeon>.rls. That third "" state is the one a naming rule cannot see.
TEST(SohApEnums, RegionQuestComesFromTheDeclaringFile) {
	const std::vector<SourceFile> files = {
		{"host.rls", "extern enum Scene { SCENE_* }\nextern enum Region { RR_* }\n"},
		{"deku_tree.rls",
			"region RR_SHARED_ENTRYWAY {\n    name: \"Shared Entryway\"\n    scene: SCENE_DEKU_TREE\n}\n"},
		{"deku_tree_mq.rls",
			"region RR_MQ_ROOM {\n    name: \"MQ Room\"\n    scene: SCENE_DEKU_TREE\n}\n"},
		{"deku_tree_vanilla.rls",
			"region RR_VANILLA_ROOM {\n    name: \"Vanilla Room\"\n    scene: SCENE_DEKU_TREE\n}\n"},
	};
	const std::string out = generateEnumsFromFiles(files);

	EXPECT_NE(out.find("    Regions.RR_SHARED_ENTRYWAY: \"\","), std::string::npos) << out;
	EXPECT_NE(out.find("    Regions.RR_MQ_ROOM: \"mq\","), std::string::npos) << out;
	EXPECT_NE(out.find("    Regions.RR_VANILLA_ROOM: \"vanilla\","), std::string::npos) << out;
}
