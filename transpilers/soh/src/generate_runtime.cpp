#include "soh.h"

#include "rls_match_content.h"

namespace rls::transpilers::soh {

void SohTranspiler::GenerateRuntimeHeaders(rls::OutputWriter& out) const {
	auto& os = out.open("rls_match.h");
	os << detail::kRlsMatchContent;
}

} // namespace rls::transpilers::soh
