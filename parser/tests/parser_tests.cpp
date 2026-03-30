#include <gtest/gtest.h>

#include "parser.h"

TEST(ParserTests, ReturnsEmptyMarkerForEmptySource) {
	const auto ast = rls::parser::Parse("");

	EXPECT_EQ(ast.name, "empty");
}

TEST(ParserTests, PreservesNonEmptySourceAsNodeName) {
	const auto ast = rls::parser::Parse("demo_rule");

	EXPECT_EQ(ast.name, "demo_rule");
}