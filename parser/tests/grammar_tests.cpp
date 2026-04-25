#include <gtest/gtest.h>

#include "grammar.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/analyze.hpp>

using namespace rls::parser::grammar;

// == Analyzer =================================================================

TEST(Grammar, AnalyzeDetectsNoIssues) {
	EXPECT_EQ(0u, tao::pegtl::analyze<rls_file>());
}

// == Helper ===================================================================

/// Returns true if the rule matches the ENTIRE input string.
template <typename Rule>
bool matches(const std::string& input) {
	tao::pegtl::memory_input in(input, "test");
	try {
		return tao::pegtl::parse<tao::pegtl::must<Rule, tao::pegtl::eof>>(in);
	} catch (const tao::pegtl::parse_error&) {
		return false;
	}
}

// == Whitespace & comments ====================================================

TEST(LexWhitespace, EmptyStringMatchesSkipper) {
	EXPECT_TRUE(matches<_>(""));
}

TEST(LexWhitespace, SpacesAndTabs) {
	EXPECT_TRUE(matches<_>("   \t\t  "));
}

TEST(LexWhitespace, Newlines) {
	EXPECT_TRUE(matches<_>("\n\r\n\n"));
}

TEST(LexWhitespace, MixedWhitespace) {
	EXPECT_TRUE(matches<_>("  \t\n  \r\n  "));
}

TEST(LexComments, LineComment) {
	EXPECT_TRUE(matches<line_comment>("# this is a comment\n"));
}

TEST(LexComments, LineCommentAtEof) {
	EXPECT_TRUE(matches<line_comment>("# comment at end of file"));
}

TEST(LexComments, CommentInWhitespace) {
	EXPECT_TRUE(matches<_>("  # comment\n  "));
}

TEST(LexComments, MultipleComments) {
	EXPECT_TRUE(matches<_>("# first\n# second\n"));
}

TEST(LexComments, CommentWithSymbols) {
	EXPECT_TRUE(matches<line_comment>("# has(RG_HOOKSHOT) and can_use(RG_BOW)\n"));
}

// == Identifiers ==============================================================

TEST(LexIdentifier, SimpleUpper) {
	EXPECT_TRUE(matches<ident>("RG_HOOKSHOT"));
}

TEST(LexIdentifier, SimpleLower) {
	EXPECT_TRUE(matches<ident>("distance"));
}

TEST(LexIdentifier, MixedCase) {
	EXPECT_TRUE(matches<ident>("wallOrFloor"));
}

TEST(LexIdentifier, SingleChar) {
	EXPECT_TRUE(matches<ident>("x"));
}

TEST(LexIdentifier, SingleUnderscore) {
	EXPECT_TRUE(matches<ident>("_"));
}

TEST(LexIdentifier, UnderscorePrefix) {
	EXPECT_TRUE(matches<ident>("_private"));
}

TEST(LexIdentifier, WithDigits) {
	EXPECT_TRUE(matches<ident>("RC_SPIRIT_TEMPLE_LOBBY_POT_1"));
}

TEST(LexIdentifier, StartsWithDigitFails) {
	EXPECT_FALSE(matches<ident>("1bad"));
}

TEST(LexIdentifier, EmptyFails) {
	EXPECT_FALSE(matches<ident>(""));
}

// Keywords must NOT match as identifiers
TEST(LexIdentifier, KeywordRegionIsNotIdentifier) {
	EXPECT_FALSE(matches<ident>("region"));
}

TEST(LexIdentifier, KeywordExternIsNotIdentifier) {
	EXPECT_FALSE(matches<ident>("extern"));
}

TEST(LexIdentifier, KeywordTrueIsNotIdentifier) {
	EXPECT_FALSE(matches<ident>("true"));
}

TEST(LexIdentifier, KeywordAndIsNotIdentifier) {
	EXPECT_FALSE(matches<ident>("and"));
}

TEST(LexIdentifier, KeywordIsChildIsNotIdentifier) {
	EXPECT_FALSE(matches<ident>("is_child"));
}

TEST(LexIdentifier, KeywordAlwaysIsNotIdentifier) {
	EXPECT_FALSE(matches<ident>("always"));
}

TEST(LexIdentifier, KeywordMatchIsNotIdentifier) {
	EXPECT_FALSE(matches<ident>("match"));
}

// Identifier that starts with a keyword prefix but is longer — SHOULD match
TEST(LexIdentifier, KeywordPrefixExtended) {
	EXPECT_TRUE(matches<ident>("regions"));
}

TEST(LexIdentifier, KeywordPrefixTrue) {
	EXPECT_TRUE(matches<ident>("trueValue"));
}

TEST(LexIdentifier, KeywordPrefixAlways) {
	EXPECT_TRUE(matches<ident>("always_something"));
}

TEST(LexIdentifier, KeywordPrefixIs) {
	EXPECT_TRUE(matches<ident>("is_something_else"));
}

TEST(LexIdentifier, KeywordPrefixMatch) {
	EXPECT_TRUE(matches<ident>("match_result"));
}

TEST(LexIdentifier, KeywordPrefixNot) {
	EXPECT_TRUE(matches<ident>("nothing"));
}

// == Integer literals =========================================================

TEST(LexInteger, Zero) {
	EXPECT_TRUE(matches<integer>("0"));
}

TEST(LexInteger, SingleDigit) {
	EXPECT_TRUE(matches<integer>("3"));
}

TEST(LexInteger, MultiDigit) {
	EXPECT_TRUE(matches<integer>("48"));
}

TEST(LexInteger, Large) {
	EXPECT_TRUE(matches<integer>("1234567890"));
}

TEST(LexInteger, Negative) {
	EXPECT_TRUE(matches<integer>("-42"));
}

TEST(LexInteger, EmptyFails) {
	EXPECT_FALSE(matches<integer>(""));
}

TEST(LexInteger, AlphaFails) {
	EXPECT_FALSE(matches<integer>("abc"));
}

// == Keyword boundary =========================================================

TEST(LexKeyword, RegionExact) {
	EXPECT_TRUE(matches<kw<kw_region>>("region"));
}

TEST(LexKeyword, ExternExact) {
	EXPECT_TRUE(matches<kw<kw_extern>>("extern"));
}

TEST(LexKeyword, RegionFollowedBySpace) {
	// kw<kw_region> only matches "region" — the space is leftover
	tao::pegtl::memory_input in("region RR_TEST", "test");
	EXPECT_TRUE(tao::pegtl::parse<kw<kw_region>>(in));
}

TEST(LexKeyword, RegionFollowedByIdentCharFails) {
	// "regionFoo" should not match kw<kw_region>
	EXPECT_FALSE(matches<kw<kw_region>>("regionFoo"));
}

TEST(LexKeyword, RegionFollowedByUnderscoreFails) {
	EXPECT_FALSE(matches<kw<kw_region>>("region_extra"));
}

TEST(LexKeyword, RegionFollowedByDigitFails) {
	EXPECT_FALSE(matches<kw<kw_region>>("region2"));
}

TEST(LexKeyword, TrueExact) {
	EXPECT_TRUE(matches<kw<kw_true>>("true"));
}

TEST(LexKeyword, TrueExtendedFails) {
	EXPECT_FALSE(matches<kw<kw_true>>("trueValue"));
}

TEST(LexKeyword, NotExact) {
	EXPECT_TRUE(matches<kw<kw_not>>("not"));
}

TEST(LexKeyword, NotExtendedFails) {
	EXPECT_FALSE(matches<kw<kw_not>>("nothing"));
}

TEST(LexKeyword, IsExact) {
	EXPECT_TRUE(matches<kw<kw_is>>("is"));
}

TEST(LexKeyword, IsChildExact) {
	EXPECT_TRUE(matches<kw<kw_is_child>>("is_child"));
}

TEST(LexKeyword, IsAdultExact) {
	EXPECT_TRUE(matches<kw<kw_is_adult>>("is_adult"));
}

TEST(LexKeyword, IsVanillaExact) {
	EXPECT_TRUE(matches<kw<kw_is_vanilla>>("is_vanilla"));
}

TEST(LexKeyword, IsMqExact) {
	EXPECT_TRUE(matches<kw<kw_is_mq>>("is_mq"));
}

TEST(LexKeyword, AlwaysExact) {
	EXPECT_TRUE(matches<kw<kw_always>>("always"));
}

TEST(LexKeyword, NeverExact) {
	EXPECT_TRUE(matches<kw<kw_never>>("never"));
}

TEST(LexKeyword, SharedExact) {
	EXPECT_TRUE(matches<kw<kw_shared>>("shared"));
}

TEST(LexKeyword, MatchExact) {
	EXPECT_TRUE(matches<kw<kw_match>>("match"));
}

TEST(LexKeyword, TimePassesExact) {
	EXPECT_TRUE(matches<kw<kw_time_passes>>("time_passes"));
}

TEST(LexKeyword, NoTimePassesExact) {
	EXPECT_TRUE(matches<kw<kw_no_time_passes>>("no_time_passes"));
}

// == Punctuation ==============================================================

TEST(LexPunctuation, OpenBrace) {
	EXPECT_TRUE(matches<open_brace>("{"));
}

TEST(LexPunctuation, CloseBrace) {
	EXPECT_TRUE(matches<close_brace>("}"));
}

TEST(LexPunctuation, OpenParen) {
	EXPECT_TRUE(matches<open_paren>("("));
}

TEST(LexPunctuation, CloseParen) {
	EXPECT_TRUE(matches<close_paren>(")"));
}

TEST(LexPunctuation, Colon) {
	EXPECT_TRUE(matches<colon>(":"));
}

TEST(LexPunctuation, Comma) {
	EXPECT_TRUE(matches<comma>(","));
}

TEST(LexPunctuation, QuestionMark) {
	EXPECT_TRUE(matches<question_mark>("?"));
}

// == Operators ================================================================

TEST(LexOperator, Eq) {
	EXPECT_TRUE(matches<op_eq>("=="));
}

TEST(LexOperator, Neq) {
	EXPECT_TRUE(matches<op_neq>("!="));
}

TEST(LexOperator, Gte) {
	EXPECT_TRUE(matches<op_gte>(">="));
}

TEST(LexOperator, Lte) {
	EXPECT_TRUE(matches<op_lte>("<="));
}

TEST(LexOperator, Gt) {
	EXPECT_TRUE(matches<op_gt>(">"));
}

TEST(LexOperator, Lt) {
	EXPECT_TRUE(matches<op_lt>("<"));
}

TEST(LexOperator, Plus) {
	EXPECT_TRUE(matches<op_plus>("+"));
}

TEST(LexOperator, Minus) {
	EXPECT_TRUE(matches<op_minus>("-"));
}

TEST(LexOperator, Star) {
	EXPECT_TRUE(matches<op_star>("*"));
}

TEST(LexOperator, Slash) {
	EXPECT_TRUE(matches<op_slash>("/"));
}

// == Keyword list exhaustive ==================================================

// Ensure every keyword in the `keyword` rule matches
TEST(LexKeywordList, AllKeywordsRecognized) {
	const std::string keywords[] = {
		"region", "extend", "define",
		"events", "locations", "exits",
		"scene", "areas",
		"time_passes", "no_time_passes",
		"true", "false", "always", "never",
		"and", "or", "not",
		"is",
		"is_child", "is_adult", "at_day", "at_night", "any_age",
		"is_vanilla", "is_mq",
		"shared", "from", "here",
		"match",
	};

	for (const auto& kw : keywords) {
		EXPECT_TRUE(matches<reserved>(kw)) << "keyword '" << kw << "' not recognized";
	}
}

// Ensure identifiers are NOT matched by the reserved-word rule
TEST(LexKeywordList, IdentifiersNotKeywords) {
	const std::string identifiers[] = {
		"RG_HOOKSHOT", "distance", "wallOrFloor",
		"regions", "trueValue", "always_something",
		"nothing", "matchResult", "is_something",
	};

	for (const auto& id : identifiers) {
		EXPECT_FALSE(matches<reserved>(id)) << "'" << id << "' should not be a keyword";
	}
}
