#include "acceptance_helpers.h"

using namespace rls::acceptance_tests;

TEST(AcceptanceAp, ExamplesRlsMatchesGolden) {
	std::vector<std::string> errors;
	const auto project = parseAndAnalyzeProject(repoPath("examples/soh/src"), errors);
	ASSERT_TRUE(errors.empty()) << joinLines(errors);

	TempDirectory outputDir("ap");
	{
		DirectoryWriter writer(outputDir.path());
		rls::transpilers::ap::Transpile(project, writer);
	}

	expectDirectoryMatchesGolden(
		outputDir.path(),
		repoPath("examples/soh/out_ap"),
		R"(.\build\console\RandoLogicScript.exe -p .\examples\soh\rls.json -t ap)");
}
