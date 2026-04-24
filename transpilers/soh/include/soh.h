#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh {

class SohTranspiler {
public:
	explicit SohTranspiler(const rls::ast::Project& project);

	void Transpile(rls::OutputWriter& out) const;

	void GenerateRuntimeHeaders(rls::OutputWriter& out) const;
	void GenerateFunctionDefinitionsHeader(rls::OutputWriter& out) const;
	void GenerateFunctionDefinitionsSource(rls::OutputWriter& out) const;
	void GenerateEnemiesHeader(rls::OutputWriter& out) const;
	void GenerateEnemiesSource(rls::OutputWriter& out) const;
	void GenerateRegionsHeader(rls::OutputWriter& out) const;
	void GenerateRegionsSource(rls::OutputWriter& out) const;

private:
	const rls::ast::Project& project;
};

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh
