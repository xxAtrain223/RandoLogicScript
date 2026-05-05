#include "acceptance_helpers.h"

using namespace rls::acceptance_tests;

TEST(AcceptanceAp, ExamplesRlsMatchesGolden) {
	std::vector<std::string> errors;
	const auto project = parseAndAnalyzeProject(repoPath("examples/rls"), errors);
	ASSERT_TRUE(errors.empty()) << joinLines(errors);

	TempDirectory outputDir("ap");
	{
		DirectoryWriter writer(outputDir.path());
		rls::transpilers::soh_ap::SohApTranspiler(project).Transpile(writer);
	}

	expectDirectoryMatchesGolden(
		outputDir.path(),
		repoPath("examples/soh_ap"),
		R"(.\build\console\RandoLogicScript.exe -t soh_ap -o .\examples\soh_ap .\examples\rls)");
}
