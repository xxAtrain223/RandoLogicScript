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
	case 3U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{4U, 6U, 8U, 9U, 11U, 13U}, 14U);
	case 4U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 5U);
	case 5U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOHMA); }, "RE_GOHMA", false);
	case 6U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 7U);
	case 7U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_CLOSE); }, "ED_CLOSE", false);
	case 8U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 2U);
	case 9U: return std::make_unique<ParameterExpression>(uint8_t{3}, "quantity", 10U);
	case 10U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1LL}; }, "1", false);
	case 11U: return std::make_unique<ParameterExpression>(uint8_t{4}, "timer", 12U);
	case 12U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 13U: return std::make_unique<ParameterExpression>(uint8_t{5}, "in_water", 12U);
	case 14U: return std::make_unique<MatchExpression<bool>>(15U, std::vector<index_t>{16U});
	case 15U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("e"); }, "e", false);
	case 16U: return std::make_unique<MatchArm>(17U, 19U, false);
	case 17U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{18U});
	case 18U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA); }, "RE_GOLD_SKULLTULA", false);
	case 19U: return std::make_unique<FunctionCallExpression>("_can_kill_gold_skulltula", std::vector<index_t>{20U, 22U}, 24U);
	case 20U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 21U);
	case 21U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 22U: return std::make_unique<ParameterExpression>(uint8_t{1}, "wall_or_floor", 23U);
	case 23U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("wall_or_floor"); }, "wall_or_floor", false);
	case 24U: return std::make_unique<MatchExpression<bool>>(21U, std::vector<index_t>{25U, 30U, 36U, 42U, 52U, 58U, 68U, 74U, 85U});
	case 25U: return std::make_unique<MatchArm>(26U, 27U, true);
	case 26U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{7U});
	case 27U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{28U});
	case 28U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 29U);
	case 29U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER); }, "RG_MEGATON_HAMMER", false);
	case 30U: return std::make_unique<MatchArm>(31U, 33U, true);
	case 31U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{32U});
	case 32U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH); }, "ED_SHORT_JUMPSLASH", false);
	case 33U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{34U});
	case 34U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 35U);
	case 35U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD); }, "RG_KOKIRI_SWORD", false);
	case 36U: return std::make_unique<MatchArm>(37U, 39U, true);
	case 37U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{38U});
	case 38U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_MASTER_SWORD_JUMPSLASH); }, "ED_MASTER_SWORD_JUMPSLASH", false);
	case 39U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{40U});
	case 40U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 41U);
	case 41U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MASTER_SWORD); }, "RG_MASTER_SWORD", false);
	case 42U: return std::make_unique<MatchArm>(43U, 45U, true);
	case 43U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{44U});
	case 44U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONG_JUMPSLASH); }, "ED_LONG_JUMPSLASH", false);
	case 45U: return std::make_unique<OrExpression>(std::vector<index_t>{46U, 49U});
	case 46U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{47U});
	case 47U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 48U);
	case 48U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BIGGORON_SWORD); }, "RG_BIGGORON_SWORD", false);
	case 49U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{50U});
	case 50U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 51U);
	case 51U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STICKS); }, "RG_STICKS", false);
	case 52U: return std::make_unique<MatchArm>(53U, 55U, true);
	case 53U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{54U});
	case 54U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOMB_THROW); }, "ED_BOMB_THROW", false);
	case 55U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{56U});
	case 56U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 57U);
	case 57U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMB_BAG); }, "RG_BOMB_BAG", false);
	case 58U: return std::make_unique<MatchArm>(59U, 61U, true);
	case 59U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{60U});
	case 60U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_BOOMERANG); }, "ED_BOOMERANG", false);
	case 61U: return std::make_unique<OrExpression>(std::vector<index_t>{62U, 65U});
	case 62U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{63U});
	case 63U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 64U);
	case 64U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOOMERANG); }, "RG_BOOMERANG", false);
	case 65U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{66U});
	case 66U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 67U);
	case 67U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DINS_FIRE); }, "RG_DINS_FIRE", false);
	case 68U: return std::make_unique<MatchArm>(69U, 71U, true);
	case 69U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{70U});
	case 70U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_HOOKSHOT); }, "ED_HOOKSHOT", false);
	case 71U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{72U});
	case 72U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 73U);
	case 73U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOOKSHOT); }, "RG_HOOKSHOT", false);
	case 74U: return std::make_unique<MatchArm>(75U, 77U, true);
	case 75U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{76U});
	case 76U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_LONGSHOT); }, "ED_LONGSHOT", false);
	case 77U: return std::make_unique<OrExpression>(std::vector<index_t>{78U, 81U});
	case 78U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{79U});
	case 79U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 80U);
	case 80U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_LONGSHOT); }, "RG_LONGSHOT", false);
	case 81U: return std::make_unique<AndExpression>(std::vector<index_t>{23U, 82U});
	case 82U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{83U});
	case 83U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 84U);
	case 84U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOMBCHU_5); }, "RG_BOMBCHU_5", false);
	case 85U: return std::make_unique<MatchArm>(86U, 88U, false);
	case 86U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{87U});
	case 87U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(EnemyDistance::ED_FAR); }, "ED_FAR", false);
	case 88U: return std::make_unique<OrExpression>(std::vector<index_t>{89U, 92U});
	case 89U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{90U});
	case 90U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 91U);
	case 91U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_SLINGSHOT); }, "RG_FAIRY_SLINGSHOT", false);
	case 92U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{93U});
	case 93U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 94U);
	case 94U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FAIRY_BOW); }, "RG_FAIRY_BOW", false);
	case 95U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{96U});
	case 96U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 97U);
	case 97U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR); }, "LOGIC_DEKU_TREE_CLEAR", false);
	case 98U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{99U, 6U, 8U, 9U, 11U, 13U}, 14U);
	case 99U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 100U);
	case 100U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_WITHERED_DEKU_BABA); }, "RE_WITHERED_DEKU_BABA", false);
	case 101U: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 102U);
	case 102U: return std::make_unique<OrExpression>(std::vector<index_t>{65U, 103U});
	case 103U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{104U});
	case 104U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 105U);
	case 105U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_FIRE_ARROWS); }, "RG_FIRE_ARROWS", false);
	case 106U: return std::make_unique<OrExpression>(std::vector<index_t>{107U, 71U});
	case 107U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{108U});
	case 108U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 109U);
	case 109U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CLIMB); }, "RG_CLIMB", false);
	case 110U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{111U});
	case 111U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 112U);
	case 112U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_1F_BROKE_WEB); }, "LOGIC_DEKU_TREE_1F_BROKE_WEB", false);
	case 113U: return std::make_unique<OrExpression>(std::vector<index_t>{107U, 78U});
	case 114U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{115U});
	case 115U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 116U);
	case 116U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB); }, "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", false);
	case 117U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 118U);
	case 118U: return std::make_unique<OrExpression>(std::vector<index_t>{119U, 62U});
	case 119U: return std::make_unique<FunctionCallExpression>("can_use_sword", std::vector<index_t>{}, 120U);
	case 120U: return std::make_unique<OrExpression>(std::vector<index_t>{33U, 39U, 46U});
	case 121U: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 122U);
	case 122U: return std::make_unique<OrExpression>(std::vector<index_t>{123U, 89U, 92U, 127U, 65U});
	case 123U: return std::make_unique<FunctionCallExpression>("can_jumpslash", std::vector<index_t>{}, 124U);
	case 124U: return std::make_unique<OrExpression>(std::vector<index_t>{125U, 27U});
	case 125U: return std::make_unique<FunctionCallExpression>("can_jumpslash_except_hammer", std::vector<index_t>{}, 126U);
	case 126U: return std::make_unique<OrExpression>(std::vector<index_t>{49U, 119U});
	case 127U: return std::make_unique<FunctionCallExpression>("has_explosives", std::vector<index_t>{}, 128U);
	case 128U: return std::make_unique<OrExpression>(std::vector<index_t>{55U, 82U});
	case 129U: return std::make_unique<OrExpression>(std::vector<index_t>{49U, 92U});
	case 130U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{131U, 6U, 8U, 9U, 11U, 13U}, 14U);
	case 131U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 132U);
	case 132U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_DEKU_BABA); }, "RE_DEKU_BABA", false);
	case 133U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{134U});
	case 134U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 135U);
	case 135U: return std::make_unique<ConditionExpression>(136U);
	case 136U: return std::make_unique<FunctionCallExpression>("can_hit_eye_targets", std::vector<index_t>{}, 137U);
	case 137U: return std::make_unique<OrExpression>(std::vector<index_t>{92U, 89U});
	case 138U: return std::make_unique<AndExpression>(std::vector<index_t>{139U, 145U});
	case 139U: return std::make_unique<OrExpression>(std::vector<index_t>{107U, 71U, 140U});
	case 140U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 142U});
	case 141U: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 142U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{143U});
	case 143U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 144U);
	case 144U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME); }, "RG_SONG_OF_TIME", false);
	case 145U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{146U});
	case 146U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 147U);
	case 147U: return std::make_unique<ConditionExpression>(148U);
	case 148U: return std::make_unique<OrExpression>(std::vector<index_t>{82U, 149U, 154U});
	case 149U: return std::make_unique<AndExpression>(std::vector<index_t>{55U, 150U});
	case 150U: return std::make_unique<OrExpression>(std::vector<index_t>{142U, 141U, 151U});
	case 151U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{152U});
	case 152U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 153U);
	case 153U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS); }, "RG_HOVER_BOOTS", false);
	case 154U: return std::make_unique<AndExpression>(std::vector<index_t>{27U, 155U});
	case 155U: return std::make_unique<OrExpression>(std::vector<index_t>{140U, 156U});
	case 156U: return std::make_unique<AndExpression>(std::vector<index_t>{157U, 107U});
	case 157U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{158U});
	case 158U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 159U);
	case 159U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_COMPASS_GS); }, "RT_DEKU_MQ_COMPASS_GS", false);
	case 160U: return std::make_unique<FunctionCallExpression>("blast_or_smash", std::vector<index_t>{}, 161U);
	case 161U: return std::make_unique<OrExpression>(std::vector<index_t>{127U, 27U});
	case 162U: return std::make_unique<AndExpression>(std::vector<index_t>{49U, 163U});
	case 163U: return std::make_unique<OrExpression>(std::vector<index_t>{164U, 141U, 151U});
	case 164U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{165U});
	case 165U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 166U);
	case 166U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK); }, "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", false);
	case 167U: return std::make_unique<AndExpression>(std::vector<index_t>{133U, 168U, 171U});
	case 168U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{169U});
	case 169U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 170U);
	case 170U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM); }, "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", false);
	case 171U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{172U});
	case 172U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 173U);
	case 173U: return std::make_unique<ConditionExpression>(49U);
	case 174U: return std::make_unique<OrExpression>(std::vector<index_t>{141U, 175U, 178U, 164U, 151U});
	case 175U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{176U});
	case 176U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 177U);
	case 177U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_SKIP); }, "RT_DEKU_B1_SKIP", false);
	case 178U: return std::make_unique<FunctionCallExpression>("can_ground_jump", std::vector<index_t>{179U}, 180U);
	case 179U: return std::make_unique<ParameterExpression>(uint8_t{0}, "hasBombflower", 12U);
	case 180U: return std::make_unique<AndExpression>(std::vector<index_t>{181U, 184U, 196U});
	case 181U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{182U});
	case 182U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 183U);
	case 183U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_GROUND_JUMP); }, "RT_GROUND_JUMP", false);
	case 184U: return std::make_unique<FunctionCallExpression>("can_standing_shield", std::vector<index_t>{}, 185U);
	case 185U: return std::make_unique<OrExpression>(std::vector<index_t>{186U, 189U, 193U});
	case 186U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{187U});
	case 187U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 188U);
	case 188U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MIRROR_SHIELD); }, "RG_MIRROR_SHIELD", false);
	case 189U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 190U});
	case 190U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{191U});
	case 191U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 192U);
	case 192U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_HYLIAN_SHIELD); }, "RG_HYLIAN_SHIELD", false);
	case 193U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{194U});
	case 194U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 195U);
	case 195U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD); }, "RG_DEKU_SHIELD", false);
	case 196U: return std::make_unique<OrExpression>(std::vector<index_t>{55U, 197U});
	case 197U: return std::make_unique<AndExpression>(std::vector<index_t>{198U, 199U});
	case 198U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("hasBombflower"); }, "hasBombflower", false);
	case 199U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{200U});
	case 200U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 201U);
	case 201U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_GORONS_BRACELET); }, "RG_GORONS_BRACELET", false);
	case 202U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{203U, 6U, 8U, 9U, 11U, 13U}, 14U);
	case 203U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 204U);
	case 204U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_MAD_SCRUB); }, "RE_MAD_SCRUB", false);
	case 205U: return std::make_unique<OrExpression>(std::vector<index_t>{103U, 206U});
	case 206U: return std::make_unique<AndExpression>(std::vector<index_t>{49U, 207U});
	case 207U: return std::make_unique<OrExpression>(std::vector<index_t>{208U, 211U});
	case 208U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{209U});
	case 209U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 210U);
	case 210U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_LOG); }, "RT_DEKU_MQ_LOG", false);
	case 211U: return std::make_unique<AndExpression>(std::vector<index_t>{212U, 213U});
	case 212U: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 213U: return std::make_unique<FunctionCallExpression>("can_shield", std::vector<index_t>{}, 214U);
	case 214U: return std::make_unique<OrExpression>(std::vector<index_t>{186U, 190U, 193U});
	case 215U: return std::make_unique<OrExpression>(std::vector<index_t>{208U, 211U, 78U, 216U});
	case 216U: return std::make_unique<AndExpression>(std::vector<index_t>{71U, 217U});
	case 217U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{218U});
	case 218U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 219U);
	case 219U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_IRON_BOOTS); }, "RG_IRON_BOOTS", false);
	case 220U: return std::make_unique<AndExpression>(std::vector<index_t>{221U, 224U});
	case 221U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{222U});
	case 222U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 223U);
	case 223U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES); }, "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", false);
	case 224U: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{225U, 227U, 8U}, 229U);
	case 225U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 226U);
	case 226U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_BIG_SKULLTULA); }, "RE_BIG_SKULLTULA", false);
	case 227U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 228U);
	case 228U: return std::make_unique<TernaryExpression>(142U, 7U, 32U);
	case 229U: return std::make_unique<OrExpression>(std::vector<index_t>{230U, 234U});
	case 230U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{231U, 232U, 233U, 9U, 11U, 13U}, 14U);
	case 231U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 15U);
	case 232U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 21U);
	case 233U: return std::make_unique<ParameterExpression>(uint8_t{2}, "wall_or_floor", 23U);
	case 234U: return std::make_unique<MatchExpression<bool>>(15U, std::vector<index_t>{235U});
	case 235U: return std::make_unique<MatchArm>(17U, 2U, false);
	case 236U: return std::make_unique<OrExpression>(std::vector<index_t>{208U, 211U, 78U, 237U, 240U});
	case 237U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{238U});
	case 238U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 239U);
	case 239U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE); }, "RG_BRONZE_SCALE", false);
	case 240U: return std::make_unique<AndExpression>(std::vector<index_t>{217U, 241U});
	case 241U: return std::make_unique<OrExpression>(std::vector<index_t>{141U, 71U});
	case 242U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{243U});
	case 243U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 244U);
	case 244U: return std::make_unique<ConditionExpression>(245U);
	case 245U: return std::make_unique<AndExpression>(std::vector<index_t>{202U, 246U});
	case 246U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{247U, 6U, 8U, 9U, 11U, 13U}, 14U);
	case 247U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 248U);
	case 248U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_KEESE); }, "RE_KEESE", false);
	case 249U: return std::make_unique<AndExpression>(std::vector<index_t>{250U, 253U});
	case 250U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{251U});
	case 251U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 252U);
	case 252U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CRAWL); }, "RG_CRAWL", false);
	case 253U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{254U});
	case 254U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 255U);
	case 255U: return std::make_unique<ConditionExpression>(256U);
	case 256U: return std::make_unique<OrExpression>(std::vector<index_t>{257U, 92U});
	case 257U: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 258U);
	case 258U: return std::make_unique<OrExpression>(std::vector<index_t>{101U, 49U});
	case 259U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{260U});
	case 260U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 261U);
	case 261U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_POWER_BRACELET); }, "RG_POWER_BRACELET", false);
	case 262U: return std::make_unique<AndExpression>(std::vector<index_t>{263U, 266U});
	case 263U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{264U});
	case 264U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 265U);
	case 265U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_B1_BROKE_WEB); }, "LOGIC_DEKU_TREE_B1_BROKE_WEB", false);
	case 266U: return std::make_unique<OrExpression>(std::vector<index_t>{237U, 217U});
	case 267U: return std::make_unique<AndExpression>(std::vector<index_t>{107U, 268U});
	case 268U: return std::make_unique<OrExpression>(std::vector<index_t>{237U, 71U});
	case 269U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{270U});
	case 270U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 271U);
	case 271U: return std::make_unique<ConditionExpression>(272U);
	case 272U: return std::make_unique<FunctionCallExpression>("can_reflect_nuts", std::vector<index_t>{}, 273U);
	case 273U: return std::make_unique<OrExpression>(std::vector<index_t>{193U, 189U});
	case 274U: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{225U, 6U, 8U}, 229U);
	case 275U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{276U});
	case 276U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 277U);
	case 277U: return std::make_unique<ConditionExpression>(278U);
	case 278U: return std::make_unique<OrExpression>(std::vector<index_t>{272U, 27U});
	case 279U: return std::make_unique<OrExpression>(std::vector<index_t>{162U, 280U});
	case 280U: return std::make_unique<AndExpression>(std::vector<index_t>{281U, 141U, 92U});
	case 281U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{282U});
	case 282U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 283U);
	case 283U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BOW_WEBS); }, "RT_DEKU_B1_BOW_WEBS", false);
	case 284U: return std::make_unique<OrExpression>(std::vector<index_t>{107U, 285U});
	case 285U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 78U});
	case 286U: return std::make_unique<OrExpression>(std::vector<index_t>{141U, 175U, 178U, 164U});
	case 287U: return std::make_unique<OrExpression>(std::vector<index_t>{237U, 288U});
	case 288U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{289U});
	case 289U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 290U);
	case 290U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG); }, "RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG", false);
	case 291U: return std::make_unique<AndExpression>(std::vector<index_t>{253U, 292U});
	case 292U: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{293U});
	case 293U: return std::make_unique<ParameterExpression>(uint8_t{0}, "condition", 294U);
	case 294U: return std::make_unique<ConditionExpression>(160U);
	case 295U: return std::make_unique<AndExpression>(std::vector<index_t>{253U, 250U});
	case 296U: return std::make_unique<OrExpression>(std::vector<index_t>{297U, 305U});
	case 297U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 298U);
	case 298U: return std::make_unique<OrExpression>(std::vector<index_t>{299U, 302U, 142U});
	case 299U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{300U});
	case 300U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 301U);
	case 301U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_ZELDAS_LULLABY); }, "RG_ZELDAS_LULLABY", false);
	case 302U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{303U});
	case 303U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 304U);
	case 304U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_EPONAS_SONG); }, "RG_EPONAS_SONG", false);
	case 305U: return std::make_unique<AndExpression>(std::vector<index_t>{212U, 306U, 309U, 312U});
	case 306U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{307U});
	case 307U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 308U);
	case 308U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MAGIC_BEAN); }, "RG_MAGIC_BEAN", false);
	case 309U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{310U});
	case 310U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 311U);
	case 311U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL); }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 312U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{313U});
	case 313U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 314U);
	case 314U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS); }, "RG_SONG_OF_STORMS", false);
	case 315U: return std::make_unique<AndExpression>(std::vector<index_t>{212U, 316U, 33U, 193U});
	case 316U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{317U});
	case 317U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 318U);
	case 318U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI); }, "RG_SPEAK_KOKIRI", false);
	case 319U: return std::make_unique<TernaryExpression>(212U, 320U, 326U);
	case 320U: return std::make_unique<FunctionCallExpression>("can_climb_ladder", std::vector<index_t>{}, 321U);
	case 321U: return std::make_unique<OrExpression>(std::vector<index_t>{107U, 322U});
	case 322U: return std::make_unique<AndExpression>(std::vector<index_t>{323U, 71U});
	case 323U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{324U});
	case 324U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 325U);
	case 325U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_HOOKSHOT_LADDERS); }, "RT_HOOKSHOT_LADDERS", false);
	case 326U: return std::make_unique<OrExpression>(std::vector<index_t>{107U, 151U});
	case 327U: return std::make_unique<OrExpression>(std::vector<index_t>{328U, 331U, 336U});
	case 328U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{329U});
	case 329U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 330U);
	case 330U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD); }, "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", false);
	case 331U: return std::make_unique<ComparisonExpression>("==", 332U, 335U);
	case 332U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{333U});
	case 333U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 334U);
	case 334U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_FOREST); }, "RSK_FOREST", false);
	case 335U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_CLOSED_FOREST_OFF); }, "RO_CLOSED_FOREST_OFF", false);
	case 336U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 337U});
	case 337U: return std::make_unique<OrExpression>(std::vector<index_t>{274U, 338U});
	case 338U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{339U});
	case 339U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 340U);
	case 340U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_FOREST_TEMPLE_CLEAR); }, "LOGIC_FOREST_TEMPLE_CLEAR", false);
	case 341U: return std::make_unique<OrExpression>(std::vector<index_t>{107U, 71U, 342U});
	case 342U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 343U});
	case 343U: return std::make_unique<OrExpression>(std::vector<index_t>{344U, 348U});
	case 344U: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{345U, 347U});
	case 345U: return std::make_unique<ParameterExpression>(uint8_t{0}, "reg", 346U);
	case 346U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST); }, "RR_KOKIRI_FOREST", false);
	case 347U: return std::make_unique<ParameterExpression>(uint8_t{1}, "bean", 311U);
	case 348U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{349U});
	case 349U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 350U);
	case 350U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_UNINTUITIVE_JUMPS); }, "RT_UNINTUITIVE_JUMPS", false);
	case 351U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 344U});
	case 352U: return std::make_unique<OrExpression>(std::vector<index_t>{141U, 353U, 95U});
	case 353U: return std::make_unique<ComparisonExpression>("!=", 332U, 354U);
	case 354U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_CLOSED_FOREST_ON); }, "RO_CLOSED_FOREST_ON", false);
	case 355U: return std::make_unique<AndExpression>(std::vector<index_t>{212U, 356U});
	case 356U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{357U});
	case 357U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 358U);
	case 358U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_OPEN_CHEST); }, "RG_OPEN_CHEST", false);
	case 359U: return std::make_unique<OrExpression>(std::vector<index_t>{212U, 360U});
	case 360U: return std::make_unique<AndExpression>(std::vector<index_t>{361U, 366U});
	case 361U: return std::make_unique<ComparisonExpression>("!=", 362U, 365U);
	case 362U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{363U});
	case 363U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 364U);
	case 364U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SHUFFLE_DUNGEON_ENTRANCES); }, "RSK_SHUFFLE_DUNGEON_ENTRANCES", false);
	case 365U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF); }, "RO_DUNGEON_ENTRANCE_SHUFFLE_OFF", false);
	case 366U: return std::make_unique<OrExpression>(std::vector<index_t>{331U, 328U});
	case 367U: return std::make_unique<OrExpression>(std::vector<index_t>{368U, 331U, 328U});
	case 368U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 369U});
	case 369U: return std::make_unique<OrExpression>(std::vector<index_t>{274U, 95U});
	case 370U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 371U});
	case 371U: return std::make_unique<OrExpression>(std::vector<index_t>{344U, 151U});
	case 372U: return std::make_unique<FunctionCallExpression>("can_open_storms_grotto", std::vector<index_t>{}, 373U);
	case 373U: return std::make_unique<AndExpression>(std::vector<index_t>{312U, 374U});
	case 374U: return std::make_unique<OrExpression>(std::vector<index_t>{375U, 378U});
	case 375U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{376U});
	case 376U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 377U);
	case 377U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_STONE_OF_AGONY); }, "RG_STONE_OF_AGONY", false);
	case 378U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{379U});
	case 379U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 380U);
	case 380U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_GROTTOS_WITHOUT_AGONY); }, "RT_GROTTOS_WITHOUT_AGONY", false);
	case 381U: return std::make_unique<OrExpression>(std::vector<index_t>{382U, 49U});
	case 382U: return std::make_unique<FunctionCallExpression>("call_gossip_fairy", std::vector<index_t>{}, 383U);
	case 383U: return std::make_unique<OrExpression>(std::vector<index_t>{297U, 384U});
	case 384U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{385U});
	case 385U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 386U);
	case 386U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SUNS_SONG); }, "RG_SUNS_SONG", false);
	case 387U: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 388U);
	case 388U: return std::make_unique<OrExpression>(std::vector<index_t>{33U, 62U, 127U, 199U, 39U, 27U, 46U, 389U});
	case 389U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{390U});
	case 390U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 391U);
	case 391U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_GIANTS_KNIFE); }, "RG_GIANTS_KNIFE", false);
	case 392U: return std::make_unique<ComparisonExpression>("==", 393U, 396U);
	case 393U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{394U});
	case 394U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 395U);
	case 395U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_KAK_GATE); }, "RSK_KAK_GATE", false);
	case 396U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_KAK_GATE_OPEN); }, "RO_KAK_GATE_OPEN", false);
	case 397U: return std::make_unique<ComparisonExpression>("==", 398U, 401U);
	case 398U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{399U});
	case 399U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 400U);
	case 400U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_GERUDO_FORTRESS); }, "RSK_GERUDO_FORTRESS", false);
	case 401U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FREE); }, "RO_GF_CARPENTERS_FREE", false);
	case 402U: return std::make_unique<OrExpression>(std::vector<index_t>{397U, 403U});
	case 403U: return std::make_unique<ComparisonExpression>("==", 398U, 404U);
	case 404U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GF_CARPENTERS_FAST); }, "RO_GF_CARPENTERS_FAST", false);
	case 405U: return std::make_unique<ComparisonExpression>("==", 406U, 409U);
	case 406U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{407U});
	case 407U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 408U);
	case 408U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKIP_EPONA_RACE); }, "RSK_SKIP_EPONA_RACE", false);
	case 409U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_GENERIC_YES); }, "RO_GENERIC_YES", false);
	case 410U: return std::make_unique<ComparisonExpression>(">=", 411U, 412U);
	case 411U: return std::make_unique<ExternFunctionCallExpression>(collected_triforce_pieces_adapter, "collected_triforce_pieces", std::vector<index_t>{});
	case 412U: return std::make_unique<ExternFunctionCallExpression>(required_triforce_pieces_adapter, "required_triforce_pieces", std::vector<index_t>{});
	case 413U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{414U});
	case 414U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 415U);
	case 415U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SARIAS_SONG); }, "RG_SARIAS_SONG", false);
	case 416U: return std::make_unique<ComparisonExpression>("==", 417U, 409U);
	case 417U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{418U});
	case 418U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 419U);
	case 419U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKIP_CHILD_ZELDA); }, "RSK_SKIP_CHILD_ZELDA", false);
	case 420U: return std::make_unique<ComparisonExpression>("==", 421U, 424U);
	case 421U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{422U});
	case 422U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 423U);
	case 423U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SELECTED_STARTING_AGE); }, "RSK_SELECTED_STARTING_AGE", false);
	case 424U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RO_AGE_ADULT); }, "RO_AGE_ADULT", false);
	case 425U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{426U});
	case 426U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 427U);
	case 427U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_MINUET_OF_FOREST); }, "RG_MINUET_OF_FOREST", false);
	case 428U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{429U});
	case 429U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 430U);
	case 430U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOLERO_OF_FIRE); }, "RG_BOLERO_OF_FIRE", false);
	case 431U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{432U});
	case 432U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 433U);
	case 433U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_SERENADE_OF_WATER); }, "RG_SERENADE_OF_WATER", false);
	case 434U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{435U});
	case 435U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 436U);
	case 436U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_NOCTURNE_OF_SHADOW); }, "RG_NOCTURNE_OF_SHADOW", false);
	case 437U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{438U});
	case 438U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 439U);
	case 439U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_REQUIEM_OF_SPIRIT); }, "RG_REQUIEM_OF_SPIRIT", false);
	case 440U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{441U});
	case 441U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 442U);
	case 442U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_PRELUDE_OF_LIGHT); }, "RG_PRELUDE_OF_LIGHT", false);
	case 443U: return std::make_unique<FunctionCallExpression>("can_break_lower_beehives", std::vector<index_t>{}, 444U);
	case 444U: return std::make_unique<OrExpression>(std::vector<index_t>{445U, 55U});
	case 445U: return std::make_unique<FunctionCallExpression>("can_break_upper_beehives", std::vector<index_t>{}, 446U);
	case 446U: return std::make_unique<OrExpression>(std::vector<index_t>{447U, 449U, 453U});
	case 447U: return std::make_unique<FunctionCallExpression>("hookshot_or_boomerang", std::vector<index_t>{}, 448U);
	case 448U: return std::make_unique<OrExpression>(std::vector<index_t>{71U, 62U});
	case 449U: return std::make_unique<AndExpression>(std::vector<index_t>{450U, 82U});
	case 450U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{451U});
	case 451U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 452U);
	case 452U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_BOMBCHU_BEEHIVES); }, "RT_BOMBCHU_BEEHIVES", false);
	case 453U: return std::make_unique<AndExpression>(std::vector<index_t>{454U, 137U});
	case 454U: return std::make_unique<ConversionExpression<bool>>(455U);
	case 455U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{456U});
	case 456U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 457U);
	case 457U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SLINGBOW_BREAK_BEEHIVES); }, "RSK_SLINGBOW_BREAK_BEEHIVES", false);
	case 458U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 302U, 459U});
	case 459U: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{460U});
	case 460U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 461U);
	case 461U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(LogicVal::LOGIC_LINKS_COW); }, "LOGIC_LINKS_COW", false);
	case 462U: return std::make_unique<FunctionCallExpression>("has_bottle", std::vector<index_t>{}, 463U);
	case 463U: return std::make_unique<ComparisonExpression>(">=", 464U, 10U);
	case 464U: return std::make_unique<ExternFunctionCallExpression>(bottle_count_adapter, "bottle_count", std::vector<index_t>{});
	case 465U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 466U});
	case 466U: return std::make_unique<OrExpression>(std::vector<index_t>{344U, 151U, 62U});
	case 467U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 62U});
	case 468U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 151U});
	case 469U: return std::make_unique<AndExpression>(std::vector<index_t>{212U, 470U, 472U});
	case 470U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{471U, 6U, 8U, 9U, 11U, 13U}, 14U);
	case 471U: return std::make_unique<ParameterExpression>(uint8_t{0}, "e", 18U);
	case 472U: return std::make_unique<FunctionCallExpression>("can_get_night_time_gs", std::vector<index_t>{}, 473U);
	case 473U: return std::make_unique<AndExpression>(std::vector<index_t>{474U, 475U});
	case 474U: return std::make_unique<ExternFunctionCallExpression>(at_night_adapter, "at_night", std::vector<index_t>{});
	case 475U: return std::make_unique<OrExpression>(std::vector<index_t>{384U, 476U});
	case 476U: return std::make_unique<NotExpression>(477U);
	case 477U: return std::make_unique<ConversionExpression<bool>>(478U);
	case 478U: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{479U});
	case 479U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 480U);
	case 480U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RSK_SKULLS_SUNS_SONG); }, "RSK_SKULLS_SUNS_SONG", false);
	case 481U: return std::make_unique<AndExpression>(std::vector<index_t>{482U, 470U});
	case 482U: return std::make_unique<FunctionCallExpression>("can_spawn_soil_skull", std::vector<index_t>{483U}, 484U);
	case 483U: return std::make_unique<ParameterExpression>(uint8_t{0}, "bean", 311U);
	case 484U: return std::make_unique<AndExpression>(std::vector<index_t>{212U, 485U, 488U});
	case 485U: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{486U});
	case 486U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 487U);
	case 487U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_BOTTLE_WITH_BUGS); }, "RG_BOTTLE_WITH_BUGS", false);
	case 488U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{489U});
	case 489U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 490U);
	case 490U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("bean"); }, "bean", false);
	case 491U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 472U, 492U});
	case 492U: return std::make_unique<OrExpression>(std::vector<index_t>{493U, 540U});
	case 493U: return std::make_unique<FunctionCallExpression>("can_get_drop", std::vector<index_t>{471U, 494U, 495U}, 496U);
	case 494U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 60U);
	case 495U: return std::make_unique<ParameterExpression>(uint8_t{2}, "above_link", 12U);
	case 496U: return std::make_unique<AndExpression>(std::vector<index_t>{497U, 498U});
	case 497U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{231U, 232U, 8U, 9U, 11U, 13U}, 14U);
	case 498U: return std::make_unique<OrExpression>(std::vector<index_t>{499U, 521U});
	case 499U: return std::make_unique<ComparisonExpression>("<=", 500U, 519U);
	case 500U: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{20U}, 501U);
	case 501U: return std::make_unique<MatchExpression<int32_t>>(21U, std::vector<index_t>{502U, 504U, 505U, 507U, 509U, 511U, 513U, 515U, 517U});
	case 502U: return std::make_unique<MatchArm>(26U, 503U, false);
	case 503U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{0LL}; }, "0", false);
	case 504U: return std::make_unique<MatchArm>(31U, 10U, false);
	case 505U: return std::make_unique<MatchArm>(37U, 506U, false);
	case 506U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{2LL}; }, "2", false);
	case 507U: return std::make_unique<MatchArm>(43U, 508U, false);
	case 508U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{3LL}; }, "3", false);
	case 509U: return std::make_unique<MatchArm>(53U, 510U, false);
	case 510U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{4LL}; }, "4", false);
	case 511U: return std::make_unique<MatchArm>(59U, 512U, false);
	case 512U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{5LL}; }, "5", false);
	case 513U: return std::make_unique<MatchArm>(69U, 514U, false);
	case 514U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{6LL}; }, "6", false);
	case 515U: return std::make_unique<MatchArm>(75U, 516U, false);
	case 516U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{7LL}; }, "7", false);
	case 517U: return std::make_unique<MatchArm>(86U, 518U, false);
	case 518U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{8LL}; }, "8", false);
	case 519U: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{520U}, 501U);
	case 520U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 38U);
	case 521U: return std::make_unique<MatchExpression<bool>>(15U, std::vector<index_t>{522U, 528U, 532U});
	case 522U: return std::make_unique<MatchArm>(17U, 523U, false);
	case 523U: return std::make_unique<FunctionCallExpression>("_can_get_drop_gold_skulltula", std::vector<index_t>{20U}, 524U);
	case 524U: return std::make_unique<MatchExpression<bool>>(21U, std::vector<index_t>{525U, 68U, 527U});
	case 525U: return std::make_unique<MatchArm>(526U, 62U, true);
	case 526U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{7U, 32U, 38U, 44U, 54U, 60U});
	case 527U: return std::make_unique<MatchArm>(75U, 78U, false);
	case 528U: return std::make_unique<MatchArm>(529U, 2U, false);
	case 529U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{248U, 530U, 531U});
	case 530U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_FIRE_KEESE); }, "RE_FIRE_KEESE", false);
	case 531U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerEnemy::RE_GUAY); }, "RE_GUAY", false);
	case 532U: return std::make_unique<MatchArm>(533U, 534U, false);
	case 533U: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{});
	case 534U: return std::make_unique<OrExpression>(std::vector<index_t>{535U, 536U});
	case 535U: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("above_link"); }, "above_link", false);
	case 536U: return std::make_unique<AndExpression>(std::vector<index_t>{537U, 62U});
	case 537U: return std::make_unique<ComparisonExpression>("<=", 500U, 538U);
	case 538U: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{539U}, 501U);
	case 539U: return std::make_unique<ParameterExpression>(uint8_t{0}, "distance", 60U);
	case 540U: return std::make_unique<AndExpression>(std::vector<index_t>{541U, 151U, 544U});
	case 541U: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{542U});
	case 542U: return std::make_unique<ParameterExpression>(uint8_t{0}, "key", 543U);
	case 543U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerTrick::RT_KF_ADULT_GS); }, "RT_KF_ADULT_GS", false);
	case 544U: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{471U, 545U, 8U, 9U, 11U, 13U}, 14U);
	case 545U: return std::make_unique<ParameterExpression>(uint8_t{1}, "distance", 32U);
	case 546U: return std::make_unique<AndExpression>(std::vector<index_t>{212U, 387U});
	case 547U: return std::make_unique<AndExpression>(std::vector<index_t>{141U, 387U});
	case 548U: return std::make_unique<AndExpression>(std::vector<index_t>{316U, 549U});
	case 549U: return std::make_unique<ComparisonExpression>("<=", 550U, 553U);
	case 550U: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{551U});
	case 551U: return std::make_unique<ParameterExpression>(uint8_t{0}, "check", 552U);
	case 552U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK); }, "RC_UNKNOWN_CHECK", false);
	case 553U: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 554U);
	case 554U: return std::make_unique<TernaryExpression>(555U, 558U, 559U);
	case 555U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{556U});
	case 556U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 557U);
	case 557U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_TYCOON_WALLET); }, "RG_TYCOON_WALLET", false);
	case 558U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{999LL}; }, "999", false);
	case 559U: return std::make_unique<TernaryExpression>(560U, 563U, 564U);
	case 560U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{561U});
	case 561U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 562U);
	case 562U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_GIANT_WALLET); }, "RG_GIANT_WALLET", false);
	case 563U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{500LL}; }, "500", false);
	case 564U: return std::make_unique<TernaryExpression>(565U, 568U, 569U);
	case 565U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{566U});
	case 566U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 567U);
	case 567U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_ADULT_WALLET); }, "RG_ADULT_WALLET", false);
	case 568U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{200LL}; }, "200", false);
	case 569U: return std::make_unique<TernaryExpression>(570U, 573U, 503U);
	case 570U: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{571U});
	case 571U: return std::make_unique<ParameterExpression>(uint8_t{0}, "item", 572U);
	case 572U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return CheckedHostInt(RandomizerGet::RG_CHILD_WALLET); }, "RG_CHILD_WALLET", false);
	case 573U: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{99LL}; }, "99", false);
	default: throw std::out_of_range("Invalid tracker expression index.");
	}
}

} // namespace rls_tracker_generated_detail

uint64_t GetExpressionCount() {
	return 574ULL;
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
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ENTRYWAY", "RR_DEKU_TREE_BOSS_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_EXIT", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 0U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_EXIT", "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", 0U}, 1U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BOSS_ROOM", "LOGIC_DEKU_TREE_CLEAR", 0U}, 3U},
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_QUEEN_GOHMA", 0U}, 95U},
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_DEKU_TREE_QUEEN_GOHMA_HEART", 0U}, 95U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_EXIT", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_KF_OUTSIDE_DEKU_TREE", 0U}, 95U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_STICK_ACCESS", 0U}, 98U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_NUT_ACCESS", 0U}, 101U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_ENTRYWAY", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_2F", 0U}, 106U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 110U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_2F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0U}, 101U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_1F", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_3F", 0U}, 113U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0U}, 114U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0U}, 129U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_2F", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "RR_DEKU_TREE_MQ_3F", 0U}, 130U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0U}, 133U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_2F", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", 0U}, 138U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0U}, 160U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 162U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_1F", 0U}, 113U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0U}, 133U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0U}, 167U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0U}, 174U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", 0U}, 202U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0U}, 101U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 168U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", 0U}, 205U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0U}, 215U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_STICK_ACCESS", 0U}, 98U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_NUT_ACCESS", 0U}, 101U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0U}, 220U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0U}, 236U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0U}, 242U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0U}, 242U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0U}, 249U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", 0U}, 253U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0U}, 259U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 101U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0U}, 250U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", 0U}, 262U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0U}, 267U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 269U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0U}, 101U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_ENTRYWAY", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_LOBBY_2F", 0U}, 106U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BASEMENT_LOWER", 0U}, 110U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 12U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 12U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY_3F", 0U}, 113U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0U}, 274U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_LOBBY_2F", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_COMPASS_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_LOBBY", 0U}, 275U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_SLINGSHOT_ROOM", 0U}, 275U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_SLINGSHOT_ROOM", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_LOBBY", 0U}, 256U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 12U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 279U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_LOBBY", 0U}, 284U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0U}, 253U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_UPPER", 0U}, 286U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 12U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_LOWER", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0U}, 133U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0U}, 287U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0U}, 287U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0U}, 253U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0U}, 253U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_BACK_ROOM", 0U}, 291U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_UPPER", 0U}, 295U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0U}, 259U},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0U}, 101U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_LOWER", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0U}, 250U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0U}, 262U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BASEMENT_UPPER", 0U}, 267U},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0U}, 269U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_HOUSE", "RR_KF_LINKS_PORCH", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KF_LINKS_HOUSE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_FAIRY_ACCESS", 0U}, 296U},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0U}, 315U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_BOULDER_LOOP", 0U}, 250U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_LINKS_PORCH", 0U}, 319U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_MIDOS_HOUSE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_SARIAS_HOUSE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_HOUSE_OF_TWINS", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KNOW_IT_ALL_HOUSE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KOKIRI_SHOP", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_DEKU_TREE", 0U}, 327U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_LOST_WOODS", 0U}, 341U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_RUPEE_ALCOVE", 0U}, 351U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_LW_BRIDGE_FROM_FOREST", 0U}, 352U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_KOKIRI_SWORD_CHEST", 0U}, 355U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_BOULDER_LOOP", "RR_KOKIRI_FOREST", 0U}, 250U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_STICK_ACCESS", 0U}, 117U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_NUT_ACCESS", 0U}, 121U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_FAIRY_ACCESS", 0U}, 297U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0U}, 315U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_DEKU_TREE_ENTRYWAY", 0U}, 359U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_KOKIRI_FOREST", 0U}, 367U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_LEFT_CHEST", 0U}, 356U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_RIGHT_CHEST", 0U}, 356U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_LEFT_CHEST", 0U}, 356U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_RIGHT_CHEST", 0U}, 356U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_MIDOS_HOUSE", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_SARIAS_HOUSE", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_HOUSE_OF_TWINS", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KNOW_IT_ALL_HOUSE", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KOKIRI_SHOP", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_THE_LOST_WOODS", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_RUPEE_ALCOVE", 0U}, 370U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_STORMS_GROTTO", 0U}, 372U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_RUPEE_ALCOVE", "RR_KOKIRI_FOREST", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FAIRY_ACCESS", 0U}, 381U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_BUG_ACCESS", 0U}, 387U},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FISH_ACCESS", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_CHEST", 0U}, 356U},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_STORMS_GROTTO", "RR_KF_OUTSIDE_LOST_WOODS", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_KAKARIKO_GATE_OPEN", 0U}, 392U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_1_TORCH_CARPENTER", 0U}, 397U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DOUBLE_CELL_CARPENTER", 0U}, 402U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DEAD_END_CARPENTER", 0U}, 402U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_SLOPE_CARPENTER", 0U}, 402U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_RESCUED_ALL_CARPENTERS", 0U}, 397U},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_FREED_EPONA", 0U}, 405U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_LINKS_POCKET", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TRIFORCE_COMPLETED", 0U}, 410U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SARIA_SONG_HINT", 0U}, 413U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SONG_FROM_IMPA", 0U}, 416U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_MALON_EGG", 0U}, 416U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_ZELDAS_LETTER", 0U}, 416U},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TOT_MASTER_SWORD", 0U}, 420U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT", "RR_ROOT_EXITS", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_CHILD_SPAWN", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_ADULT_SPAWN", 0U}, 141U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_MINUET_OF_FOREST_WARP", 0U}, 425U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_BOLERO_OF_FIRE_WARP", 0U}, 428U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_SERENADE_OF_WATER_WARP", 0U}, 431U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_NOCTURNE_OF_SHADOW_WARP", 0U}, 434U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_REQUIEM_OF_SPIRIT_WARP", 0U}, 437U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_PRELUDE_OF_LIGHT_WARP", 0U}, 440U},
		{LogicRootKey{LogicRootKind::Exit, "RR_CHILD_SPAWN", "RR_KF_LINKS_HOUSE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_ADULT_SPAWN", "RR_TEMPLE_OF_TIME", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_MINUET_OF_FOREST_WARP", "RR_SACRED_FOREST_MEADOW", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_BOLERO_OF_FIRE_WARP", "RR_DMC_PAD_ENTRY", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_SERENADE_OF_WATER_WARP", "RR_LAKE_HYLIA", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_REQUIEM_OF_SPIRIT_WARP", "RR_DESERT_COLOSSUS", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_NOCTURNE_OF_SHADOW_WARP", "RR_GRAVEYARD_WARP_PAD_REGION", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Exit, "RR_PRELUDE_OF_LIGHT_WARP", "RR_TEMPLE_OF_TIME", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_LEFT", 0U}, 443U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_RIGHT", 0U}, 443U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_COW", 0U}, 458U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_1", 0U}, 305U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_2", 0U}, 305U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_3", 0U}, 305U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY", 0U}, 297U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY_BIG", 0U}, 312U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY", 0U}, 297U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY_BIG", 0U}, 312U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY", 0U}, 297U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY_BIG", 0U}, 312U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY", 0U}, 382U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY_BIG", 0U}, 312U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_FISH", 0U}, 462U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BRIDGE_RUPEE", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEHIND_MIDOS_RUPEE", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_WEST_RUPEE", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_EAST_RUPEE", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_WEST_RUPEE", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_EAST_RUPEE", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_WEST_HEART", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_EAST_HEART", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_NORTH_HEART", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_1", 0U}, 465U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_2", 0U}, 465U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_3", 0U}, 465U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_4", 0U}, 465U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_5", 0U}, 465U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_6", 0U}, 465U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RED_RUPEE", 0U}, 465U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_1", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_2", 0U}, 212U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_LEFT_HEART", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_RIGHT_HEART", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_LEFT_HEART", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_RIGHT_HEART", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_1", 0U}, 467U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_2", 0U}, 467U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_3", 0U}, 467U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_4", 0U}, 467U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_5", 0U}, 467U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_6", 0U}, 467U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RED_RUPEE", 0U}, 467U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_1", 0U}, 468U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_2", 0U}, 468U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_3", 0U}, 468U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_4", 0U}, 468U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_5", 0U}, 468U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_6", 0U}, 468U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RED_RUPEE", 0U}, 468U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_KNOW_IT_ALL_HOUSE", 0U}, 469U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_BEAN_PATCH", 0U}, 481U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_HOUSE_OF_TWINS", 0U}, 491U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_1", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_2", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_3", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_4", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_5", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_6", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_7", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_8", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_9", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_10", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_11", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_12", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_1", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_2", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_3", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_4", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_5", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_6", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_7", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_8", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_9", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_10", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_11", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_12", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_13", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_14", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_15", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_16", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_17", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_18", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_19", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_20", 0U}, 547U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_1", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_2", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_3", 0U}, 546U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_1", 0U}, 387U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_2", 0U}, 387U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_3", 0U}, 387U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_4", 0U}, 387U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE", 0U}, 2U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_POT", 0U}, 259U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_1", 0U}, 259U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_2", 0U}, 259U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_1", 0U}, 259U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_2", 0U}, 259U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_1", 0U}, 548U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_2", 0U}, 548U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_3", 0U}, 548U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_4", 0U}, 548U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_5", 0U}, 548U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_6", 0U}, 548U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_7", 0U}, 548U},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_8", 0U}, 548U},
	};
	return roots;
}
