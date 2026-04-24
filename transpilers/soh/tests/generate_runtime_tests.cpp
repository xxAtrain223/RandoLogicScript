#include "helpers.h"

using namespace rls::transpilers::soh_tests;

TEST(SohRuntime, GenerateRlsMatchHeader) {
	rls::ast::Project project;
	MemoryWriter out;
	rls::transpilers::soh::SohTranspiler(project).GenerateRuntimeHeaders(out);

	const auto& content = out.content("rls_match.h");
	EXPECT_FALSE(content.empty());
	EXPECT_NE(content.find("match"), std::string::npos);
}
