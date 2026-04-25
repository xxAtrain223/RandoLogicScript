#include "acceptance_helpers.h"

using namespace rls::acceptance_tests;

TEST(AcceptanceSoh, ExamplesRlsMatchesGolden) {
	std::vector<std::string> errors;
	const auto project = parseAndAnalyzeProject(repoPath("examples/rls"), errors);
	ASSERT_TRUE(errors.empty()) << joinLines(errors);

	TempDirectory outputDir("soh");
	{
		DirectoryWriter writer(outputDir.path());
		rls::transpilers::soh::SohTranspiler(project).Transpile(writer);
	}

	expectDirectoryMatchesGolden(
		outputDir.path(),
		repoPath("examples/soh"),
		R"(.\build\console\RandoLogicScript.exe -t soh -o .\examples\soh .\examples\rls)");
}
