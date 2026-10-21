#include <gtest/gtest.h>

#include "parser.h"
#include "sema.h"
#include "soh_tracker.h"

TEST(SohTrackerFoundation, AcceptsConstAnalyzedProject) {
	rls::ast::Project project;
	project.files.push_back(rls::parser::ParseString(
		"define available():\n"
		"    true\n",
		"tracker_tests.rls"));

	for (const auto& diagnostic : project.files.front().diagnostics) {
		ASSERT_NE(diagnostic.level, rls::ast::DiagnosticLevel::Error)
			<< diagnostic.message;
	}
	for (const auto& diagnostic : rls::sema::analyze(project)) {
		ASSERT_NE(diagnostic.level, rls::ast::DiagnosticLevel::Error)
			<< diagnostic.message;
	}

	const auto& analyzedProject = project;
	[[maybe_unused]] const rls::transpilers::soh_tracker::SohTrackerTranspiler
		transpiler(analyzedProject);
}
