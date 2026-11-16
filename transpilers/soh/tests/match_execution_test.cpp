#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "rls_match.h"

namespace {

enum class Dispatch { FIRST, MIDDLE, LAST, OTHER };
enum class Result { ZERO, VALUE, ALTERNATE };
enum class Alias { ALIAS_FIRST = 1, ALIAS_SECOND = 1, ALIAS_OTHER = 2 };

struct Evaluation {
    std::array<int, 3> values{};
    std::vector<int> calls;
    int conditionCalls = 0;
};

Evaluation evaluation;

bool probe(int index) {
    evaluation.calls.push_back(index);
    return evaluation.values.at(index) != 0;
}

int probe_int(int index) {
    evaluation.calls.push_back(index);
    return evaluation.values.at(index);
}

Result probe_result(int index) {
    return static_cast<Result>(probe_int(index));
}

bool evaluate(std::function<bool()> condition) {
    ++evaluation.conditionCalls;
    return condition();
}

#include "match_execution_fixtures.h"

static_assert(std::is_same_v<decltype(generated::chain(Dispatch::FIRST)), bool>);
static_assert(std::is_same_v<decltype(generated::integer_chain(Dispatch::FIRST)), int>);
static_assert(std::is_same_v<decltype(generated::enum_chain(Dispatch::FIRST)), Result>);
static_assert(std::is_same_v<decltype(generated::mixed_chain(Dispatch::FIRST)), bool>);

class SohMatchExecution : public testing::Test {
protected:
    void SetUp() override { evaluation = {}; }
};

TEST_F(SohMatchExecution, ChainAllEntriesAndBooleanCombinations) {
    for (int entry = 0; entry < 4; ++entry) {
        for (int bits = 0; bits < 8; ++bits) {
            SCOPED_TRACE(testing::Message() << "entry=" << entry << " bits=" << bits);
            evaluation = {{bits & 1, bits & 2, bits & 4}, {}, 0};
            const auto d = static_cast<Dispatch>(entry);
            const bool actual = generated::chain(d);
            const auto actualCalls = evaluation.calls;

            std::vector<int> expectedCalls;
            bool expected = false;
            for (int index = entry; index < 3; ++index) {
                expectedCalls.push_back(index);
                if (evaluation.values[index] != 0) {
                    expected = true;
                    break;
                }
            }
            EXPECT_EQ(actual, expected);
            EXPECT_EQ(actualCalls, expectedCalls);

            evaluation.calls.clear();
            const bool reference = rls::match(
                [&] { return d == Dispatch::FIRST; }, [&] { return probe(0); }, true,
                [&] { return d == Dispatch::MIDDLE; }, [&] { return probe(1); }, true,
                [&] { return d == Dispatch::LAST; }, [&] { return probe(2); }, false);
            EXPECT_EQ(actual, reference);
            EXPECT_EQ(actualCalls, evaluation.calls);
        }
    }
}

TEST_F(SohMatchExecution, TerminalFalseStopsContinuation) {
    evaluation.values = {0, 0, 1};
    EXPECT_FALSE(generated::terminal(Dispatch::FIRST));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0, 1}));
    evaluation.calls.clear();
    EXPECT_TRUE(generated::terminal(Dispatch::LAST));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{2}));
}

TEST_F(SohMatchExecution, GroupedEntryAndWildcardContinuation) {
    for (const auto d : {Dispatch::FIRST, Dispatch::MIDDLE, Dispatch::OTHER}) {
        evaluation = {{0, 1, 0}, {}, 0};
        EXPECT_TRUE(generated::grouped_default(d));
        EXPECT_EQ(evaluation.calls, d == Dispatch::OTHER
            ? (std::vector<int>{1}) : (std::vector<int>{0, 1}));
    }
    evaluation = {{1, 1, 0}, {}, 0};
    EXPECT_TRUE(generated::grouped_default(Dispatch::FIRST));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0}));
}

TEST_F(SohMatchExecution, BodyOrRemainsDistinctFromContinuation) {
    for (int bits = 0; bits < 8; ++bits) {
        evaluation = {{bits & 1, bits & 2, bits & 4}, {}, 0};
        EXPECT_EQ(generated::body_or(Dispatch::FIRST), bits != 0);
        const auto expected = bits & 1 ? std::vector<int>{0}
            : bits & 2 ? std::vector<int>{0, 1} : std::vector<int>{0, 1, 2};
        EXPECT_EQ(evaluation.calls, expected);
    }
}

TEST_F(SohMatchExecution, IntegerValuesAndTypedNoMatch) {
    for (const int first : {0, 7, -7}) {
        evaluation = {{first, 42, 9}, {}, 0};
        EXPECT_EQ(generated::integer_chain(Dispatch::FIRST), first == 0 ? 42 : first);
        EXPECT_EQ(evaluation.calls, first == 0
            ? (std::vector<int>{0, 1}) : (std::vector<int>{0}));
    }
    evaluation = {{0, 0, 9}, {}, 0};
    EXPECT_EQ(generated::integer_chain(Dispatch::FIRST), 0);
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0, 1}));
    evaluation.calls.clear();
    EXPECT_EQ(generated::integer_chain(Dispatch::OTHER), 0);
    EXPECT_TRUE(evaluation.calls.empty());
}

TEST_F(SohMatchExecution, EnumValuesAndTypedNoMatch) {
    for (const int first : {0, 2}) {
        evaluation = {{first, 1, 2}, {}, 0};
        EXPECT_EQ(generated::enum_chain(Dispatch::FIRST),
            first == 0 ? Result::VALUE : Result::ALTERNATE);
        EXPECT_EQ(evaluation.calls, first == 0
            ? (std::vector<int>{0, 1}) : (std::vector<int>{0}));
    }
    evaluation.calls.clear();
    EXPECT_EQ(generated::enum_chain(Dispatch::OTHER), Result::ZERO);
    EXPECT_TRUE(evaluation.calls.empty());
}

TEST_F(SohMatchExecution, MixedBodiesDecayToBool) {
    for (const int first : {0, -7}) {
        evaluation = {{first, 1, 0}, {}, 0};
        EXPECT_TRUE(generated::mixed_chain(Dispatch::FIRST));
        EXPECT_EQ(evaluation.calls, first == 0
            ? (std::vector<int>{0, 1}) : (std::vector<int>{0}));
    }
}

TEST_F(SohMatchExecution, DuplicateAliasedAndRuntimePatternsKeepSourceOrder) {
    evaluation.values = {0, 1, 1};
    EXPECT_TRUE(generated::duplicate(Dispatch::FIRST));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0, 1}));
    evaluation.calls.clear();
    EXPECT_TRUE(generated::aliased(Alias::ALIAS_SECOND));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0, 1}));
    evaluation.calls.clear();
    EXPECT_TRUE(generated::runtime_pattern(Dispatch::FIRST, Dispatch::FIRST));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0, 1}));
    evaluation.calls.clear();
    EXPECT_TRUE(generated::runtime_pattern(Dispatch::FIRST, Dispatch::OTHER));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{1}));
    evaluation.calls.clear();
    EXPECT_TRUE(generated::runtime_pattern(Dispatch::MIDDLE, Dispatch::OTHER));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{2}));
}

TEST_F(SohMatchExecution, CompositionPreservesOuterLazinessAndNestedCaptures) {
    evaluation.values = {1, 1, 0};
    EXPECT_FALSE(generated::composed(Dispatch::FIRST, false));
    EXPECT_TRUE(evaluation.calls.empty());
    EXPECT_EQ(evaluation.conditionCalls, 0);
    EXPECT_TRUE(generated::composed(Dispatch::OTHER, true));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{1}));
    EXPECT_EQ(evaluation.conditionCalls, 0);
    evaluation.calls.clear();
    EXPECT_TRUE(generated::conditional(Dispatch::FIRST, true));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0}));
    evaluation.calls.clear();
    EXPECT_TRUE(generated::conditional(Dispatch::FIRST, false));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{1}));
    evaluation.calls.clear();
    EXPECT_FALSE(generated::thunk(false));
    EXPECT_TRUE(evaluation.calls.empty());
    EXPECT_EQ(evaluation.conditionCalls, 0);
    EXPECT_TRUE(generated::thunk(true));
    EXPECT_EQ(evaluation.calls, (std::vector<int>{0}));
    EXPECT_EQ(evaluation.conditionCalls, 1);
}

} // namespace
