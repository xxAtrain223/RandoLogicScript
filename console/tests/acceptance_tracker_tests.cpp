#include "acceptance_helpers.h"
#include "soh_tracker.h"

#include <set>

using namespace rls::acceptance_tests;

TEST(AcceptanceTracker, ExamplesRlsMatchesGolden) {
	std::vector<std::string> errors;
	const auto project = parseAndAnalyzeProject(repoPath("examples/soh/src"), errors);
	ASSERT_TRUE(errors.empty()) << joinLines(errors);

	TempDirectory outputDir("soh_tracker");
	{
		DirectoryWriter writer(outputDir.path());
		const auto diagnostics = rls::transpilers::soh_tracker::Transpile(
			project, writer, repoPath("examples/soh"));
		ASSERT_TRUE(collectSemaErrors(diagnostics).empty()) << joinLines(collectSemaErrors(diagnostics));
	}

	expectDirectoryMatchesGolden(
		outputDir.path(),
		repoPath("examples/soh/out_soh_tracker"),
		R"(.\build\console\Debug\RandoLogicScript.exe -p .\examples\soh\rls.json -t soh_tracker)");

	std::istringstream source(readTextFile(outputDir.path() / "logic_tracker.gen.cpp"));
	std::set<std::string> constructors;
	for (std::string line; std::getline(source, line);) {
		if (!line.starts_with("\tcase ")) continue;
		const auto start = line.find(": return ");
		ASSERT_NE(start, std::string::npos);
		const auto constructor = line.substr(start + 9);
		EXPECT_TRUE(constructors.insert(constructor).second) << "Duplicate factory: " << constructor;
	}
	EXPECT_FALSE(constructors.empty());
}
