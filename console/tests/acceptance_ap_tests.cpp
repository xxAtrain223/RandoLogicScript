#include "acceptance_helpers.h"

using namespace rls::acceptance_tests;

TEST(AcceptanceAp, ExamplesRlsMatchesGolden) {
	std::vector<std::string> errors;
	const auto project = parseAndAnalyzeProject(repoPath("examples/soh/src"), errors);
	ASSERT_TRUE(errors.empty()) << joinLines(errors);

	TempDirectory outputDir("soh_ap");
	{
		DirectoryWriter writer(outputDir.path());
		rls::transpilers::soh_ap::SohApTranspiler(project).Transpile(writer);
	}

	expectDirectoryMatchesGolden(
		outputDir.path(),
		repoPath("examples/soh/out_soh_ap"),
		R"(.\build\console\Debug\RandoLogicScript.exe -p .\examples\soh\rls.json -t soh_ap)");
}
