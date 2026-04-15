#include "generate_runtime.h"

#include "rls_match_content.h"

namespace rls::transpilers::soh {

void GenerateRuntimeHeaders(rls::OutputWriter& out) {
	auto& os = out.open("rls_match.h");
	os << detail::kRlsMatchContent;
}

} // namespace rls::transpilers::soh
