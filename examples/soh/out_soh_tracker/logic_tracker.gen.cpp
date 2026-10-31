#include "logic_tracker.gen.h"
#include "rls_host.h"
#include "functions.gen.h"

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace rls_tracker_generated_detail {

template <typename T>
static int32_t CheckedHostInt(T value) {
	if constexpr (std::is_enum_v<T>) {
		return CheckedHostInt(static_cast<std::underlying_type_t<T>>(value));
	} else if constexpr (std::is_same_v<T, bool>) {
		return value ? int32_t{1} : int32_t{0};
	} else {
		static_assert(std::is_integral_v<T>);
		using Wide = std::conditional_t<std::is_signed_v<T>, intmax_t, uintmax_t>;
		if (!std::in_range<int32_t>(static_cast<Wide>(value))) {
			throw std::out_of_range("Tracker host value is outside int32_t range.");
		}
		return static_cast<int32_t>(value);
	}
}

static value_t any_age_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::any_age(MakeConditionCallback(arguments[0])));
}

static value_t at_night_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::at_night());
}

static value_t bottle_count_adapter(std::span<EvalResult> arguments) {
	return CheckedHostInt(::bottle_count());
}

static value_t can_plant_bean_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::can_plant_bean(static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[0].Result)), static_cast<RandomizerGet>(ToInt<int32_t>(arguments[1].Result))));
}

static value_t can_use_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::can_use(static_cast<RandomizerGet>(ToInt<int32_t>(arguments[0].Result))));
}

static value_t check_price_adapter(std::span<EvalResult> arguments) {
	return CheckedHostInt(::check_price(static_cast<RandomizerCheck>(ToInt<int32_t>(arguments[0].Result))));
}

static value_t collected_triforce_pieces_adapter(std::span<EvalResult> arguments) {
	return CheckedHostInt(::collected_triforce_pieces());
}

static value_t flag_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::flag(static_cast<LogicVal>(ToInt<int32_t>(arguments[0].Result))));
}

static value_t has_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::has(static_cast<RandomizerGet>(ToInt<int32_t>(arguments[0].Result))));
}

static value_t is_adult_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::is_adult());
}

static value_t is_child_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::is_child());
}

static value_t is_mq_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::is_mq());
}

static value_t is_vanilla_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::is_vanilla());
}

static value_t required_triforce_pieces_adapter(std::span<EvalResult> arguments) {
	return CheckedHostInt(::required_triforce_pieces());
}

static value_t setting_adapter(std::span<EvalResult> arguments) {
	return CheckedHostInt(::setting(static_cast<RandomizerSettingKey>(ToInt<int32_t>(arguments[0].Result))));
}

static value_t trick_adapter(std::span<EvalResult> arguments) {
	return static_cast<bool>(::trick(static_cast<RandomizerTrick>(ToInt<int32_t>(arguments[0].Result))));
}

static std::unique_ptr<LogicExpression> CreateExpression(index_t index) {
	switch (index) {
	case 0U: return std::make_unique<ExternFunctionCallExpression>(is_vanilla_adapter, "is_vanilla", std::vector<index_t>{});
	case 1U: return std::make_unique<ExternFunctionCallExpression>(is_mq_adapter, "is_mq", std::vector<index_t>{});
	case 2U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 3U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 4U: return std::make_unique<ExternFunctionCallExpression>(is_vanilla_adapter, "is_vanilla", std::vector<index_t>{});
	case 5U: return std::make_unique<ExternFunctionCallExpression>(is_mq_adapter, "is_mq", std::vector<index_t>{});
	case 6U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{7U, 9U, 11U, 13U, 15U, 17U}, 19U);
	case 7U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 8U);
	case 8U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOHMA); }, "RE_GOHMA", false);
	case 9U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 10U);
	case 10U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 11U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 12U);
	case 12U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 13U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 14U);
	case 14U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 15U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 16U);
	case 16U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 17U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 18U);
	case 18U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 19U: return std::make_unique<MatchExpression<bool>>(20U, std::vector<index_t>{21U});
	case 20U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("e"); }, "e", false);
	case 21U: return std::make_unique<MatchArm>(22U, 24U, false);
	case 22U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{23U});
	case 23U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 24U: return std::make_unique<FunctionCallExpression>("_can_kill_gold_skulltula", std::vector<index_t>{25U, 27U}, 29U);
	case 25U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 26U);
	case 26U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 27U: return std::make_unique<ParameterExpression>(uint8_t{1}, "wall_or_floor", 28U);
	case 28U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("wall_or_floor"); }, "wall_or_floor", false);
	case 29U: return std::make_unique<MatchExpression<bool>>(30U, std::vector<index_t>{31U, 37U, 43U, 49U, 59U, 65U, 75U, 81U, 93U});
	case 30U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 31U: return std::make_unique<MatchArm>(32U, 34U, true);
	case 32U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{33U});
	case 33U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 34U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{35U});
	case 35U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 36U);
	case 36U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 37U: return std::make_unique<MatchArm>(38U, 40U, true);
	case 38U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{39U});
	case 39U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH); }, "ED_SHORT_JUMPSLASH", false);
	case 40U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{41U});
	case 41U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 42U);
	case 42U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD); }, "RG_KOKIRI_SWORD", false);
	case 43U: return std::make_unique<MatchArm>(44U, 46U, true);
	case 44U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{45U});
	case 45U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_MASTER_SWORD_JUMPSLASH); }, "ED_MASTER_SWORD_JUMPSLASH", false);
	case 46U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{47U});
	case 47U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 48U);
	case 48U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MASTER_SWORD); }, "RG_MASTER_SWORD", false);
	case 49U: return std::make_unique<MatchArm>(50U, 52U, true);
	case 50U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{51U});
	case 51U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONG_JUMPSLASH); }, "ED_LONG_JUMPSLASH", false);
	case 52U: return std::make_unique<OrExpression>(std::vector<index_t>{53U, 56U});
	case 53U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{54U});
	case 54U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 55U);
	case 55U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BIGGORON_SWORD); }, "RG_BIGGORON_SWORD", false);
	case 56U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{57U});
	case 57U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 58U);
	case 58U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 59U: return std::make_unique<MatchArm>(60U, 62U, true);
	case 60U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{61U});
	case 61U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOMB_THROW); }, "ED_BOMB_THROW", false);
	case 62U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{63U});
	case 63U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 64U);
	case 64U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMB_BAG); }, "RG_BOMB_BAG", false);
	case 65U: return std::make_unique<MatchArm>(66U, 68U, true);
	case 66U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{67U});
	case 67U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOOMERANG); }, "ED_BOOMERANG", false);
	case 68U: return std::make_unique<OrExpression>(std::vector<index_t>{69U, 72U});
	case 69U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{70U});
	case 70U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 71U);
	case 71U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 72U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{73U});
	case 73U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 74U);
	case 74U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DINS_FIRE); }, "RG_DINS_FIRE", false);
	case 75U: return std::make_unique<MatchArm>(76U, 78U, true);
	case 76U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{77U});
	case 77U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_HOOKSHOT); }, "ED_HOOKSHOT", false);
	case 78U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{79U});
	case 79U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 80U);
	case 80U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 81U: return std::make_unique<MatchArm>(82U, 84U, true);
	case 82U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{83U});
	case 83U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONGSHOT); }, "ED_LONGSHOT", false);
	case 84U: return std::make_unique<OrExpression>(std::vector<index_t>{85U, 88U});
	case 85U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{86U});
	case 86U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 87U);
	case 87U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 88U: return std::make_unique<AndExpression>(std::vector<index_t>{89U, 90U});
	case 89U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("wall_or_floor"); }, "wall_or_floor", false);
	case 90U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{91U});
	case 91U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 92U);
	case 92U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMBCHU_5); }, "RG_BOMBCHU_5", false);
	case 93U: return std::make_unique<MatchArm>(94U, 96U, false);
	case 94U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{95U});
	case 95U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_FAR); }, "ED_FAR", false);
	case 96U: return std::make_unique<OrExpression>(std::vector<index_t>{97U, 100U});
	case 97U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{98U});
	case 98U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 99U);
	case 99U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_SLINGSHOT); }, "RG_FAIRY_SLINGSHOT", false);
	case 100U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{101U});
	case 101U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 102U);
	case 102U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 103U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{104U});
	case 104U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 105U);
	case 105U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR); }, "LOGIC_DEKU_TREE_CLEAR", false);
	case 106U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{107U});
	case 107U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 108U);
	case 108U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR); }, "LOGIC_DEKU_TREE_CLEAR", false);
	case 109U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 110U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{111U});
	case 111U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 112U);
	case 112U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR); }, "LOGIC_DEKU_TREE_CLEAR", false);
	case 113U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{114U, 116U, 118U, 120U, 122U, 124U}, 19U);
	case 114U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 115U);
	case 115U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_WITHERED_DEKU_BABA); }, "RE_WITHERED_DEKU_BABA", false);
	case 116U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 117U);
	case 117U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 118U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 119U);
	case 119U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 120U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 121U);
	case 121U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 122U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 123U);
	case 123U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 124U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 125U);
	case 125U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 126U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 127U: return std::make_unique<OrExpression>(std::vector<index_t>{128U, 131U});
	case 128U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{129U});
	case 129U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 130U);
	case 130U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DINS_FIRE); }, "RG_DINS_FIRE", false);
	case 131U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{132U});
	case 132U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 133U);
	case 133U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FIRE_ARROWS); }, "RG_FIRE_ARROWS", false);
	case 134U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 135U: return std::make_unique<OrExpression>(std::vector<index_t>{136U, 139U});
	case 136U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{137U});
	case 137U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 138U);
	case 138U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 139U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{140U});
	case 140U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 141U);
	case 141U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 142U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{143U});
	case 143U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 144U);
	case 144U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_1F_BROKE_WEB); }, "LOGIC_DEKU_TREE_1F_BROKE_WEB", false);
	case 145U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 146U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 147U: return std::make_unique<OrExpression>(std::vector<index_t>{148U, 151U});
	case 148U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{149U});
	case 149U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 150U);
	case 150U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 151U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{152U});
	case 152U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 153U);
	case 153U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 154U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{155U});
	case 155U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 156U);
	case 156U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB); }, "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", false);
	case 157U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 158U: return std::make_unique<OrExpression>(std::vector<index_t>{159U, 170U});
	case 159U: return std::make_unique<FunctionCallExpression>("can_use_sword", std::vector<index_t>{}, 160U);
	case 160U: return std::make_unique<OrExpression>(std::vector<index_t>{161U, 164U, 167U});
	case 161U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{162U});
	case 162U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 163U);
	case 163U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD); }, "RG_KOKIRI_SWORD", false);
	case 164U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{165U});
	case 165U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 166U);
	case 166U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MASTER_SWORD); }, "RG_MASTER_SWORD", false);
	case 167U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{168U});
	case 168U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 169U);
	case 169U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BIGGORON_SWORD); }, "RG_BIGGORON_SWORD", false);
	case 170U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{171U});
	case 171U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 172U);
	case 172U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 173U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 174U: return std::make_unique<OrExpression>(std::vector<index_t>{175U, 186U, 189U, 192U, 200U});
	case 175U: return std::make_unique<FunctionCallExpression>("can_jumpslash", std::vector<index_t>{}, 176U);
	case 176U: return std::make_unique<OrExpression>(std::vector<index_t>{177U, 183U});
	case 177U: return std::make_unique<FunctionCallExpression>("can_jumpslash_except_hammer", std::vector<index_t>{}, 178U);
	case 178U: return std::make_unique<OrExpression>(std::vector<index_t>{179U, 182U});
	case 179U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{180U});
	case 180U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 181U);
	case 181U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 182U: return std::make_unique<FunctionCallExpression>("can_use_sword", std::vector<index_t>{}, 160U);
	case 183U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{184U});
	case 184U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 185U);
	case 185U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 186U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{187U});
	case 187U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 188U);
	case 188U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_SLINGSHOT); }, "RG_FAIRY_SLINGSHOT", false);
	case 189U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{190U});
	case 190U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 191U);
	case 191U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 192U: return std::make_unique<FunctionCallExpression>("has_explosives", std::vector<index_t>{}, 193U);
	case 193U: return std::make_unique<OrExpression>(std::vector<index_t>{194U, 197U});
	case 194U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{195U});
	case 195U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 196U);
	case 196U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMB_BAG); }, "RG_BOMB_BAG", false);
	case 197U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{198U});
	case 198U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 199U);
	case 199U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMBCHU_5); }, "RG_BOMBCHU_5", false);
	case 200U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{201U});
	case 201U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 202U);
	case 202U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DINS_FIRE); }, "RG_DINS_FIRE", false);
	case 203U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 204U: return std::make_unique<OrExpression>(std::vector<index_t>{205U, 208U});
	case 205U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{206U});
	case 206U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 207U);
	case 207U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 208U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{209U});
	case 209U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 210U);
	case 210U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 211U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 212U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 213U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 214U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 215U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 216U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{217U, 219U, 221U, 223U, 225U, 227U}, 19U);
	case 217U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 218U);
	case 218U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_DEKU_BABA); }, "RE_DEKU_BABA", false);
	case 219U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 220U);
	case 220U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 221U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 222U);
	case 222U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 223U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 224U);
	case 224U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 225U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 226U);
	case 226U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 227U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 228U);
	case 228U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 229U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{230U});
	case 230U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 231U);
	case 231U: return std::make_unique<ConditionExpression>(232U);
	case 232U: return std::make_unique<FunctionCallExpression>("can_hit_eye_targets", std::vector<index_t>{}, 233U);
	case 233U: return std::make_unique<OrExpression>(std::vector<index_t>{234U, 237U});
	case 234U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{235U});
	case 235U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 236U);
	case 236U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 237U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{238U});
	case 238U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 239U);
	case 239U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_SLINGSHOT); }, "RG_FAIRY_SLINGSHOT", false);
	case 240U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 241U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 242U: return std::make_unique<AndExpression>(std::vector<index_t>{243U, 255U});
	case 243U: return std::make_unique<OrExpression>(std::vector<index_t>{244U, 247U, 250U});
	case 244U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{245U});
	case 245U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 246U);
	case 246U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 247U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{248U});
	case 248U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 249U);
	case 249U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 250U: return std::make_unique<AndExpression>(std::vector<index_t>{251U, 252U});
	case 251U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 252U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{253U});
	case 253U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 254U);
	case 254U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME); }, "RG_SONG_OF_TIME", false);
	case 255U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{256U});
	case 256U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 257U);
	case 257U: return std::make_unique<ConditionExpression>(258U);
	case 258U: return std::make_unique<OrExpression>(std::vector<index_t>{259U, 262U, 274U});
	case 259U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{260U});
	case 260U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 261U);
	case 261U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMBCHU_5); }, "RG_BOMBCHU_5", false);
	case 262U: return std::make_unique<AndExpression>(std::vector<index_t>{263U, 266U});
	case 263U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{264U});
	case 264U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 265U);
	case 265U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMB_BAG); }, "RG_BOMB_BAG", false);
	case 266U: return std::make_unique<OrExpression>(std::vector<index_t>{267U, 270U, 271U});
	case 267U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{268U});
	case 268U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 269U);
	case 269U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME); }, "RG_SONG_OF_TIME", false);
	case 270U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 271U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{272U});
	case 272U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 273U);
	case 273U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 274U: return std::make_unique<AndExpression>(std::vector<index_t>{275U, 278U});
	case 275U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{276U});
	case 276U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 277U);
	case 277U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 278U: return std::make_unique<OrExpression>(std::vector<index_t>{279U, 284U});
	case 279U: return std::make_unique<AndExpression>(std::vector<index_t>{280U, 281U});
	case 280U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 281U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{282U});
	case 282U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 283U);
	case 283U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME); }, "RG_SONG_OF_TIME", false);
	case 284U: return std::make_unique<AndExpression>(std::vector<index_t>{285U, 288U});
	case 285U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{286U});
	case 286U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 287U);
	case 287U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_COMPASS_GS); }, "RT_DEKU_MQ_COMPASS_GS", false);
	case 288U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{289U});
	case 289U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 290U);
	case 290U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 291U: return std::make_unique<FunctionCallExpression>("blast_or_smash", std::vector<index_t>{}, 292U);
	case 292U: return std::make_unique<OrExpression>(std::vector<index_t>{293U, 294U});
	case 293U: return std::make_unique<FunctionCallExpression>("has_explosives", std::vector<index_t>{}, 193U);
	case 294U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{295U});
	case 295U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 296U);
	case 296U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 297U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 298U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 299U: return std::make_unique<AndExpression>(std::vector<index_t>{300U, 303U});
	case 300U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{301U});
	case 301U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 302U);
	case 302U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 303U: return std::make_unique<OrExpression>(std::vector<index_t>{304U, 307U, 308U});
	case 304U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{305U});
	case 305U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 306U);
	case 306U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK); }, "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", false);
	case 307U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 308U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{309U});
	case 309U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 310U);
	case 310U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 311U: return std::make_unique<OrExpression>(std::vector<index_t>{312U, 315U});
	case 312U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{313U});
	case 313U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 314U);
	case 314U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 315U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{316U});
	case 316U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 317U);
	case 317U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 318U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{319U});
	case 319U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 320U);
	case 320U: return std::make_unique<ConditionExpression>(321U);
	case 321U: return std::make_unique<FunctionCallExpression>("can_hit_eye_targets", std::vector<index_t>{}, 233U);
	case 322U: return std::make_unique<AndExpression>(std::vector<index_t>{323U, 327U, 330U});
	case 323U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{324U});
	case 324U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 325U);
	case 325U: return std::make_unique<ConditionExpression>(326U);
	case 326U: return std::make_unique<FunctionCallExpression>("can_hit_eye_targets", std::vector<index_t>{}, 233U);
	case 327U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{328U});
	case 328U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 329U);
	case 329U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM); }, "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", false);
	case 330U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{331U});
	case 331U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 332U);
	case 332U: return std::make_unique<ConditionExpression>(333U);
	case 333U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{334U});
	case 334U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 335U);
	case 335U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 336U: return std::make_unique<OrExpression>(std::vector<index_t>{337U, 338U, 341U, 370U, 373U});
	case 337U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 338U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{339U});
	case 339U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 340U);
	case 340U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_SKIP); }, "RT_DEKU_B1_SKIP", false);
	case 341U: return std::make_unique<FunctionCallExpression>("can_ground_jump", std::vector<index_t>{342U}, 344U);
	case 342U: return std::make_unique<ParameterExpression>(uint8_t{0}, "hasBombflower", 343U);
	case 343U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 344U: return std::make_unique<AndExpression>(std::vector<index_t>{345U, 348U, 361U});
	case 345U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{346U});
	case 346U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 347U);
	case 347U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_GROUND_JUMP); }, "RT_GROUND_JUMP", false);
	case 348U: return std::make_unique<FunctionCallExpression>("can_standing_shield", std::vector<index_t>{}, 349U);
	case 349U: return std::make_unique<OrExpression>(std::vector<index_t>{350U, 353U, 358U});
	case 350U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{351U});
	case 351U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 352U);
	case 352U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MIRROR_SHIELD); }, "RG_MIRROR_SHIELD", false);
	case 353U: return std::make_unique<AndExpression>(std::vector<index_t>{354U, 355U});
	case 354U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 355U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{356U});
	case 356U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 357U);
	case 357U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HYLIAN_SHIELD); }, "RG_HYLIAN_SHIELD", false);
	case 358U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{359U});
	case 359U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 360U);
	case 360U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD); }, "RG_DEKU_SHIELD", false);
	case 361U: return std::make_unique<OrExpression>(std::vector<index_t>{362U, 365U});
	case 362U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{363U});
	case 363U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 364U);
	case 364U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMB_BAG); }, "RG_BOMB_BAG", false);
	case 365U: return std::make_unique<AndExpression>(std::vector<index_t>{366U, 367U});
	case 366U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("hasBombflower"); }, "hasBombflower", false);
	case 367U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{368U});
	case 368U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 369U);
	case 369U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_GORONS_BRACELET); }, "RG_GORONS_BRACELET", false);
	case 370U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{371U});
	case 371U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 372U);
	case 372U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK); }, "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", false);
	case 373U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{374U});
	case 374U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 375U);
	case 375U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 376U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{377U, 379U, 381U, 383U, 385U, 387U}, 19U);
	case 377U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 378U);
	case 378U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_MAD_SCRUB); }, "RE_MAD_SCRUB", false);
	case 379U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 380U);
	case 380U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 381U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 382U);
	case 382U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 383U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 384U);
	case 384U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 385U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 386U);
	case 386U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 387U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 388U);
	case 388U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 389U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 390U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{391U});
	case 391U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 392U);
	case 392U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM); }, "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", false);
	case 393U: return std::make_unique<OrExpression>(std::vector<index_t>{394U, 397U});
	case 394U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{395U});
	case 395U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 396U);
	case 396U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FIRE_ARROWS); }, "RG_FIRE_ARROWS", false);
	case 397U: return std::make_unique<AndExpression>(std::vector<index_t>{398U, 401U});
	case 398U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{399U});
	case 399U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 400U);
	case 400U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 401U: return std::make_unique<OrExpression>(std::vector<index_t>{402U, 405U});
	case 402U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{403U});
	case 403U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 404U);
	case 404U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_LOG); }, "RT_DEKU_MQ_LOG", false);
	case 405U: return std::make_unique<AndExpression>(std::vector<index_t>{406U, 407U});
	case 406U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 407U: return std::make_unique<FunctionCallExpression>("can_shield", std::vector<index_t>{}, 408U);
	case 408U: return std::make_unique<OrExpression>(std::vector<index_t>{409U, 412U, 415U});
	case 409U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{410U});
	case 410U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 411U);
	case 411U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MIRROR_SHIELD); }, "RG_MIRROR_SHIELD", false);
	case 412U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{413U});
	case 413U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 414U);
	case 414U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HYLIAN_SHIELD); }, "RG_HYLIAN_SHIELD", false);
	case 415U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{416U});
	case 416U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 417U);
	case 417U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD); }, "RG_DEKU_SHIELD", false);
	case 418U: return std::make_unique<OrExpression>(std::vector<index_t>{419U, 422U, 425U, 428U});
	case 419U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{420U});
	case 420U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 421U);
	case 421U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_LOG); }, "RT_DEKU_MQ_LOG", false);
	case 422U: return std::make_unique<AndExpression>(std::vector<index_t>{423U, 424U});
	case 423U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 424U: return std::make_unique<FunctionCallExpression>("can_shield", std::vector<index_t>{}, 408U);
	case 425U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{426U});
	case 426U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 427U);
	case 427U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 428U: return std::make_unique<AndExpression>(std::vector<index_t>{429U, 432U});
	case 429U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{430U});
	case 430U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 431U);
	case 431U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 432U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{433U});
	case 433U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 434U);
	case 434U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_IRON_BOOTS); }, "RG_IRON_BOOTS", false);
	case 435U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 436U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{437U, 439U, 441U, 443U, 445U, 447U}, 19U);
	case 437U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 438U);
	case 438U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_WITHERED_DEKU_BABA); }, "RE_WITHERED_DEKU_BABA", false);
	case 439U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 440U);
	case 440U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 441U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 442U);
	case 442U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 443U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 444U);
	case 444U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 445U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 446U);
	case 446U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 447U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 448U);
	case 448U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 449U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 450U: return std::make_unique<AndExpression>(std::vector<index_t>{451U, 454U});
	case 451U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{452U});
	case 452U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 453U);
	case 453U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES); }, "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", false);
	case 454U: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{455U, 457U, 464U}, 466U);
	case 455U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 456U);
	case 456U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_BIG_SKULLTULA); }, "RE_BIG_SKULLTULA", false);
	case 457U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 458U);
	case 458U: return std::make_unique<TernaryExpression>(459U, 462U, 463U);
	case 459U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{460U});
	case 460U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 461U);
	case 461U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME); }, "RG_SONG_OF_TIME", false);
	case 462U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 463U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH); }, "ED_SHORT_JUMPSLASH", false);
	case 464U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 465U);
	case 465U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 466U: return std::make_unique<OrExpression>(std::vector<index_t>{467U, 480U});
	case 467U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{468U, 470U, 472U, 474U, 476U, 478U}, 19U);
	case 468U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 469U);
	case 469U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("e"); }, "e", false);
	case 470U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 471U);
	case 471U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 472U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 473U);
	case 473U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("wall_or_floor"); }, "wall_or_floor", false);
	case 474U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 475U);
	case 475U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 476U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 477U);
	case 477U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 478U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 479U);
	case 479U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 480U: return std::make_unique<MatchExpression<bool>>(481U, std::vector<index_t>{482U});
	case 481U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("e"); }, "e", false);
	case 482U: return std::make_unique<MatchArm>(483U, 485U, false);
	case 483U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{484U});
	case 484U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 485U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 486U: return std::make_unique<OrExpression>(std::vector<index_t>{487U, 490U, 493U, 496U, 499U});
	case 487U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{488U});
	case 488U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 489U);
	case 489U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_LOG); }, "RT_DEKU_MQ_LOG", false);
	case 490U: return std::make_unique<AndExpression>(std::vector<index_t>{491U, 492U});
	case 491U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 492U: return std::make_unique<FunctionCallExpression>("can_shield", std::vector<index_t>{}, 408U);
	case 493U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{494U});
	case 494U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 495U);
	case 495U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 496U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{497U});
	case 497U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 498U);
	case 498U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 499U: return std::make_unique<AndExpression>(std::vector<index_t>{500U, 503U});
	case 500U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{501U});
	case 501U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 502U);
	case 502U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_IRON_BOOTS); }, "RG_IRON_BOOTS", false);
	case 503U: return std::make_unique<OrExpression>(std::vector<index_t>{504U, 505U});
	case 504U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 505U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{506U});
	case 506U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 507U);
	case 507U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 508U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{509U});
	case 509U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 510U);
	case 510U: return std::make_unique<ConditionExpression>(511U);
	case 511U: return std::make_unique<AndExpression>(std::vector<index_t>{512U, 525U});
	case 512U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{513U, 515U, 517U, 519U, 521U, 523U}, 19U);
	case 513U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 514U);
	case 514U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_MAD_SCRUB); }, "RE_MAD_SCRUB", false);
	case 515U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 516U);
	case 516U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 517U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 518U);
	case 518U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 519U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 520U);
	case 520U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 521U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 522U);
	case 522U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 523U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 524U);
	case 524U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 525U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{526U, 528U, 530U, 532U, 534U, 536U}, 19U);
	case 526U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 527U);
	case 527U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_KEESE); }, "RE_KEESE", false);
	case 528U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 529U);
	case 529U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 530U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 531U);
	case 531U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 532U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 533U);
	case 533U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 534U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 535U);
	case 535U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 536U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 537U);
	case 537U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 538U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{539U});
	case 539U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 540U);
	case 540U: return std::make_unique<ConditionExpression>(541U);
	case 541U: return std::make_unique<AndExpression>(std::vector<index_t>{542U, 555U});
	case 542U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{543U, 545U, 547U, 549U, 551U, 553U}, 19U);
	case 543U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 544U);
	case 544U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_MAD_SCRUB); }, "RE_MAD_SCRUB", false);
	case 545U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 546U);
	case 546U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 547U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 548U);
	case 548U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 549U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 550U);
	case 550U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 551U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 552U);
	case 552U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 553U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 554U);
	case 554U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 555U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{556U, 558U, 560U, 562U, 564U, 566U}, 19U);
	case 556U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 557U);
	case 557U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_KEESE); }, "RE_KEESE", false);
	case 558U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 559U);
	case 559U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 560U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 561U);
	case 561U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 562U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 563U);
	case 563U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 564U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 565U);
	case 565U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 566U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 567U);
	case 567U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 568U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 569U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 570U: return std::make_unique<AndExpression>(std::vector<index_t>{571U, 574U});
	case 571U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{572U});
	case 572U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 573U);
	case 573U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CRAWL); }, "RG_CRAWL", false);
	case 574U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{575U});
	case 575U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 576U);
	case 576U: return std::make_unique<ConditionExpression>(577U);
	case 577U: return std::make_unique<OrExpression>(std::vector<index_t>{578U, 584U});
	case 578U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 579U: return std::make_unique<OrExpression>(std::vector<index_t>{580U, 581U});
	case 580U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 581U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{582U});
	case 582U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 583U);
	case 583U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 584U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{585U});
	case 585U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 586U);
	case 586U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 587U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 588U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{589U});
	case 589U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 590U);
	case 590U: return std::make_unique<ConditionExpression>(591U);
	case 591U: return std::make_unique<OrExpression>(std::vector<index_t>{592U, 593U});
	case 592U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 593U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{594U});
	case 594U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 595U);
	case 595U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 596U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 597U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{598U});
	case 598U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 599U);
	case 599U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 600U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 601U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{602U});
	case 602U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 603U);
	case 603U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CRAWL); }, "RG_CRAWL", false);
	case 604U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 605U: return std::make_unique<AndExpression>(std::vector<index_t>{606U, 609U});
	case 606U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{607U});
	case 607U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 608U);
	case 608U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_B1_BROKE_WEB); }, "LOGIC_DEKU_TREE_B1_BROKE_WEB", false);
	case 609U: return std::make_unique<OrExpression>(std::vector<index_t>{610U, 613U});
	case 610U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{611U});
	case 611U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 612U);
	case 612U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 613U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{614U});
	case 614U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 615U);
	case 615U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_IRON_BOOTS); }, "RG_IRON_BOOTS", false);
	case 616U: return std::make_unique<AndExpression>(std::vector<index_t>{617U, 620U});
	case 617U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{618U});
	case 618U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 619U);
	case 619U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 620U: return std::make_unique<OrExpression>(std::vector<index_t>{621U, 624U});
	case 621U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{622U});
	case 622U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 623U);
	case 623U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 624U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{625U});
	case 625U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 626U);
	case 626U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 627U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{628U});
	case 628U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 629U);
	case 629U: return std::make_unique<ConditionExpression>(630U);
	case 630U: return std::make_unique<FunctionCallExpression>("can_reflect_nuts", std::vector<index_t>{}, 631U);
	case 631U: return std::make_unique<OrExpression>(std::vector<index_t>{632U, 635U});
	case 632U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{633U});
	case 633U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 634U);
	case 634U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD); }, "RG_DEKU_SHIELD", false);
	case 635U: return std::make_unique<AndExpression>(std::vector<index_t>{636U, 637U});
	case 636U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 637U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{638U});
	case 638U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 639U);
	case 639U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HYLIAN_SHIELD); }, "RG_HYLIAN_SHIELD", false);
	case 640U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 641U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 642U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 643U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 644U: return std::make_unique<OrExpression>(std::vector<index_t>{645U, 648U});
	case 645U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{646U});
	case 646U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 647U);
	case 647U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 648U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{649U});
	case 649U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 650U);
	case 650U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 651U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{652U});
	case 652U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 653U);
	case 653U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_1F_BROKE_WEB); }, "LOGIC_DEKU_TREE_1F_BROKE_WEB", false);
	case 654U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 655U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 656U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 657U: return std::make_unique<OrExpression>(std::vector<index_t>{658U, 661U});
	case 658U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{659U});
	case 659U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 660U);
	case 660U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 661U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{662U});
	case 662U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 663U);
	case 663U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 664U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 665U: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{666U, 668U, 670U}, 466U);
	case 666U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 667U);
	case 667U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_BIG_SKULLTULA); }, "RE_BIG_SKULLTULA", false);
	case 668U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 669U);
	case 669U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 670U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 671U);
	case 671U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 672U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 673U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 674U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{675U});
	case 675U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 676U);
	case 676U: return std::make_unique<ConditionExpression>(677U);
	case 677U: return std::make_unique<OrExpression>(std::vector<index_t>{678U, 679U});
	case 678U: return std::make_unique<FunctionCallExpression>("can_reflect_nuts", std::vector<index_t>{}, 631U);
	case 679U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{680U});
	case 680U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 681U);
	case 681U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 682U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{683U});
	case 683U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 684U);
	case 684U: return std::make_unique<ConditionExpression>(685U);
	case 685U: return std::make_unique<OrExpression>(std::vector<index_t>{686U, 687U});
	case 686U: return std::make_unique<FunctionCallExpression>("can_reflect_nuts", std::vector<index_t>{}, 631U);
	case 687U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{688U});
	case 688U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 689U);
	case 689U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 690U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 691U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 692U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 693U: return std::make_unique<OrExpression>(std::vector<index_t>{694U, 695U});
	case 694U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 695U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{696U});
	case 696U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 697U);
	case 697U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 698U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 699U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 700U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 701U: return std::make_unique<OrExpression>(std::vector<index_t>{702U, 714U});
	case 702U: return std::make_unique<AndExpression>(std::vector<index_t>{703U, 706U});
	case 703U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{704U});
	case 704U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 705U);
	case 705U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 706U: return std::make_unique<OrExpression>(std::vector<index_t>{707U, 710U, 711U});
	case 707U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{708U});
	case 708U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 709U);
	case 709U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK); }, "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", false);
	case 710U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 711U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{712U});
	case 712U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 713U);
	case 713U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 714U: return std::make_unique<AndExpression>(std::vector<index_t>{715U, 718U, 719U});
	case 715U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{716U});
	case 716U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 717U);
	case 717U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BOW_WEBS); }, "RT_DEKU_B1_BOW_WEBS", false);
	case 718U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 719U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{720U});
	case 720U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 721U);
	case 721U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 722U: return std::make_unique<OrExpression>(std::vector<index_t>{723U, 726U});
	case 723U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{724U});
	case 724U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 725U);
	case 725U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 726U: return std::make_unique<AndExpression>(std::vector<index_t>{727U, 728U});
	case 727U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 728U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{729U});
	case 729U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 730U);
	case 730U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 731U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{732U});
	case 732U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 733U);
	case 733U: return std::make_unique<ConditionExpression>(734U);
	case 734U: return std::make_unique<OrExpression>(std::vector<index_t>{735U, 736U});
	case 735U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 736U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{737U});
	case 737U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 738U);
	case 738U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 739U: return std::make_unique<OrExpression>(std::vector<index_t>{740U, 741U, 744U, 747U});
	case 740U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 741U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{742U});
	case 742U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 743U);
	case 743U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_SKIP); }, "RT_DEKU_B1_SKIP", false);
	case 744U: return std::make_unique<FunctionCallExpression>("can_ground_jump", std::vector<index_t>{745U}, 344U);
	case 745U: return std::make_unique<ParameterExpression>(uint8_t{0}, "hasBombflower", 746U);
	case 746U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 747U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{748U});
	case 748U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 749U);
	case 749U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK); }, "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", false);
	case 750U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 751U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 752U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{753U});
	case 753U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 754U);
	case 754U: return std::make_unique<ConditionExpression>(755U);
	case 755U: return std::make_unique<FunctionCallExpression>("can_hit_eye_targets", std::vector<index_t>{}, 233U);
	case 756U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 757U: return std::make_unique<OrExpression>(std::vector<index_t>{758U, 761U});
	case 758U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{759U});
	case 759U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 760U);
	case 760U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 761U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{762U});
	case 762U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 763U);
	case 763U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG); }, "RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG", false);
	case 764U: return std::make_unique<OrExpression>(std::vector<index_t>{765U, 768U});
	case 765U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{766U});
	case 766U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 767U);
	case 767U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 768U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{769U});
	case 769U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 770U);
	case 770U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG); }, "RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG", false);
	case 771U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 772U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 773U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 774U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{775U});
	case 775U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 776U);
	case 776U: return std::make_unique<ConditionExpression>(777U);
	case 777U: return std::make_unique<OrExpression>(std::vector<index_t>{778U, 779U});
	case 778U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 779U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{780U});
	case 780U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 781U);
	case 781U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 782U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{783U});
	case 783U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 784U);
	case 784U: return std::make_unique<ConditionExpression>(785U);
	case 785U: return std::make_unique<OrExpression>(std::vector<index_t>{786U, 787U});
	case 786U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 787U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{788U});
	case 788U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 789U);
	case 789U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 790U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 791U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 792U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 793U: return std::make_unique<AndExpression>(std::vector<index_t>{794U, 802U});
	case 794U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{795U});
	case 795U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 796U);
	case 796U: return std::make_unique<ConditionExpression>(797U);
	case 797U: return std::make_unique<OrExpression>(std::vector<index_t>{798U, 799U});
	case 798U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 799U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{800U});
	case 800U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 801U);
	case 801U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 802U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{803U});
	case 803U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 804U);
	case 804U: return std::make_unique<ConditionExpression>(805U);
	case 805U: return std::make_unique<FunctionCallExpression>("blast_or_smash", std::vector<index_t>{}, 292U);
	case 806U: return std::make_unique<AndExpression>(std::vector<index_t>{807U, 815U});
	case 807U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{808U});
	case 808U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 809U);
	case 809U: return std::make_unique<ConditionExpression>(810U);
	case 810U: return std::make_unique<OrExpression>(std::vector<index_t>{811U, 812U});
	case 811U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 579U);
	case 812U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{813U});
	case 813U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 814U);
	case 814U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 815U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{816U});
	case 816U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 817U);
	case 817U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CRAWL); }, "RG_CRAWL", false);
	case 818U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 819U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 820U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 821U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{822U});
	case 822U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 823U);
	case 823U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 824U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 127U);
	case 825U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 826U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{827U});
	case 827U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 828U);
	case 828U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CRAWL); }, "RG_CRAWL", false);
	case 829U: return std::make_unique<AndExpression>(std::vector<index_t>{830U, 833U});
	case 830U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{831U});
	case 831U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 832U);
	case 832U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_B1_BROKE_WEB); }, "LOGIC_DEKU_TREE_B1_BROKE_WEB", false);
	case 833U: return std::make_unique<OrExpression>(std::vector<index_t>{834U, 837U});
	case 834U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{835U});
	case 835U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 836U);
	case 836U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 837U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{838U});
	case 838U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 839U);
	case 839U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_IRON_BOOTS); }, "RG_IRON_BOOTS", false);
	case 840U: return std::make_unique<AndExpression>(std::vector<index_t>{841U, 844U});
	case 841U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{842U});
	case 842U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 843U);
	case 843U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 844U: return std::make_unique<OrExpression>(std::vector<index_t>{845U, 848U});
	case 845U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{846U});
	case 846U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 847U);
	case 847U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 848U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{849U});
	case 849U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 850U);
	case 850U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 851U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{852U});
	case 852U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 853U);
	case 853U: return std::make_unique<ConditionExpression>(854U);
	case 854U: return std::make_unique<FunctionCallExpression>("can_reflect_nuts", std::vector<index_t>{}, 631U);
	case 855U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 856U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 857U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 858U: return std::make_unique<OrExpression>(std::vector<index_t>{859U, 870U});
	case 859U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 860U);
	case 860U: return std::make_unique<OrExpression>(std::vector<index_t>{861U, 864U, 867U});
	case 861U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{862U});
	case 862U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 863U);
	case 863U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_ZELDAS_LULLABY); }, "RG_ZELDAS_LULLABY", false);
	case 864U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{865U});
	case 865U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 866U);
	case 866U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_EPONAS_SONG); }, "RG_EPONAS_SONG", false);
	case 867U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{868U});
	case 868U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 869U);
	case 869U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME); }, "RG_SONG_OF_TIME", false);
	case 870U: return std::make_unique<AndExpression>(std::vector<index_t>{871U, 872U, 875U, 878U});
	case 871U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 872U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{873U});
	case 873U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 874U);
	case 874U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MAGIC_BEAN); }, "RG_MAGIC_BEAN", false);
	case 875U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{876U});
	case 876U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 877U);
	case 877U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 878U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{879U});
	case 879U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 880U);
	case 880U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 881U: return std::make_unique<AndExpression>(std::vector<index_t>{882U, 883U, 886U, 889U});
	case 882U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 883U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{884U});
	case 884U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 885U);
	case 885U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 886U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{887U});
	case 887U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 888U);
	case 888U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD); }, "RG_KOKIRI_SWORD", false);
	case 889U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{890U});
	case 890U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 891U);
	case 891U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD); }, "RG_DEKU_SHIELD", false);
	case 892U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{893U});
	case 893U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 894U);
	case 894U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CRAWL); }, "RG_CRAWL", false);
	case 895U: return std::make_unique<TernaryExpression>(896U, 897U, 909U);
	case 896U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 897U: return std::make_unique<FunctionCallExpression>("can_climb_ladder", std::vector<index_t>{}, 898U);
	case 898U: return std::make_unique<OrExpression>(std::vector<index_t>{899U, 902U});
	case 899U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{900U});
	case 900U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 901U);
	case 901U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 902U: return std::make_unique<AndExpression>(std::vector<index_t>{903U, 906U});
	case 903U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{904U});
	case 904U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 905U);
	case 905U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_HOOKSHOT_LADDERS); }, "RT_HOOKSHOT_LADDERS", false);
	case 906U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{907U});
	case 907U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 908U);
	case 908U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 909U: return std::make_unique<OrExpression>(std::vector<index_t>{910U, 913U});
	case 910U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{911U});
	case 911U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 912U);
	case 912U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 913U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{914U});
	case 914U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 915U);
	case 915U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 916U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 917U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 918U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 919U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 920U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 921U: return std::make_unique<OrExpression>(std::vector<index_t>{922U, 925U, 930U});
	case 922U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{923U});
	case 923U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 924U);
	case 924U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD); }, "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", false);
	case 925U: return std::make_unique<ComparisonExpression>("==", 926U, 929U);
	case 926U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{927U});
	case 927U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 928U);
	case 928U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_FOREST); }, "RSK_FOREST", false);
	case 929U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_CLOSED_FOREST_OFF); }, "RO_CLOSED_FOREST_OFF", false);
	case 930U: return std::make_unique<AndExpression>(std::vector<index_t>{931U, 932U});
	case 931U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 932U: return std::make_unique<OrExpression>(std::vector<index_t>{933U, 940U});
	case 933U: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{934U, 936U, 938U}, 466U);
	case 934U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 935U);
	case 935U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_BIG_SKULLTULA); }, "RE_BIG_SKULLTULA", false);
	case 936U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 937U);
	case 937U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 938U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 939U);
	case 939U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 940U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{941U});
	case 941U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 942U);
	case 942U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_FOREST_TEMPLE_CLEAR); }, "LOGIC_FOREST_TEMPLE_CLEAR", false);
	case 943U: return std::make_unique<OrExpression>(std::vector<index_t>{944U, 947U, 950U});
	case 944U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{945U});
	case 945U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 946U);
	case 946U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 947U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{948U});
	case 948U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 949U);
	case 949U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 950U: return std::make_unique<AndExpression>(std::vector<index_t>{951U, 952U});
	case 951U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 952U: return std::make_unique<OrExpression>(std::vector<index_t>{953U, 958U});
	case 953U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{954U, 956U});
	case 954U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 955U);
	case 955U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 956U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 957U);
	case 957U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 958U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{959U});
	case 959U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 960U);
	case 960U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_UNINTUITIVE_JUMPS); }, "RT_UNINTUITIVE_JUMPS", false);
	case 961U: return std::make_unique<AndExpression>(std::vector<index_t>{962U, 963U});
	case 962U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 963U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{964U, 966U});
	case 964U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 965U);
	case 965U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 966U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 967U);
	case 967U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 968U: return std::make_unique<OrExpression>(std::vector<index_t>{969U, 970U, 975U});
	case 969U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 970U: return std::make_unique<ComparisonExpression>("!=", 971U, 974U);
	case 971U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{972U});
	case 972U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 973U);
	case 973U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_FOREST); }, "RSK_FOREST", false);
	case 974U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_CLOSED_FOREST_ON); }, "RO_CLOSED_FOREST_ON", false);
	case 975U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{976U});
	case 976U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 977U);
	case 977U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR); }, "LOGIC_DEKU_TREE_CLEAR", false);
	case 978U: return std::make_unique<AndExpression>(std::vector<index_t>{979U, 980U});
	case 979U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 980U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{981U});
	case 981U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 982U);
	case 982U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_OPEN_CHEST); }, "RG_OPEN_CHEST", false);
	case 983U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{984U});
	case 984U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 985U);
	case 985U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CRAWL); }, "RG_CRAWL", false);
	case 986U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 158U);
	case 987U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 174U);
	case 988U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 860U);
	case 989U: return std::make_unique<AndExpression>(std::vector<index_t>{990U, 991U, 994U, 997U});
	case 990U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 991U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{992U});
	case 992U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 993U);
	case 993U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 994U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{995U});
	case 995U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 996U);
	case 996U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD); }, "RG_KOKIRI_SWORD", false);
	case 997U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{998U});
	case 998U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 999U);
	case 999U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD); }, "RG_DEKU_SHIELD", false);
	case 1000U: return std::make_unique<OrExpression>(std::vector<index_t>{1001U, 1002U});
	case 1001U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1002U: return std::make_unique<AndExpression>(std::vector<index_t>{1003U, 1008U});
	case 1003U: return std::make_unique<ComparisonExpression>("!=", 1004U, 1007U);
	case 1004U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1005U});
	case 1005U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1006U);
	case 1006U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SHUFFLE_DUNGEON_ENTRANCES); }, "RSK_SHUFFLE_DUNGEON_ENTRANCES", false);
	case 1007U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF); }, "RO_DUNGEON_ENTRANCE_SHUFFLE_OFF", false);
	case 1008U: return std::make_unique<OrExpression>(std::vector<index_t>{1009U, 1014U});
	case 1009U: return std::make_unique<ComparisonExpression>("==", 1010U, 1013U);
	case 1010U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1011U});
	case 1011U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1012U);
	case 1012U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_FOREST); }, "RSK_FOREST", false);
	case 1013U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_CLOSED_FOREST_OFF); }, "RO_CLOSED_FOREST_OFF", false);
	case 1014U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{1015U});
	case 1015U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1016U);
	case 1016U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD); }, "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", false);
	case 1017U: return std::make_unique<OrExpression>(std::vector<index_t>{1018U, 1031U, 1036U});
	case 1018U: return std::make_unique<AndExpression>(std::vector<index_t>{1019U, 1020U});
	case 1019U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1020U: return std::make_unique<OrExpression>(std::vector<index_t>{1021U, 1028U});
	case 1021U: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{1022U, 1024U, 1026U}, 466U);
	case 1022U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 1023U);
	case 1023U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_BIG_SKULLTULA); }, "RE_BIG_SKULLTULA", false);
	case 1024U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 1025U);
	case 1025U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 1026U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 1027U);
	case 1027U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1028U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{1029U});
	case 1029U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1030U);
	case 1030U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR); }, "LOGIC_DEKU_TREE_CLEAR", false);
	case 1031U: return std::make_unique<ComparisonExpression>("==", 1032U, 1035U);
	case 1032U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1033U});
	case 1033U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1034U);
	case 1034U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_FOREST); }, "RSK_FOREST", false);
	case 1035U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_CLOSED_FOREST_OFF); }, "RO_CLOSED_FOREST_OFF", false);
	case 1036U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{1037U});
	case 1037U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1038U);
	case 1038U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD); }, "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", false);
	case 1039U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1040U});
	case 1040U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1041U);
	case 1041U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_OPEN_CHEST); }, "RG_OPEN_CHEST", false);
	case 1042U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1043U});
	case 1043U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1044U);
	case 1044U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_OPEN_CHEST); }, "RG_OPEN_CHEST", false);
	case 1045U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1046U});
	case 1046U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1047U);
	case 1047U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_OPEN_CHEST); }, "RG_OPEN_CHEST", false);
	case 1048U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1049U});
	case 1049U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1050U);
	case 1050U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_OPEN_CHEST); }, "RG_OPEN_CHEST", false);
	case 1051U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1052U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1053U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1054U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1055U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1056U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1057U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1058U: return std::make_unique<AndExpression>(std::vector<index_t>{1059U, 1060U});
	case 1059U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1060U: return std::make_unique<OrExpression>(std::vector<index_t>{1061U, 1066U});
	case 1061U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1062U, 1064U});
	case 1062U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1063U);
	case 1063U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1064U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1065U);
	case 1065U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1066U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1067U});
	case 1067U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1068U);
	case 1068U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1069U: return std::make_unique<FunctionCallExpression>("can_open_storms_grotto", std::vector<index_t>{}, 1070U);
	case 1070U: return std::make_unique<AndExpression>(std::vector<index_t>{1071U, 1074U});
	case 1071U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1072U});
	case 1072U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1073U);
	case 1073U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1074U: return std::make_unique<OrExpression>(std::vector<index_t>{1075U, 1078U});
	case 1075U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1076U});
	case 1076U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1077U);
	case 1077U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STONE_OF_AGONY); }, "RG_STONE_OF_AGONY", false);
	case 1078U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{1079U});
	case 1079U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1080U);
	case 1080U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_GROTTOS_WITHOUT_AGONY); }, "RT_GROTTOS_WITHOUT_AGONY", false);
	case 1081U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1082U: return std::make_unique<OrExpression>(std::vector<index_t>{1083U, 1089U});
	case 1083U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy", std::vector<index_t>{}, 1084U);
	case 1084U: return std::make_unique<OrExpression>(std::vector<index_t>{1085U, 1086U});
	case 1085U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 860U);
	case 1086U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1087U});
	case 1087U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1088U);
	case 1088U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SUNS_SONG); }, "RG_SUNS_SONG", false);
	case 1089U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1090U});
	case 1090U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1091U);
	case 1091U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 1092U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1093U: return std::make_unique<OrExpression>(std::vector<index_t>{1094U, 1097U, 1100U, 1101U, 1104U, 1107U, 1110U, 1113U});
	case 1094U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1095U});
	case 1095U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1096U);
	case 1096U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD); }, "RG_KOKIRI_SWORD", false);
	case 1097U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1098U});
	case 1098U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1099U);
	case 1099U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1100U: return std::make_unique<FunctionCallExpression>("has_explosives", std::vector<index_t>{}, 193U);
	case 1101U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1102U});
	case 1102U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1103U);
	case 1103U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_GORONS_BRACELET); }, "RG_GORONS_BRACELET", false);
	case 1104U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1105U});
	case 1105U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1106U);
	case 1106U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MASTER_SWORD); }, "RG_MASTER_SWORD", false);
	case 1107U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1108U});
	case 1108U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1109U);
	case 1109U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 1110U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1111U});
	case 1111U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1112U);
	case 1112U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BIGGORON_SWORD); }, "RG_BIGGORON_SWORD", false);
	case 1113U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1114U});
	case 1114U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1115U);
	case 1115U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_GIANTS_KNIFE); }, "RG_GIANTS_KNIFE", false);
	case 1116U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1117U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1118U});
	case 1118U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1119U);
	case 1119U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_OPEN_CHEST); }, "RG_OPEN_CHEST", false);
	case 1120U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1121U: return std::make_unique<ComparisonExpression>("==", 1122U, 1125U);
	case 1122U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1123U});
	case 1123U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1124U);
	case 1124U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_KAK_GATE); }, "RSK_KAK_GATE", false);
	case 1125U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_KAK_GATE_OPEN); }, "RO_KAK_GATE_OPEN", false);
	case 1126U: return std::make_unique<ComparisonExpression>("==", 1127U, 1130U);
	case 1127U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1128U});
	case 1128U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1129U);
	case 1129U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1130U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FREE); }, "RO_GF_CARPENTERS_FREE", false);
	case 1131U: return std::make_unique<OrExpression>(std::vector<index_t>{1132U, 1137U});
	case 1132U: return std::make_unique<ComparisonExpression>("==", 1133U, 1136U);
	case 1133U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1134U});
	case 1134U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1135U);
	case 1135U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1136U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FREE); }, "RO_GF_CARPENTERS_FREE", false);
	case 1137U: return std::make_unique<ComparisonExpression>("==", 1138U, 1141U);
	case 1138U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1139U});
	case 1139U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1140U);
	case 1140U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1141U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FAST); }, "RO_GF_CARPENTERS_FAST", false);
	case 1142U: return std::make_unique<OrExpression>(std::vector<index_t>{1143U, 1148U});
	case 1143U: return std::make_unique<ComparisonExpression>("==", 1144U, 1147U);
	case 1144U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1145U});
	case 1145U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1146U);
	case 1146U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1147U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FREE); }, "RO_GF_CARPENTERS_FREE", false);
	case 1148U: return std::make_unique<ComparisonExpression>("==", 1149U, 1152U);
	case 1149U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1150U});
	case 1150U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1151U);
	case 1151U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1152U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FAST); }, "RO_GF_CARPENTERS_FAST", false);
	case 1153U: return std::make_unique<OrExpression>(std::vector<index_t>{1154U, 1159U});
	case 1154U: return std::make_unique<ComparisonExpression>("==", 1155U, 1158U);
	case 1155U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1156U});
	case 1156U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1157U);
	case 1157U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1158U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FREE); }, "RO_GF_CARPENTERS_FREE", false);
	case 1159U: return std::make_unique<ComparisonExpression>("==", 1160U, 1163U);
	case 1160U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1161U});
	case 1161U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1162U);
	case 1162U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1163U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FAST); }, "RO_GF_CARPENTERS_FAST", false);
	case 1164U: return std::make_unique<ComparisonExpression>("==", 1165U, 1168U);
	case 1165U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1166U});
	case 1166U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1167U);
	case 1167U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 1168U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FREE); }, "RO_GF_CARPENTERS_FREE", false);
	case 1169U: return std::make_unique<ComparisonExpression>("==", 1170U, 1173U);
	case 1170U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1171U});
	case 1171U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1172U);
	case 1172U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKIP_EPONA_RACE); }, "RSK_SKIP_EPONA_RACE", false);
	case 1173U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GENERIC_YES); }, "RO_GENERIC_YES", false);
	case 1174U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1175U: return std::make_unique<ComparisonExpression>(">=", 1176U, 1177U);
	case 1176U: return std::make_unique<ExternFunctionCallExpression>(collected_triforce_pieces_adapter, "collected_triforce_pieces", std::vector<index_t>{});
	case 1177U: return std::make_unique<ExternFunctionCallExpression>(required_triforce_pieces_adapter, "required_triforce_pieces", std::vector<index_t>{});
	case 1178U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1179U});
	case 1179U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1180U);
	case 1180U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SARIAS_SONG); }, "RG_SARIAS_SONG", false);
	case 1181U: return std::make_unique<ComparisonExpression>("==", 1182U, 1185U);
	case 1182U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1183U});
	case 1183U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1184U);
	case 1184U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKIP_CHILD_ZELDA); }, "RSK_SKIP_CHILD_ZELDA", false);
	case 1185U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GENERIC_YES); }, "RO_GENERIC_YES", false);
	case 1186U: return std::make_unique<ComparisonExpression>("==", 1187U, 1190U);
	case 1187U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1188U});
	case 1188U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1189U);
	case 1189U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKIP_CHILD_ZELDA); }, "RSK_SKIP_CHILD_ZELDA", false);
	case 1190U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GENERIC_YES); }, "RO_GENERIC_YES", false);
	case 1191U: return std::make_unique<ComparisonExpression>("==", 1192U, 1195U);
	case 1192U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1193U});
	case 1193U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1194U);
	case 1194U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKIP_CHILD_ZELDA); }, "RSK_SKIP_CHILD_ZELDA", false);
	case 1195U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GENERIC_YES); }, "RO_GENERIC_YES", false);
	case 1196U: return std::make_unique<ComparisonExpression>("==", 1197U, 1200U);
	case 1197U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1198U});
	case 1198U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1199U);
	case 1199U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SELECTED_STARTING_AGE); }, "RSK_SELECTED_STARTING_AGE", false);
	case 1200U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_AGE_ADULT); }, "RO_AGE_ADULT", false);
	case 1201U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1202U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1203U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1204U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1205U});
	case 1205U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1206U);
	case 1206U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MINUET_OF_FOREST); }, "RG_MINUET_OF_FOREST", false);
	case 1207U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1208U});
	case 1208U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1209U);
	case 1209U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOLERO_OF_FIRE); }, "RG_BOLERO_OF_FIRE", false);
	case 1210U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1211U});
	case 1211U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1212U);
	case 1212U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SERENADE_OF_WATER); }, "RG_SERENADE_OF_WATER", false);
	case 1213U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1214U});
	case 1214U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1215U);
	case 1215U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_NOCTURNE_OF_SHADOW); }, "RG_NOCTURNE_OF_SHADOW", false);
	case 1216U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1217U});
	case 1217U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1218U);
	case 1218U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_REQUIEM_OF_SPIRIT); }, "RG_REQUIEM_OF_SPIRIT", false);
	case 1219U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1220U});
	case 1220U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1221U);
	case 1221U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_PRELUDE_OF_LIGHT); }, "RG_PRELUDE_OF_LIGHT", false);
	case 1222U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1223U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1224U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1225U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1226U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1227U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1228U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1229U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1230U: return std::make_unique<FunctionCallExpression>("can_break_lower_beehives", std::vector<index_t>{}, 1231U);
	case 1231U: return std::make_unique<OrExpression>(std::vector<index_t>{1232U, 1261U});
	case 1232U: return std::make_unique<FunctionCallExpression>("can_break_upper_beehives", std::vector<index_t>{}, 1233U);
	case 1233U: return std::make_unique<OrExpression>(std::vector<index_t>{1234U, 1242U, 1249U});
	case 1234U: return std::make_unique<FunctionCallExpression>("hookshot_or_boomerang", std::vector<index_t>{}, 1235U);
	case 1235U: return std::make_unique<OrExpression>(std::vector<index_t>{1236U, 1239U});
	case 1236U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1237U});
	case 1237U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1238U);
	case 1238U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 1239U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1240U});
	case 1240U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1241U);
	case 1241U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1242U: return std::make_unique<AndExpression>(std::vector<index_t>{1243U, 1246U});
	case 1243U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{1244U});
	case 1244U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1245U);
	case 1245U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_BOMBCHU_BEEHIVES); }, "RT_BOMBCHU_BEEHIVES", false);
	case 1246U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1247U});
	case 1247U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1248U);
	case 1248U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMBCHU_5); }, "RG_BOMBCHU_5", false);
	case 1249U: return std::make_unique<AndExpression>(std::vector<index_t>{1250U, 1254U});
	case 1250U: return std::make_unique<ConversionExpression<bool>>(1251U);
	case 1251U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1252U});
	case 1252U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1253U);
	case 1253U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SLINGBOW_BREAK_BEEHIVES); }, "RSK_SLINGBOW_BREAK_BEEHIVES", false);
	case 1254U: return std::make_unique<OrExpression>(std::vector<index_t>{1255U, 1258U});
	case 1255U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1256U});
	case 1256U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1257U);
	case 1257U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 1258U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1259U});
	case 1259U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1260U);
	case 1260U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_SLINGSHOT); }, "RG_FAIRY_SLINGSHOT", false);
	case 1261U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1262U});
	case 1262U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1263U);
	case 1263U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMB_BAG); }, "RG_BOMB_BAG", false);
	case 1264U: return std::make_unique<FunctionCallExpression>("can_break_lower_beehives", std::vector<index_t>{}, 1231U);
	case 1265U: return std::make_unique<AndExpression>(std::vector<index_t>{1266U, 1267U, 1270U});
	case 1266U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1267U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1268U});
	case 1268U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1269U);
	case 1269U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_EPONAS_SONG); }, "RG_EPONAS_SONG", false);
	case 1270U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{1271U});
	case 1271U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1272U);
	case 1272U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_LINKS_COW); }, "LOGIC_LINKS_COW", false);
	case 1273U: return std::make_unique<AndExpression>(std::vector<index_t>{1274U, 1275U, 1278U, 1281U});
	case 1274U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1275U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1276U});
	case 1276U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1277U);
	case 1277U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MAGIC_BEAN); }, "RG_MAGIC_BEAN", false);
	case 1278U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1279U});
	case 1279U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1280U);
	case 1280U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1281U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1282U});
	case 1282U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1283U);
	case 1283U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1284U: return std::make_unique<AndExpression>(std::vector<index_t>{1285U, 1286U, 1289U, 1292U});
	case 1285U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1286U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1287U});
	case 1287U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1288U);
	case 1288U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MAGIC_BEAN); }, "RG_MAGIC_BEAN", false);
	case 1289U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1290U});
	case 1290U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1291U);
	case 1291U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1292U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1293U});
	case 1293U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1294U);
	case 1294U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1295U: return std::make_unique<AndExpression>(std::vector<index_t>{1296U, 1297U, 1300U, 1303U});
	case 1296U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1297U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1298U});
	case 1298U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1299U);
	case 1299U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MAGIC_BEAN); }, "RG_MAGIC_BEAN", false);
	case 1300U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1301U});
	case 1301U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1302U);
	case 1302U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1303U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1304U});
	case 1304U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1305U);
	case 1305U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1306U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 860U);
	case 1307U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1308U});
	case 1308U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1309U);
	case 1309U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1310U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 860U);
	case 1311U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1312U});
	case 1312U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1313U);
	case 1313U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1314U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 860U);
	case 1315U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1316U});
	case 1316U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1317U);
	case 1317U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1318U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy", std::vector<index_t>{}, 1084U);
	case 1319U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1320U});
	case 1320U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1321U);
	case 1321U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 1322U: return std::make_unique<FunctionCallExpression>("has_bottle", std::vector<index_t>{}, 1323U);
	case 1323U: return std::make_unique<ComparisonExpression>(">=", 1324U, 1325U);
	case 1324U: return std::make_unique<ExternFunctionCallExpression>(bottle_count_adapter, "bottle_count", std::vector<index_t>{});
	case 1325U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 1326U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1327U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1328U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1329U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1330U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1331U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1332U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1333U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1334U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1335U: return std::make_unique<AndExpression>(std::vector<index_t>{1336U, 1337U});
	case 1336U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1337U: return std::make_unique<OrExpression>(std::vector<index_t>{1338U, 1343U, 1346U});
	case 1338U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1339U, 1341U});
	case 1339U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1340U);
	case 1340U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1341U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1342U);
	case 1342U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1343U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1344U});
	case 1344U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1345U);
	case 1345U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1346U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1347U});
	case 1347U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1348U);
	case 1348U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1349U: return std::make_unique<AndExpression>(std::vector<index_t>{1350U, 1351U});
	case 1350U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1351U: return std::make_unique<OrExpression>(std::vector<index_t>{1352U, 1357U, 1360U});
	case 1352U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1353U, 1355U});
	case 1353U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1354U);
	case 1354U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1355U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1356U);
	case 1356U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1357U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1358U});
	case 1358U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1359U);
	case 1359U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1360U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1361U});
	case 1361U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1362U);
	case 1362U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1363U: return std::make_unique<AndExpression>(std::vector<index_t>{1364U, 1365U});
	case 1364U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1365U: return std::make_unique<OrExpression>(std::vector<index_t>{1366U, 1371U, 1374U});
	case 1366U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1367U, 1369U});
	case 1367U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1368U);
	case 1368U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1369U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1370U);
	case 1370U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1371U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1372U});
	case 1372U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1373U);
	case 1373U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1374U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1375U});
	case 1375U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1376U);
	case 1376U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1377U: return std::make_unique<AndExpression>(std::vector<index_t>{1378U, 1379U});
	case 1378U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1379U: return std::make_unique<OrExpression>(std::vector<index_t>{1380U, 1385U, 1388U});
	case 1380U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1381U, 1383U});
	case 1381U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1382U);
	case 1382U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1383U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1384U);
	case 1384U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1385U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1386U});
	case 1386U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1387U);
	case 1387U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1388U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1389U});
	case 1389U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1390U);
	case 1390U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1391U: return std::make_unique<AndExpression>(std::vector<index_t>{1392U, 1393U});
	case 1392U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1393U: return std::make_unique<OrExpression>(std::vector<index_t>{1394U, 1399U, 1402U});
	case 1394U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1395U, 1397U});
	case 1395U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1396U);
	case 1396U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1397U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1398U);
	case 1398U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1399U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1400U});
	case 1400U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1401U);
	case 1401U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1402U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1403U});
	case 1403U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1404U);
	case 1404U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1405U: return std::make_unique<AndExpression>(std::vector<index_t>{1406U, 1407U});
	case 1406U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1407U: return std::make_unique<OrExpression>(std::vector<index_t>{1408U, 1413U, 1416U});
	case 1408U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1409U, 1411U});
	case 1409U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1410U);
	case 1410U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1411U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1412U);
	case 1412U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1413U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1414U});
	case 1414U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1415U);
	case 1415U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1416U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1417U});
	case 1417U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1418U);
	case 1418U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1419U: return std::make_unique<AndExpression>(std::vector<index_t>{1420U, 1421U});
	case 1420U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1421U: return std::make_unique<OrExpression>(std::vector<index_t>{1422U, 1427U, 1430U});
	case 1422U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{1423U, 1425U});
	case 1423U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 1424U);
	case 1424U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 1425U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 1426U);
	case 1426U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1427U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1428U});
	case 1428U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1429U);
	case 1429U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1430U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1431U});
	case 1431U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1432U);
	case 1432U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1433U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1434U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1435U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1436U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1437U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1438U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1439U: return std::make_unique<AndExpression>(std::vector<index_t>{1440U, 1441U});
	case 1440U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1441U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1442U});
	case 1442U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1443U);
	case 1443U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1444U: return std::make_unique<AndExpression>(std::vector<index_t>{1445U, 1446U});
	case 1445U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1446U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1447U});
	case 1447U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1448U);
	case 1448U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1449U: return std::make_unique<AndExpression>(std::vector<index_t>{1450U, 1451U});
	case 1450U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1451U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1452U});
	case 1452U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1453U);
	case 1453U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1454U: return std::make_unique<AndExpression>(std::vector<index_t>{1455U, 1456U});
	case 1455U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1456U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1457U});
	case 1457U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1458U);
	case 1458U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1459U: return std::make_unique<AndExpression>(std::vector<index_t>{1460U, 1461U});
	case 1460U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1461U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1462U});
	case 1462U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1463U);
	case 1463U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1464U: return std::make_unique<AndExpression>(std::vector<index_t>{1465U, 1466U});
	case 1465U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1466U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1467U});
	case 1467U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1468U);
	case 1468U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1469U: return std::make_unique<AndExpression>(std::vector<index_t>{1470U, 1471U});
	case 1470U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1471U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1472U});
	case 1472U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1473U);
	case 1473U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1474U: return std::make_unique<AndExpression>(std::vector<index_t>{1475U, 1476U});
	case 1475U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1476U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1477U});
	case 1477U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1478U);
	case 1478U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1479U: return std::make_unique<AndExpression>(std::vector<index_t>{1480U, 1481U});
	case 1480U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1481U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1482U});
	case 1482U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1483U);
	case 1483U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1484U: return std::make_unique<AndExpression>(std::vector<index_t>{1485U, 1486U});
	case 1485U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1486U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1487U});
	case 1487U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1488U);
	case 1488U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1489U: return std::make_unique<AndExpression>(std::vector<index_t>{1490U, 1491U});
	case 1490U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1491U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1492U});
	case 1492U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1493U);
	case 1493U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1494U: return std::make_unique<AndExpression>(std::vector<index_t>{1495U, 1496U});
	case 1495U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1496U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1497U});
	case 1497U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1498U);
	case 1498U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1499U: return std::make_unique<AndExpression>(std::vector<index_t>{1500U, 1501U});
	case 1500U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1501U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1502U});
	case 1502U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1503U);
	case 1503U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1504U: return std::make_unique<AndExpression>(std::vector<index_t>{1505U, 1506U});
	case 1505U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1506U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1507U});
	case 1507U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1508U);
	case 1508U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1509U: return std::make_unique<AndExpression>(std::vector<index_t>{1510U, 1511U, 1524U});
	case 1510U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1511U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{1512U, 1514U, 1516U, 1518U, 1520U, 1522U}, 19U);
	case 1512U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 1513U);
	case 1513U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 1514U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 1515U);
	case 1515U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 1516U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 1517U);
	case 1517U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1518U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 1519U);
	case 1519U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 1520U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 1521U);
	case 1521U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1522U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 1523U);
	case 1523U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1524U: return std::make_unique<FunctionCallExpression>("can_get_night_time_gs", std::vector<index_t>{}, 1525U);
	case 1525U: return std::make_unique<AndExpression>(std::vector<index_t>{1526U, 1527U});
	case 1526U: return std::make_unique<ExternFunctionCallExpression>(at_night_adapter, "at_night", std::vector<index_t>{});
	case 1527U: return std::make_unique<OrExpression>(std::vector<index_t>{1528U, 1531U});
	case 1528U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1529U});
	case 1529U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1530U);
	case 1530U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SUNS_SONG); }, "RG_SUNS_SONG", false);
	case 1531U: return std::make_unique<NotExpression>(1532U);
	case 1532U: return std::make_unique<ConversionExpression<bool>>(1533U);
	case 1533U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{1534U});
	case 1534U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1535U);
	case 1535U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKULLS_SUNS_SONG); }, "RSK_SKULLS_SUNS_SONG", false);
	case 1536U: return std::make_unique<AndExpression>(std::vector<index_t>{1537U, 1548U});
	case 1537U: return std::make_unique<FunctionCallExpression>("can_spawn_soil_skull", std::vector<index_t>{1538U}, 1540U);
	case 1538U: return std::make_unique<ParameterExpression>(uint8_t{0}, "bean", 1539U);
	case 1539U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 1540U: return std::make_unique<AndExpression>(std::vector<index_t>{1541U, 1542U, 1545U});
	case 1541U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1542U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1543U});
	case 1543U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1544U);
	case 1544U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOTTLE_WITH_BUGS); }, "RG_BOTTLE_WITH_BUGS", false);
	case 1545U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1546U});
	case 1546U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1547U);
	case 1547U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("bean"); }, "bean", false);
	case 1548U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{1549U, 1551U, 1553U, 1555U, 1557U, 1559U}, 19U);
	case 1549U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 1550U);
	case 1550U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 1551U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 1552U);
	case 1552U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 1553U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 1554U);
	case 1554U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1555U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 1556U);
	case 1556U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 1557U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 1558U);
	case 1558U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1559U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 1560U);
	case 1560U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1561U: return std::make_unique<AndExpression>(std::vector<index_t>{1562U, 1563U, 1564U});
	case 1562U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1563U: return std::make_unique<FunctionCallExpression>("can_get_night_time_gs", std::vector<index_t>{}, 1525U);
	case 1564U: return std::make_unique<OrExpression>(std::vector<index_t>{1565U, 1686U});
	case 1565U: return std::make_unique<FunctionCallExpression>("can_get_drop", std::vector<index_t>{1566U, 1568U, 1570U}, 1572U);
	case 1566U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 1567U);
	case 1567U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 1568U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 1569U);
	case 1569U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOOMERANG); }, "ED_BOOMERANG", false);
	case 1570U: return std::make_unique<ParameterExpression>(uint8_t{2}, "above_link", 1571U);
	case 1571U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1572U: return std::make_unique<AndExpression>(std::vector<index_t>{1573U, 1586U});
	case 1573U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{1574U, 1576U, 1578U, 1580U, 1582U, 1584U}, 19U);
	case 1574U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 1575U);
	case 1575U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("e"); }, "e", false);
	case 1576U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 1577U);
	case 1577U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 1578U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 1579U);
	case 1579U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1580U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 1581U);
	case 1581U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 1582U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 1583U);
	case 1583U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1584U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 1585U);
	case 1585U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1586U: return std::make_unique<OrExpression>(std::vector<index_t>{1587U, 1632U});
	case 1587U: return std::make_unique<ComparisonExpression>("<=", 1588U, 1629U);
	case 1588U: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{1589U}, 1591U);
	case 1589U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 1590U);
	case 1590U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 1591U: return std::make_unique<MatchExpression<int32_t>>(1592U, std::vector<index_t>{1593U, 1597U, 1601U, 1605U, 1609U, 1613U, 1617U, 1621U, 1625U});
	case 1592U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 1593U: return std::make_unique<MatchArm>(1594U, 1596U, false);
	case 1594U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1595U});
	case 1595U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 1596U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{0LL}; }, "0", false);
	case 1597U: return std::make_unique<MatchArm>(1598U, 1600U, false);
	case 1598U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1599U});
	case 1599U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH); }, "ED_SHORT_JUMPSLASH", false);
	case 1600U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 1601U: return std::make_unique<MatchArm>(1602U, 1604U, false);
	case 1602U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1603U});
	case 1603U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_MASTER_SWORD_JUMPSLASH); }, "ED_MASTER_SWORD_JUMPSLASH", false);
	case 1604U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{2LL}; }, "2", false);
	case 1605U: return std::make_unique<MatchArm>(1606U, 1608U, false);
	case 1606U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1607U});
	case 1607U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONG_JUMPSLASH); }, "ED_LONG_JUMPSLASH", false);
	case 1608U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{3LL}; }, "3", false);
	case 1609U: return std::make_unique<MatchArm>(1610U, 1612U, false);
	case 1610U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1611U});
	case 1611U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOMB_THROW); }, "ED_BOMB_THROW", false);
	case 1612U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{4LL}; }, "4", false);
	case 1613U: return std::make_unique<MatchArm>(1614U, 1616U, false);
	case 1614U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1615U});
	case 1615U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOOMERANG); }, "ED_BOOMERANG", false);
	case 1616U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{5LL}; }, "5", false);
	case 1617U: return std::make_unique<MatchArm>(1618U, 1620U, false);
	case 1618U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1619U});
	case 1619U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_HOOKSHOT); }, "ED_HOOKSHOT", false);
	case 1620U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{6LL}; }, "6", false);
	case 1621U: return std::make_unique<MatchArm>(1622U, 1624U, false);
	case 1622U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1623U});
	case 1623U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONGSHOT); }, "ED_LONGSHOT", false);
	case 1624U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{7LL}; }, "7", false);
	case 1625U: return std::make_unique<MatchArm>(1626U, 1628U, false);
	case 1626U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1627U});
	case 1627U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_FAR); }, "ED_FAR", false);
	case 1628U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{8LL}; }, "8", false);
	case 1629U: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{1630U}, 1591U);
	case 1630U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 1631U);
	case 1631U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_MASTER_SWORD_JUMPSLASH); }, "ED_MASTER_SWORD_JUMPSLASH", false);
	case 1632U: return std::make_unique<MatchExpression<bool>>(1633U, std::vector<index_t>{1634U, 1665U, 1671U});
	case 1633U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("e"); }, "e", false);
	case 1634U: return std::make_unique<MatchArm>(1635U, 1637U, false);
	case 1635U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1636U});
	case 1636U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 1637U: return std::make_unique<FunctionCallExpression>("_can_get_drop_gold_skulltula", std::vector<index_t>{1638U}, 1640U);
	case 1638U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 1639U);
	case 1639U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 1640U: return std::make_unique<MatchExpression<bool>>(1641U, std::vector<index_t>{1642U, 1653U, 1659U});
	case 1641U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 1642U: return std::make_unique<MatchArm>(1643U, 1650U, true);
	case 1643U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1644U, 1645U, 1646U, 1647U, 1648U, 1649U});
	case 1644U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 1645U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH); }, "ED_SHORT_JUMPSLASH", false);
	case 1646U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_MASTER_SWORD_JUMPSLASH); }, "ED_MASTER_SWORD_JUMPSLASH", false);
	case 1647U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONG_JUMPSLASH); }, "ED_LONG_JUMPSLASH", false);
	case 1648U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOMB_THROW); }, "ED_BOMB_THROW", false);
	case 1649U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOOMERANG); }, "ED_BOOMERANG", false);
	case 1650U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1651U});
	case 1651U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1652U);
	case 1652U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1653U: return std::make_unique<MatchArm>(1654U, 1656U, true);
	case 1654U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1655U});
	case 1655U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_HOOKSHOT); }, "ED_HOOKSHOT", false);
	case 1656U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1657U});
	case 1657U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1658U);
	case 1658U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 1659U: return std::make_unique<MatchArm>(1660U, 1662U, false);
	case 1660U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1661U});
	case 1661U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONGSHOT); }, "ED_LONGSHOT", false);
	case 1662U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1663U});
	case 1663U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1664U);
	case 1664U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 1665U: return std::make_unique<MatchArm>(1666U, 1670U, false);
	case 1666U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{1667U, 1668U, 1669U});
	case 1667U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_KEESE); }, "RE_KEESE", false);
	case 1668U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_FIRE_KEESE); }, "RE_FIRE_KEESE", false);
	case 1669U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GUAY); }, "RE_GUAY", false);
	case 1670U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1671U: return std::make_unique<MatchArm>(1672U, 1673U, false);
	case 1672U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{});
	case 1673U: return std::make_unique<OrExpression>(std::vector<index_t>{1674U, 1675U});
	case 1674U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("above_link"); }, "above_link", false);
	case 1675U: return std::make_unique<AndExpression>(std::vector<index_t>{1676U, 1683U});
	case 1676U: return std::make_unique<ComparisonExpression>("<=", 1677U, 1680U);
	case 1677U: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{1678U}, 1591U);
	case 1678U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 1679U);
	case 1679U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 1680U: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{1681U}, 1591U);
	case 1681U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 1682U);
	case 1682U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOOMERANG); }, "ED_BOOMERANG", false);
	case 1683U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1684U});
	case 1684U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1685U);
	case 1685U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 1686U: return std::make_unique<AndExpression>(std::vector<index_t>{1687U, 1690U, 1693U});
	case 1687U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{1688U});
	case 1688U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 1689U);
	case 1689U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_KF_ADULT_GS); }, "RT_KF_ADULT_GS", false);
	case 1690U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{1691U});
	case 1691U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1692U);
	case 1692U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 1693U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{1694U, 1696U, 1698U, 1700U, 1702U, 1704U}, 19U);
	case 1694U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 1695U);
	case 1695U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 1696U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 1697U);
	case 1697U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH); }, "ED_SHORT_JUMPSLASH", false);
	case 1698U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 1699U);
	case 1699U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1700U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 1701U);
	case 1701U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 1702U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 1703U);
	case 1703U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1704U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 1705U);
	case 1705U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 1706U: return std::make_unique<AndExpression>(std::vector<index_t>{1707U, 1708U});
	case 1707U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1708U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1709U: return std::make_unique<AndExpression>(std::vector<index_t>{1710U, 1711U});
	case 1710U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1711U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1712U: return std::make_unique<AndExpression>(std::vector<index_t>{1713U, 1714U});
	case 1713U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1714U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1715U: return std::make_unique<AndExpression>(std::vector<index_t>{1716U, 1717U});
	case 1716U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1717U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1718U: return std::make_unique<AndExpression>(std::vector<index_t>{1719U, 1720U});
	case 1719U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1720U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1721U: return std::make_unique<AndExpression>(std::vector<index_t>{1722U, 1723U});
	case 1722U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1723U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1724U: return std::make_unique<AndExpression>(std::vector<index_t>{1725U, 1726U});
	case 1725U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1726U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1727U: return std::make_unique<AndExpression>(std::vector<index_t>{1728U, 1729U});
	case 1728U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1729U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1730U: return std::make_unique<AndExpression>(std::vector<index_t>{1731U, 1732U});
	case 1731U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1732U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1733U: return std::make_unique<AndExpression>(std::vector<index_t>{1734U, 1735U});
	case 1734U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1735U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1736U: return std::make_unique<AndExpression>(std::vector<index_t>{1737U, 1738U});
	case 1737U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1738U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1739U: return std::make_unique<AndExpression>(std::vector<index_t>{1740U, 1741U});
	case 1740U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1741U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1742U: return std::make_unique<AndExpression>(std::vector<index_t>{1743U, 1744U});
	case 1743U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1744U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1745U: return std::make_unique<AndExpression>(std::vector<index_t>{1746U, 1747U});
	case 1746U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1747U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1748U: return std::make_unique<AndExpression>(std::vector<index_t>{1749U, 1750U});
	case 1749U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1750U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1751U: return std::make_unique<AndExpression>(std::vector<index_t>{1752U, 1753U});
	case 1752U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1753U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1754U: return std::make_unique<AndExpression>(std::vector<index_t>{1755U, 1756U});
	case 1755U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1756U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1757U: return std::make_unique<AndExpression>(std::vector<index_t>{1758U, 1759U});
	case 1758U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1759U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1760U: return std::make_unique<AndExpression>(std::vector<index_t>{1761U, 1762U});
	case 1761U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1762U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1763U: return std::make_unique<AndExpression>(std::vector<index_t>{1764U, 1765U});
	case 1764U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1765U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1766U: return std::make_unique<AndExpression>(std::vector<index_t>{1767U, 1768U});
	case 1767U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1768U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1769U: return std::make_unique<AndExpression>(std::vector<index_t>{1770U, 1771U});
	case 1770U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1771U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1772U: return std::make_unique<AndExpression>(std::vector<index_t>{1773U, 1774U});
	case 1773U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1774U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1775U: return std::make_unique<AndExpression>(std::vector<index_t>{1776U, 1777U});
	case 1776U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1777U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1778U: return std::make_unique<AndExpression>(std::vector<index_t>{1779U, 1780U});
	case 1779U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1780U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1781U: return std::make_unique<AndExpression>(std::vector<index_t>{1782U, 1783U});
	case 1782U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1783U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1784U: return std::make_unique<AndExpression>(std::vector<index_t>{1785U, 1786U});
	case 1785U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1786U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1787U: return std::make_unique<AndExpression>(std::vector<index_t>{1788U, 1789U});
	case 1788U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1789U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1790U: return std::make_unique<AndExpression>(std::vector<index_t>{1791U, 1792U});
	case 1791U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1792U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1793U: return std::make_unique<AndExpression>(std::vector<index_t>{1794U, 1795U});
	case 1794U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1795U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1796U: return std::make_unique<AndExpression>(std::vector<index_t>{1797U, 1798U});
	case 1797U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1798U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1799U: return std::make_unique<AndExpression>(std::vector<index_t>{1800U, 1801U});
	case 1800U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 1801U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1802U: return std::make_unique<AndExpression>(std::vector<index_t>{1803U, 1804U});
	case 1803U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1804U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1805U: return std::make_unique<AndExpression>(std::vector<index_t>{1806U, 1807U});
	case 1806U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1807U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1808U: return std::make_unique<AndExpression>(std::vector<index_t>{1809U, 1810U});
	case 1809U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 1810U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1811U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1812U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1813U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1814U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 1093U);
	case 1815U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1816U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1817U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1818U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 1819U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1820U});
	case 1820U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1821U);
	case 1821U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 1822U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1823U});
	case 1823U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1824U);
	case 1824U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 1825U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1826U});
	case 1826U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1827U);
	case 1827U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 1828U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1829U});
	case 1829U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1830U);
	case 1830U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 1831U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1832U});
	case 1832U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1833U);
	case 1833U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 1834U: return std::make_unique<AndExpression>(std::vector<index_t>{1835U, 1838U});
	case 1835U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1836U});
	case 1836U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1837U);
	case 1837U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1838U: return std::make_unique<ComparisonExpression>("<=", 1839U, 1842U);
	case 1839U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1840U});
	case 1840U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1841U);
	case 1841U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1842U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	case 1843U: return std::make_unique<TernaryExpression>(1844U, 1847U, 1848U);
	case 1844U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1845U});
	case 1845U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1846U);
	case 1846U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_TYCOON_WALLET); }, "RG_TYCOON_WALLET", false);
	case 1847U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{999LL}; }, "999", false);
	case 1848U: return std::make_unique<TernaryExpression>(1849U, 1852U, 1853U);
	case 1849U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1850U});
	case 1850U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1851U);
	case 1851U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_GIANT_WALLET); }, "RG_GIANT_WALLET", false);
	case 1852U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{500LL}; }, "500", false);
	case 1853U: return std::make_unique<TernaryExpression>(1854U, 1857U, 1858U);
	case 1854U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1855U});
	case 1855U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1856U);
	case 1856U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_ADULT_WALLET); }, "RG_ADULT_WALLET", false);
	case 1857U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{200LL}; }, "200", false);
	case 1858U: return std::make_unique<TernaryExpression>(1859U, 1862U, 1863U);
	case 1859U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1860U});
	case 1860U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1861U);
	case 1861U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CHILD_WALLET); }, "RG_CHILD_WALLET", false);
	case 1862U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{99LL}; }, "99", false);
	case 1863U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{0LL}; }, "0", false);
	case 1864U: return std::make_unique<AndExpression>(std::vector<index_t>{1865U, 1868U});
	case 1865U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1866U});
	case 1866U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1867U);
	case 1867U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1868U: return std::make_unique<ComparisonExpression>("<=", 1869U, 1872U);
	case 1869U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1870U});
	case 1870U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1871U);
	case 1871U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1872U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	case 1873U: return std::make_unique<AndExpression>(std::vector<index_t>{1874U, 1877U});
	case 1874U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1875U});
	case 1875U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1876U);
	case 1876U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1877U: return std::make_unique<ComparisonExpression>("<=", 1878U, 1881U);
	case 1878U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1879U});
	case 1879U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1880U);
	case 1880U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1881U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	case 1882U: return std::make_unique<AndExpression>(std::vector<index_t>{1883U, 1886U});
	case 1883U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1884U});
	case 1884U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1885U);
	case 1885U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1886U: return std::make_unique<ComparisonExpression>("<=", 1887U, 1890U);
	case 1887U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1888U});
	case 1888U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1889U);
	case 1889U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1890U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	case 1891U: return std::make_unique<AndExpression>(std::vector<index_t>{1892U, 1895U});
	case 1892U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1893U});
	case 1893U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1894U);
	case 1894U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1895U: return std::make_unique<ComparisonExpression>("<=", 1896U, 1899U);
	case 1896U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1897U});
	case 1897U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1898U);
	case 1898U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1899U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	case 1900U: return std::make_unique<AndExpression>(std::vector<index_t>{1901U, 1904U});
	case 1901U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1902U});
	case 1902U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1903U);
	case 1903U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1904U: return std::make_unique<ComparisonExpression>("<=", 1905U, 1908U);
	case 1905U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1906U});
	case 1906U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1907U);
	case 1907U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1908U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	case 1909U: return std::make_unique<AndExpression>(std::vector<index_t>{1910U, 1913U});
	case 1910U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1911U});
	case 1911U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1912U);
	case 1912U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1913U: return std::make_unique<ComparisonExpression>("<=", 1914U, 1917U);
	case 1914U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1915U});
	case 1915U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1916U);
	case 1916U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1917U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	case 1918U: return std::make_unique<AndExpression>(std::vector<index_t>{1919U, 1922U});
	case 1919U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{1920U});
	case 1920U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 1921U);
	case 1921U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 1922U: return std::make_unique<ComparisonExpression>("<=", 1923U, 1926U);
	case 1923U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{1924U});
	case 1924U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 1925U);
	case 1925U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 1926U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 1843U);
	default: throw std::out_of_range("Invalid tracker expression index.");
	}
}

} // namespace rls_tracker_generated_detail

uint64_t GetExpressionCount() {
	return 1927ULL;
}

LogicExpression* GetExpression(index_t index) {
	static LogicExpressionCache cache(GetExpressionCount(), rls_tracker_generated_detail::CreateExpression);
	return cache.GetExpression(index);
}

const LogicRootMap& GetLogicRoots() {
	static const LogicRootMap roots{
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_ENTRYWAY", "RR_DEKU_TREE_LOBBY", 0U}, 0U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_ENTRYWAY", "RR_DEKU_TREE_MQ_1F", 0U}, 1U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_ENTRYWAY", "RR_KF_OUTSIDE_DEKU_TREE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ENTRYWAY", "RR_DEKU_TREE_BOSS_ROOM", 0U}, 3U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_EXIT", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 4U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_EXIT", "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", 0U}, 5U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BOSS_ROOM", "LOGIC_DEKU_TREE_CLEAR", 0U}, 6U},
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_QUEEN_GOHMA", 0U}, 103U},
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_DEKU_TREE_QUEEN_GOHMA_HEART", 0U}, 106U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_EXIT", 0U}, 109U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_KF_OUTSIDE_DEKU_TREE", 0U}, 110U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_STICK_ACCESS", 0U}, 113U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_NUT_ACCESS", 0U}, 126U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_ENTRYWAY", 0U}, 134U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_2F", 0U}, 135U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 142U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_2F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0U}, 145U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_1F", 0U}, 146U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_3F", 0U}, 147U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0U}, 154U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_STICK_ACCESS", 0U}, 157U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_NUT_ACCESS", 0U}, 173U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0U}, 203U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0U}, 204U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_2F", 0U}, 211U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 213U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_STICK_ACCESS", 0U}, 214U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_NUT_ACCESS", 0U}, 215U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "RR_DEKU_TREE_MQ_3F", 0U}, 216U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0U}, 229U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_2F", 0U}, 240U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0U}, 241U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", 0U}, 242U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0U}, 291U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_STICK_ACCESS", 0U}, 297U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_NUT_ACCESS", 0U}, 298U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 299U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_1F", 0U}, 311U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0U}, 318U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0U}, 322U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0U}, 336U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", 0U}, 376U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0U}, 389U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 390U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", 0U}, 393U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0U}, 418U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0U}, 435U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_STICK_ACCESS", 0U}, 436U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_NUT_ACCESS", 0U}, 449U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0U}, 450U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0U}, 486U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0U}, 508U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0U}, 538U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_STICK_ACCESS", 0U}, 568U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_NUT_ACCESS", 0U}, 569U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0U}, 570U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0U}, 587U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", 0U}, 588U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0U}, 596U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0U}, 597U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 600U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0U}, 601U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 604U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", 0U}, 605U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0U}, 616U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 627U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_STICK_ACCESS", 0U}, 640U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_NUT_ACCESS", 0U}, 641U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0U}, 642U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_ENTRYWAY", 0U}, 643U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_LOBBY_2F", 0U}, 644U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BASEMENT_LOWER", 0U}, 651U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 654U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 655U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY", 0U}, 656U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY_3F", 0U}, 657U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0U}, 664U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0U}, 665U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_LOBBY_2F", 0U}, 672U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_COMPASS_ROOM", 0U}, 673U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_LOBBY", 0U}, 674U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_SLINGSHOT_ROOM", 0U}, 682U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_SLINGSHOT_ROOM", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0U}, 690U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_STICK_ACCESS", 0U}, 691U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_NUT_ACCESS", 0U}, 692U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_LOBBY", 0U}, 693U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 698U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_STICK_ACCESS", 0U}, 699U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_NUT_ACCESS", 0U}, 700U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 701U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_LOBBY", 0U}, 722U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0U}, 731U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_UPPER", 0U}, 739U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 750U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_LOWER", 0U}, 751U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0U}, 752U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0U}, 756U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0U}, 757U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0U}, 764U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0U}, 771U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_STICK_ACCESS", 0U}, 772U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_NUT_ACCESS", 0U}, 773U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0U}, 774U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0U}, 782U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_STICK_ACCESS", 0U}, 790U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_NUT_ACCESS", 0U}, 791U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0U}, 792U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_BACK_ROOM", 0U}, 793U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_UPPER", 0U}, 806U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0U}, 818U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_STICK_ACCESS", 0U}, 819U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_NUT_ACCESS", 0U}, 820U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0U}, 821U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 824U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_LOWER", 0U}, 825U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0U}, 826U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 829U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BASEMENT_UPPER", 0U}, 840U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 851U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_HOUSE", "RR_KF_LINKS_PORCH", 0U}, 855U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KF_LINKS_HOUSE", 0U}, 856U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KOKIRI_FOREST", 0U}, 857U},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_FAIRY_ACCESS", 0U}, 858U},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0U}, 881U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_BOULDER_LOOP", 0U}, 892U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_LINKS_PORCH", 0U}, 895U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_MIDOS_HOUSE", 0U}, 916U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_SARIAS_HOUSE", 0U}, 917U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_HOUSE_OF_TWINS", 0U}, 918U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KNOW_IT_ALL_HOUSE", 0U}, 919U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KOKIRI_SHOP", 0U}, 920U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_DEKU_TREE", 0U}, 921U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_LOST_WOODS", 0U}, 943U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_RUPEE_ALCOVE", 0U}, 961U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_LW_BRIDGE_FROM_FOREST", 0U}, 968U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_KOKIRI_SWORD_CHEST", 0U}, 978U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_BOULDER_LOOP", "RR_KOKIRI_FOREST", 0U}, 983U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_STICK_ACCESS", 0U}, 986U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_NUT_ACCESS", 0U}, 987U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_FAIRY_ACCESS", 0U}, 988U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0U}, 989U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_DEKU_TREE_ENTRYWAY", 0U}, 1000U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_KOKIRI_FOREST", 0U}, 1017U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_LEFT_CHEST", 0U}, 1039U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_RIGHT_CHEST", 0U}, 1042U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_LEFT_CHEST", 0U}, 1045U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_RIGHT_CHEST", 0U}, 1048U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_MIDOS_HOUSE", "RR_KOKIRI_FOREST", 0U}, 1051U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_SARIAS_HOUSE", "RR_KOKIRI_FOREST", 0U}, 1052U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_HOUSE_OF_TWINS", "RR_KOKIRI_FOREST", 0U}, 1053U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KNOW_IT_ALL_HOUSE", "RR_KOKIRI_FOREST", 0U}, 1054U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KOKIRI_SHOP", "RR_KOKIRI_FOREST", 0U}, 1055U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KOKIRI_FOREST", 0U}, 1056U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_THE_LOST_WOODS", 0U}, 1057U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_RUPEE_ALCOVE", 0U}, 1058U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_STORMS_GROTTO", 0U}, 1069U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_RUPEE_ALCOVE", "RR_KOKIRI_FOREST", 0U}, 1081U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FAIRY_ACCESS", 0U}, 1082U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_BUG_ACCESS", 0U}, 1092U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FISH_ACCESS", 0U}, 1116U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_CHEST", 0U}, 1117U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_STORMS_GROTTO", "RR_KF_OUTSIDE_LOST_WOODS", 0U}, 1120U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_KAKARIKO_GATE_OPEN", 0U}, 1121U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_1_TORCH_CARPENTER", 0U}, 1126U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DOUBLE_CELL_CARPENTER", 0U}, 1131U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DEAD_END_CARPENTER", 0U}, 1142U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_SLOPE_CARPENTER", 0U}, 1153U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_RESCUED_ALL_CARPENTERS", 0U}, 1164U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_FREED_EPONA", 0U}, 1169U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_LINKS_POCKET", 0U}, 1174U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TRIFORCE_COMPLETED", 0U}, 1175U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SARIA_SONG_HINT", 0U}, 1178U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SONG_FROM_IMPA", 0U}, 1181U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_MALON_EGG", 0U}, 1186U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_ZELDAS_LETTER", 0U}, 1191U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TOT_MASTER_SWORD", 0U}, 1196U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT", "RR_ROOT_EXITS", 0U}, 1201U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_CHILD_SPAWN", 0U}, 1202U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_ADULT_SPAWN", 0U}, 1203U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_MINUET_OF_FOREST_WARP", 0U}, 1204U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_BOLERO_OF_FIRE_WARP", 0U}, 1207U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_SERENADE_OF_WATER_WARP", 0U}, 1210U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_NOCTURNE_OF_SHADOW_WARP", 0U}, 1213U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_REQUIEM_OF_SPIRIT_WARP", 0U}, 1216U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_PRELUDE_OF_LIGHT_WARP", 0U}, 1219U},
		{LogicRootKey{LogicRootKind::Exit, "RR_CHILD_SPAWN", "RR_KF_LINKS_HOUSE", 0U}, 1222U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ADULT_SPAWN", "RR_TEMPLE_OF_TIME", 0U}, 1223U},
		{LogicRootKey{LogicRootKind::Exit, "RR_MINUET_OF_FOREST_WARP", "RR_SACRED_FOREST_MEADOW", 0U}, 1224U},
		{LogicRootKey{LogicRootKind::Exit, "RR_BOLERO_OF_FIRE_WARP", "RR_DMC_PAD_ENTRY", 0U}, 1225U},
		{LogicRootKey{LogicRootKind::Exit, "RR_SERENADE_OF_WATER_WARP", "RR_LAKE_HYLIA", 0U}, 1226U},
		{LogicRootKey{LogicRootKind::Exit, "RR_REQUIEM_OF_SPIRIT_WARP", "RR_DESERT_COLOSSUS", 0U}, 1227U},
		{LogicRootKey{LogicRootKind::Exit, "RR_NOCTURNE_OF_SHADOW_WARP", "RR_GRAVEYARD_WARP_PAD_REGION", 0U}, 1228U},
		{LogicRootKey{LogicRootKind::Exit, "RR_PRELUDE_OF_LIGHT_WARP", "RR_TEMPLE_OF_TIME", 0U}, 1229U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_LEFT", 0U}, 1230U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_RIGHT", 0U}, 1264U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_COW", 0U}, 1265U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_1", 0U}, 1273U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_2", 0U}, 1284U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_3", 0U}, 1295U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY", 0U}, 1306U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY_BIG", 0U}, 1307U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY", 0U}, 1310U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY_BIG", 0U}, 1311U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY", 0U}, 1314U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY_BIG", 0U}, 1315U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY", 0U}, 1318U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY_BIG", 0U}, 1319U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_FISH", 0U}, 1322U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BRIDGE_RUPEE", 0U}, 1326U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEHIND_MIDOS_RUPEE", 0U}, 1327U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_WEST_RUPEE", 0U}, 1328U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_EAST_RUPEE", 0U}, 1329U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_WEST_RUPEE", 0U}, 1330U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_EAST_RUPEE", 0U}, 1331U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_WEST_HEART", 0U}, 1332U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_EAST_HEART", 0U}, 1333U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_NORTH_HEART", 0U}, 1334U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_1", 0U}, 1335U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_2", 0U}, 1349U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_3", 0U}, 1363U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_4", 0U}, 1377U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_5", 0U}, 1391U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_6", 0U}, 1405U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RED_RUPEE", 0U}, 1419U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_1", 0U}, 1433U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_2", 0U}, 1434U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_LEFT_HEART", 0U}, 1435U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_RIGHT_HEART", 0U}, 1436U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_LEFT_HEART", 0U}, 1437U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_RIGHT_HEART", 0U}, 1438U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_1", 0U}, 1439U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_2", 0U}, 1444U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_3", 0U}, 1449U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_4", 0U}, 1454U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_5", 0U}, 1459U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_6", 0U}, 1464U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RED_RUPEE", 0U}, 1469U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_1", 0U}, 1474U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_2", 0U}, 1479U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_3", 0U}, 1484U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_4", 0U}, 1489U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_5", 0U}, 1494U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_6", 0U}, 1499U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RED_RUPEE", 0U}, 1504U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_KNOW_IT_ALL_HOUSE", 0U}, 1509U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_BEAN_PATCH", 0U}, 1536U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_HOUSE_OF_TWINS", 0U}, 1561U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_1", 0U}, 1706U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_2", 0U}, 1709U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_3", 0U}, 1712U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_4", 0U}, 1715U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_5", 0U}, 1718U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_6", 0U}, 1721U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_7", 0U}, 1724U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_8", 0U}, 1727U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_9", 0U}, 1730U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_10", 0U}, 1733U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_11", 0U}, 1736U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_12", 0U}, 1739U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_1", 0U}, 1742U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_2", 0U}, 1745U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_3", 0U}, 1748U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_4", 0U}, 1751U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_5", 0U}, 1754U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_6", 0U}, 1757U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_7", 0U}, 1760U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_8", 0U}, 1763U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_9", 0U}, 1766U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_10", 0U}, 1769U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_11", 0U}, 1772U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_12", 0U}, 1775U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_13", 0U}, 1778U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_14", 0U}, 1781U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_15", 0U}, 1784U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_16", 0U}, 1787U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_17", 0U}, 1790U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_18", 0U}, 1793U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_19", 0U}, 1796U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_20", 0U}, 1799U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_1", 0U}, 1802U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_2", 0U}, 1805U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_3", 0U}, 1808U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_1", 0U}, 1811U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_2", 0U}, 1812U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_3", 0U}, 1813U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_4", 0U}, 1814U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE", 0U}, 1815U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE", 0U}, 1816U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE", 0U}, 1817U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE", 0U}, 1818U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_POT", 0U}, 1819U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_1", 0U}, 1822U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_2", 0U}, 1825U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_1", 0U}, 1828U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_2", 0U}, 1831U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_1", 0U}, 1834U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_2", 0U}, 1864U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_3", 0U}, 1873U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_4", 0U}, 1882U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_5", 0U}, 1891U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_6", 0U}, 1900U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_7", 0U}, 1909U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_8", 0U}, 1918U},
	};
	return roots;
}
