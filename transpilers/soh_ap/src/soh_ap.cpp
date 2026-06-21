#include "soh_ap.h"

namespace rls::transpilers::soh_ap {

SohApTranspiler::SohApTranspiler(const rls::ast::Project& project)
	: ap::ApTranspiler(project) {}

void SohApTranspiler::Transpile(rls::OutputWriter& out) const {
	GenerateRegionsSource(out);
	GenerateEnumsSource(out);
}

std::string SohApTranspiler::ruleContextParam() const {
	return "bundle";
}

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out) {
	SohApTranspiler(project).Transpile(out);
}

} // namespace rls::transpilers::soh_ap
