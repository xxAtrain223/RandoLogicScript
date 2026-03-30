#include <gtest/gtest.h>

#include "ast.h"

TEST(AstNodeTests, DefaultsToEmptyName) {
	rls::AstNode node;

	EXPECT_TRUE(node.name.empty());
}

TEST(AstNodeTests, StoresAssignedName) {
	rls::AstNode node{"demo_rule"};

	EXPECT_EQ(node.name, "demo_rule");
}