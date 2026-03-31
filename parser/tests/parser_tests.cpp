#include <gtest/gtest.h>

#include "parser.h"

TEST(ParserTests, ReturnsEmptyFileForEmptySource) {
	const auto file = rls::parser::Parse("");

	EXPECT_TRUE(file.declarations.empty());
}

TEST(ParserTests, ReturnsEmptyFileForNonEmptySource) {
	const auto file = rls::parser::Parse("region RR_TEST {}");

	EXPECT_TRUE(file.declarations.empty());
}