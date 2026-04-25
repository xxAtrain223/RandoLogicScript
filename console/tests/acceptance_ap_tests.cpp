#include "acceptance_helpers.h"

using namespace rls::acceptance_tests;

TEST(AcceptanceAp, ExamplesRlsMatchesGolden) {
	std::vector<std::string> errors;
	const auto project = parseAndAnalyzeProject(repoPath("examples/rls"), errors);
	ASSERT_TRUE(errors.empty()) << joinLines(errors);

	TempDirectory outputDir("ap");
	{
		DirectoryWriter writer(outputDir.path());
		rls::transpilers::ap::Transpile(project, writer);
	}

	expectDirectoryMatchesGolden(
		outputDir.path(),
		repoPath("examples/ap"),
		R"(.\build\console\RandoLogicScript.exe -t ap -o .\examples\ap .\examples\rls)");
}
