#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "document_store.h"
#include "symbol_support.h"

namespace {

using rls::lsp::DocumentStore;
using rls::lsp::detail::endpoints::support::callsiteFunctionNameAtPosition;
using rls::lsp::detail::endpoints::support::collectFunctionParams;
using rls::lsp::detail::endpoints::support::collectSymbols;
using rls::lsp::detail::endpoints::support::extractWordAtPosition;
using rls::lsp::detail::endpoints::support::findWordRange;
using rls::lsp::detail::endpoints::support::isIdentifierChar;
using rls::lsp::detail::endpoints::support::locationFromSpan;
using rls::lsp::detail::endpoints::support::prefixAtPosition;
using rls::lsp::detail::endpoints::support::startsWithCaseInsensitive;
using rls::lsp::detail::endpoints::support::symbolMatchesQuery;

TEST(SymbolSupportTests, IdentifierCharRecognizesAlphaNumericAndUnderscore) {
    EXPECT_TRUE(isIdentifierChar('a'));
    EXPECT_TRUE(isIdentifierChar('7'));
    EXPECT_TRUE(isIdentifierChar('_'));
    EXPECT_FALSE(isIdentifierChar('-'));
}

TEST(SymbolSupportTests, ExtractWordAtPositionReturnsIdentifierBounds) {
    const std::string text = "define alpha(value: Int): value";

    const auto word = extractWordAtPosition(text, 0, 8);

    ASSERT_TRUE(word.has_value());
    EXPECT_EQ(word->value, "alpha");
    EXPECT_EQ(word->startCharacter, 7);
    EXPECT_EQ(word->endCharacter, 12);
}

TEST(SymbolSupportTests, ExtractWordAtPositionClampsPastLineEnd) {
    const auto word = extractWordAtPosition("alpha", 0, 99);

    ASSERT_TRUE(word.has_value());
    EXPECT_EQ(word->value, "alpha");
    EXPECT_EQ(word->startCharacter, 0);
    EXPECT_EQ(word->endCharacter, 5);
}

TEST(SymbolSupportTests, PrefixAtPositionReturnsCurrentIdentifierPrefix) {
    const std::string text = "define beta(): al";

    EXPECT_EQ(prefixAtPosition(text, 0, 17), "al");
}

TEST(SymbolSupportTests, CallsiteFunctionNameAtPositionFindsOpenCall) {
    const std::string text = "foo(bar, baz)";

    const auto name = callsiteFunctionNameAtPosition(text, 0, 9);

    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "foo");
}

TEST(SymbolSupportTests, CallsiteFunctionNameAtPositionStopsAtClosedCall) {
    const std::string text = "foo(bar); baz";

    EXPECT_FALSE(callsiteFunctionNameAtPosition(text, 0, 12).has_value());
}

TEST(SymbolSupportTests, CollectSymbolsIndexesTopLevelDeclarations) {
    DocumentStore documents;
    documents.open(
        "file:///symbols.rls",
        "rls",
        1,
        "define alpha(value: Int): value\nextern define beta(flag: Bool) -> Bool");

    const auto symbols = collectSymbols(documents);

    ASSERT_EQ(symbols.size(), 2u);
    EXPECT_EQ(symbols[0].name, "alpha");
    EXPECT_EQ(symbols[0].uri, "file:///symbols.rls");
    EXPECT_EQ(symbols[0].kind, 12);
    EXPECT_EQ(symbols[0].detail, "define");
    EXPECT_EQ(symbols[1].name, "beta");
    EXPECT_EQ(symbols[1].kind, 12);
    EXPECT_EQ(symbols[1].detail, "extern define");
}

TEST(SymbolSupportTests, CollectFunctionParamsReturnsParameterNames) {
    DocumentStore documents;
    documents.open(
        "file:///params.rls",
        "rls",
        1,
        "define alpha(a: Int, b: Int): a\nextern define beta(flag: Bool) -> Bool");

    const auto params = collectFunctionParams(documents);

    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params.at("alpha"), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(params.at("beta"), (std::vector<std::string>{"flag"}));
}

TEST(SymbolSupportTests, StartsWithCaseInsensitiveMatchesPrefixes) {
    EXPECT_TRUE(startsWithCaseInsensitive("AlphaBeta", "alp"));
    EXPECT_TRUE(startsWithCaseInsensitive("AlphaBeta", ""));
    EXPECT_FALSE(startsWithCaseInsensitive("Alpha", "beta"));
}

TEST(SymbolSupportTests, SymbolMatchesQueryUsesCaseInsensitiveSubstring) {
    EXPECT_TRUE(symbolMatchesQuery("AlphaBeta", "HAB"));
    EXPECT_TRUE(symbolMatchesQuery("AlphaBeta", ""));
    EXPECT_FALSE(symbolMatchesQuery("AlphaBeta", "gamma"));
}

TEST(SymbolSupportTests, LocationFromSpanPrefersSpanFileAndNormalizesIt) {
    const auto location = locationFromSpan(
        "file:///fallback.rls",
        rls::ast::Span{R"(file:///C:\\work\\symbols.rls)", {2, 3}, {2, 6}});

    EXPECT_EQ(location["uri"], "file:///C:/work/symbols.rls");
    EXPECT_EQ(location["range"]["start"]["line"], 1);
    EXPECT_EQ(location["range"]["start"]["character"], 2);
    EXPECT_EQ(location["range"]["end"]["line"], 1);
    EXPECT_EQ(location["range"]["end"]["character"], 5);
}

TEST(SymbolSupportTests, FindWordRangeConvertsOffsetToLspRange) {
    const std::string text = "aa\nhello world";
    const auto location = findWordRange("file:///words.rls", text, "world", text.find("world"));

    EXPECT_EQ(location["uri"], "file:///words.rls");
    EXPECT_EQ(location["range"]["start"]["line"], 1);
    EXPECT_EQ(location["range"]["start"]["character"], 6);
    EXPECT_EQ(location["range"]["end"]["line"], 1);
    EXPECT_EQ(location["range"]["end"]["character"], 11);
}

} // namespace