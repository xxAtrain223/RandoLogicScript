#include "helpers.h"
#include "generate_runtime.h"

using namespace rls::transpilers::soh_tests;

TEST(SohRuntime, GenerateRlsMatchHeader) {
	MemoryWriter out;
	rls::transpilers::soh::GenerateRuntimeHeaders(out);

	const auto& content = out.content("rls_match.h");
	EXPECT_FALSE(content.empty());
	EXPECT_NE(content.find("match"), std::string::npos);
}
