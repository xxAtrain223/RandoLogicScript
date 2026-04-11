#include "helpers.h"
#include "generate_expression.h"

using rls::transpilers::soh::GenerateExpression;
using namespace rls::transpilers::soh_tests;

rls::ast::ExprPtr sourceToExpression(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	const auto defineDecl = project.DefineDecls.find(defineName);
	return defineDecl != project.DefineDecls.end()
		? std::move(const_cast<rls::ast::DefineDecl*>(defineDecl->second)->body)
		: nullptr;
}

TEST(SohExpressions, BoolLiteral) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    true\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true");

	expr = sourceToExpression(
		"define test():\n"
		"    always\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true");

	expr = sourceToExpression(
		"define test():\n"
		"    false\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"false");

	expr = sourceToExpression(
		"define test():\n"
		"    never\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"false");
}

TEST(SohExpressions, IntLiteral) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    42\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"42");

	expr = sourceToExpression(
		"define test():\n"
		"    -7\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"-7");
}

TEST(SohExpressions, Identifier) {
	auto expr = sourceToExpression(
		"define test(item: Item):\n"
		"    item\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"item");
}

TEST(SohExpressions, IsChildIsAdultKeywords) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    is_child\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsChild");

	expr = sourceToExpression(
		"define test():\n"
		"    is_adult\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsAdult");
}

TEST(SohExpressions, AtDayAtNightKeywords) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    at_day\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->AtDay");

	expr = sourceToExpression(
		"define test():\n"
		"    at_night\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->AtNight");
}

TEST(SohExpressions, IsVanillaIsMqKeywords) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    is_vanilla\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsVanilla()");

	expr = sourceToExpression(
		"define test():\n"
		"    is_mq\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"logic->IsMQ()");
}

TEST(SohExpressions, UnaryNot) {
	auto expr = sourceToExpression(
		"define test(value: Bool):\n"
		"    not value\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"!value");
}

TEST(SohExpressions, BinaryAndOr) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    true and false\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true && false");

	expr = sourceToExpression(
		"define test():\n"
		"    true or false\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true || false");
}

TEST(SohExpressions, BinaryComparisons) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 == 2\n",
		"test")),
		"1 == 2");


	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 is 2\n",
		"test")),
		"1 == 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 != 2\n",
		"test")),
		"1 != 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 is not 2\n",
		"test")),
		"1 != 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 < 2\n",
		"test")),
		"1 < 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 <= 2\n",
		"test")),
		"1 <= 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    2 > 1\n",
		"test")),
		"2 > 1");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    2 >= 1\n",
		"test")),
		"2 >= 1");
}

TEST(SohExpressions, BinaryArithmetic) {
	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    1 + 2\n",
		"test")),
		"1 + 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    5 - 3\n",
		"test")),
		"5 - 3");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    4 * 2\n",
		"test")),
		"4 * 2");

	EXPECT_EQ(GenerateExpression(sourceToExpression(
		"define test():\n"
		"    8 / 2\n",
		"test")),
		"8 / 2");
}

TEST(SohExpressions, TernarySimple) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    true ? 1 : 2\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"true ? 1 : 2");
}

TEST(SohExpressions, TernaryWithIdentifier) {
	auto expr = sourceToExpression(
		"define test(item: Item):\n"
		"    item is RG_SILVER_GAUNTLETS ? 1 : 0\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"item == RG_SILVER_GAUNTLETS ? 1 : 0");
}

TEST(SohExpressions, TernaryNestedRightAssociative) {
	auto expr = sourceToExpression(
		"define test():\n"
		"    1 ? 2 ? 3 : 4 : 5\n",
		"test");
	EXPECT_EQ(GenerateExpression(expr),
		"1 ? 2 ? 3 : 4 : 5");
}

