#pragma once

#include "ast.h"

namespace rls::transpilers::soh_tracker {

class SohTrackerTranspiler {
public:
	explicit SohTrackerTranspiler(const rls::ast::Project& project);

private:
	const rls::ast::Project& project;
};

} // namespace rls::transpilers::soh_tracker
