#include <gtest/gtest.h>

#include "soh_solver.h"

TEST(SohSolverTests, PrefixesAstNodeNameInOutput) {
	const rls::AstNode node{"demo_rule"};

	EXPECT_EQ(rls::transpilers::soh_solver::Transpile(node), "soh_solver:demo_rule");
}

TEST(SohSolverTests, HandlesEmptyAstNodeName) {
	const rls::AstNode node{};

	EXPECT_EQ(rls::transpilers::soh_solver::Transpile(node), "soh_solver:");
}