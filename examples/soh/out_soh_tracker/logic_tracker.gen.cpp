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
	return any_age(MakeConditionCallback(arguments[0]));
}

static value_t at_night_adapter(std::span<EvalResult>) {
	return at_night();
}

static value_t bottle_count_adapter(std::span<EvalResult>) {
	return CheckedHostInt(bottle_count());
}

static value_t can_plant_bean_adapter(std::span<EvalResult> arguments) {
	return can_plant_bean(static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[0].Result)), static_cast<RandomizerGet>(ToInt<int32_t>(arguments[1].Result)));
}

static value_t can_use_adapter(std::span<EvalResult> arguments) {
	return can_use(static_cast<RandomizerGet>(ToInt<int32_t>(arguments[0].Result)));
}

static value_t check_price_adapter(std::span<EvalResult> arguments) {
	return CheckedHostInt(check_price(static_cast<RandomizerCheck>(ToInt<int32_t>(arguments[0].Result))));
}

static value_t collected_triforce_pieces_adapter(std::span<EvalResult>) {
	return CheckedHostInt(collected_triforce_pieces());
}

static value_t flag_adapter(std::span<EvalResult> arguments) {
	return flag(static_cast<LogicVal>(ToInt<int32_t>(arguments[0].Result)));
}

static value_t has_adapter(std::span<EvalResult> arguments) {
	return has(static_cast<RandomizerGet>(ToInt<int32_t>(arguments[0].Result)));
}

static value_t is_adult_adapter(std::span<EvalResult>) {
	return is_adult();
}

static value_t is_child_adapter(std::span<EvalResult>) {
	return is_child();
}

static value_t is_mq_adapter(std::span<EvalResult>) {
	return is_mq();
}

static value_t is_vanilla_adapter(std::span<EvalResult>) {
	return is_vanilla();
}

static value_t required_triforce_pieces_adapter(std::span<EvalResult>) {
	return CheckedHostInt(required_triforce_pieces());
}

static value_t setting_adapter(std::span<EvalResult> arguments) {
	return CheckedHostInt(setting(static_cast<RandomizerSettingKey>(ToInt<int32_t>(arguments[0].Result))));
}

static value_t trick_adapter(std::span<EvalResult> arguments) {
	return trick(static_cast<RandomizerTrick>(ToInt<int32_t>(arguments[0].Result)));
}

static std::unique_ptr<LogicExpression> CreateExpression(index_t index) {
	switch (index) {
	case 0: return std::make_unique<ExternFunctionCallExpression>(is_vanilla_adapter, "is_vanilla", std::vector<index_t>{});
	case 1: return std::make_unique<ExternFunctionCallExpression>(is_mq_adapter, "is_mq", std::vector<index_t>{});
	case 2: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return true; }, "true", false);
	case 3: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{4, 6, 8, 9, 11, 13}, 14);
	case 4: return std::make_unique<ParameterExpression>(0, "e", 5);
	case 5: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_GOHMA), "RE_GOHMA"}; }, "RE_GOHMA", false);
	case 6: return std::make_unique<ParameterExpression>(1, "distance", 7);
	case 7: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_CLOSE), "ED_CLOSE"}; }, "ED_CLOSE", false);
	case 8: return std::make_unique<ParameterExpression>(2, "wall_or_floor", 2);
	case 9: return std::make_unique<ParameterExpression>(3, "quantity", 10);
	case 10: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{1}; }, "1", false);
	case 11: return std::make_unique<ParameterExpression>(4, "timer", 12);
	case 12: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return false; }, "false", false);
	case 13: return std::make_unique<ParameterExpression>(5, "in_water", 12);
	case 14: return std::make_unique<MatchExpression<bool>>(15, std::vector<index_t>{16});
	case 15: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("e"); }, "e", false);
	case 16: return std::make_unique<MatchArm>(17, 19, false);
	case 17: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{18});
	case 18: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_GOLD_SKULLTULA), "RE_GOLD_SKULLTULA"}; }, "RE_GOLD_SKULLTULA", false);
	case 19: return std::make_unique<FunctionCallExpression>("_can_kill_gold_skulltula", std::vector<index_t>{20, 22}, 24);
	case 20: return std::make_unique<ParameterExpression>(0, "distance", 21);
	case 21: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("distance"); }, "distance", false);
	case 22: return std::make_unique<ParameterExpression>(1, "wall_or_floor", 23);
	case 23: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("wall_or_floor"); }, "wall_or_floor", false);
	case 24: return std::make_unique<MatchExpression<bool>>(21, std::vector<index_t>{25, 30, 36, 42, 52, 58, 68, 74, 85});
	case 25: return std::make_unique<MatchArm>(26, 27, true);
	case 26: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{7});
	case 27: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{28});
	case 28: return std::make_unique<ParameterExpression>(0, "item", 29);
	case 29: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER), "RG_MEGATON_HAMMER"}; }, "RG_MEGATON_HAMMER", false);
	case 30: return std::make_unique<MatchArm>(31, 33, true);
	case 31: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{32});
	case 32: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH), "ED_SHORT_JUMPSLASH"}; }, "ED_SHORT_JUMPSLASH", false);
	case 33: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{34});
	case 34: return std::make_unique<ParameterExpression>(0, "item", 35);
	case 35: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD), "RG_KOKIRI_SWORD"}; }, "RG_KOKIRI_SWORD", false);
	case 36: return std::make_unique<MatchArm>(37, 39, true);
	case 37: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{38});
	case 38: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_MASTER_SWORD_JUMPSLASH), "ED_MASTER_SWORD_JUMPSLASH"}; }, "ED_MASTER_SWORD_JUMPSLASH", false);
	case 39: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{40});
	case 40: return std::make_unique<ParameterExpression>(0, "item", 41);
	case 41: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MASTER_SWORD), "RG_MASTER_SWORD"}; }, "RG_MASTER_SWORD", false);
	case 42: return std::make_unique<MatchArm>(43, 45, true);
	case 43: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{44});
	case 44: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_LONG_JUMPSLASH), "ED_LONG_JUMPSLASH"}; }, "ED_LONG_JUMPSLASH", false);
	case 45: return std::make_unique<OrExpression>(std::vector<index_t>{46, 49});
	case 46: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{47});
	case 47: return std::make_unique<ParameterExpression>(0, "item", 48);
	case 48: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BIGGORON_SWORD), "RG_BIGGORON_SWORD"}; }, "RG_BIGGORON_SWORD", false);
	case 49: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{50});
	case 50: return std::make_unique<ParameterExpression>(0, "item", 51);
	case 51: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_STICKS), "RG_STICKS"}; }, "RG_STICKS", false);
	case 52: return std::make_unique<MatchArm>(53, 55, true);
	case 53: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{54});
	case 54: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_BOMB_THROW), "ED_BOMB_THROW"}; }, "ED_BOMB_THROW", false);
	case 55: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{56});
	case 56: return std::make_unique<ParameterExpression>(0, "item", 57);
	case 57: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOMB_BAG), "RG_BOMB_BAG"}; }, "RG_BOMB_BAG", false);
	case 58: return std::make_unique<MatchArm>(59, 61, true);
	case 59: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{60});
	case 60: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_BOOMERANG), "ED_BOOMERANG"}; }, "ED_BOOMERANG", false);
	case 61: return std::make_unique<OrExpression>(std::vector<index_t>{62, 65});
	case 62: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{63});
	case 63: return std::make_unique<ParameterExpression>(0, "item", 64);
	case 64: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOOMERANG), "RG_BOOMERANG"}; }, "RG_BOOMERANG", false);
	case 65: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{66});
	case 66: return std::make_unique<ParameterExpression>(0, "item", 67);
	case 67: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_DINS_FIRE), "RG_DINS_FIRE"}; }, "RG_DINS_FIRE", false);
	case 68: return std::make_unique<MatchArm>(69, 71, true);
	case 69: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{70});
	case 70: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_HOOKSHOT), "ED_HOOKSHOT"}; }, "ED_HOOKSHOT", false);
	case 71: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{72});
	case 72: return std::make_unique<ParameterExpression>(0, "item", 73);
	case 73: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_HOOKSHOT), "RG_HOOKSHOT"}; }, "RG_HOOKSHOT", false);
	case 74: return std::make_unique<MatchArm>(75, 77, true);
	case 75: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{76});
	case 76: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_LONGSHOT), "ED_LONGSHOT"}; }, "ED_LONGSHOT", false);
	case 77: return std::make_unique<OrExpression>(std::vector<index_t>{78, 81});
	case 78: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{79});
	case 79: return std::make_unique<ParameterExpression>(0, "item", 80);
	case 80: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_LONGSHOT), "RG_LONGSHOT"}; }, "RG_LONGSHOT", false);
	case 81: return std::make_unique<AndExpression>(std::vector<index_t>{23, 82});
	case 82: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{83});
	case 83: return std::make_unique<ParameterExpression>(0, "item", 84);
	case 84: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOMBCHU_5), "RG_BOMBCHU_5"}; }, "RG_BOMBCHU_5", false);
	case 85: return std::make_unique<MatchArm>(86, 88, false);
	case 86: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{87});
	case 87: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_FAR), "ED_FAR"}; }, "ED_FAR", false);
	case 88: return std::make_unique<OrExpression>(std::vector<index_t>{89, 92});
	case 89: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{90});
	case 90: return std::make_unique<ParameterExpression>(0, "item", 91);
	case 91: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FAIRY_SLINGSHOT), "RG_FAIRY_SLINGSHOT"}; }, "RG_FAIRY_SLINGSHOT", false);
	case 92: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{93});
	case 93: return std::make_unique<ParameterExpression>(0, "item", 94);
	case 94: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FAIRY_BOW), "RG_FAIRY_BOW"}; }, "RG_FAIRY_BOW", false);
	case 95: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{96});
	case 96: return std::make_unique<ParameterExpression>(0, "key", 97);
	case 97: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR), "LOGIC_DEKU_TREE_CLEAR"}; }, "LOGIC_DEKU_TREE_CLEAR", true);
	case 98: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{99, 6, 8, 9, 11, 13}, 14);
	case 99: return std::make_unique<ParameterExpression>(0, "e", 100);
	case 100: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_WITHERED_DEKU_BABA), "RE_WITHERED_DEKU_BABA"}; }, "RE_WITHERED_DEKU_BABA", false);
	case 101: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 102);
	case 102: return std::make_unique<OrExpression>(std::vector<index_t>{65, 103});
	case 103: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{104});
	case 104: return std::make_unique<ParameterExpression>(0, "item", 105);
	case 105: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FIRE_ARROWS), "RG_FIRE_ARROWS"}; }, "RG_FIRE_ARROWS", false);
	case 106: return std::make_unique<OrExpression>(std::vector<index_t>{107, 71});
	case 107: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{108});
	case 108: return std::make_unique<ParameterExpression>(0, "item", 109);
	case 109: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_CLIMB), "RG_CLIMB"}; }, "RG_CLIMB", false);
	case 110: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{111});
	case 111: return std::make_unique<ParameterExpression>(0, "key", 112);
	case 112: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_1F_BROKE_WEB), "LOGIC_DEKU_TREE_1F_BROKE_WEB"}; }, "LOGIC_DEKU_TREE_1F_BROKE_WEB", true);
	case 113: return std::make_unique<OrExpression>(std::vector<index_t>{107, 78});
	case 114: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{115});
	case 115: return std::make_unique<ParameterExpression>(0, "key", 116);
	case 116: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB), "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB"}; }, "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", true);
	case 117: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 118);
	case 118: return std::make_unique<OrExpression>(std::vector<index_t>{119, 62});
	case 119: return std::make_unique<FunctionCallExpression>("can_use_sword", std::vector<index_t>{}, 120);
	case 120: return std::make_unique<OrExpression>(std::vector<index_t>{33, 39, 46});
	case 121: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 122);
	case 122: return std::make_unique<OrExpression>(std::vector<index_t>{123, 89, 92, 127, 65});
	case 123: return std::make_unique<FunctionCallExpression>("can_jumpslash", std::vector<index_t>{}, 124);
	case 124: return std::make_unique<OrExpression>(std::vector<index_t>{125, 27});
	case 125: return std::make_unique<FunctionCallExpression>("can_jumpslash_except_hammer", std::vector<index_t>{}, 126);
	case 126: return std::make_unique<OrExpression>(std::vector<index_t>{49, 119});
	case 127: return std::make_unique<FunctionCallExpression>("has_explosives", std::vector<index_t>{}, 128);
	case 128: return std::make_unique<OrExpression>(std::vector<index_t>{55, 82});
	case 129: return std::make_unique<OrExpression>(std::vector<index_t>{49, 92});
	case 130: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{131, 6, 8, 9, 11, 13}, 14);
	case 131: return std::make_unique<ParameterExpression>(0, "e", 132);
	case 132: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_DEKU_BABA), "RE_DEKU_BABA"}; }, "RE_DEKU_BABA", false);
	case 133: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{134});
	case 134: return std::make_unique<ParameterExpression>(0, "condition", 135);
	case 135: return std::make_unique<ConditionExpression>(136);
	case 136: return std::make_unique<FunctionCallExpression>("can_hit_eye_targets", std::vector<index_t>{}, 137);
	case 137: return std::make_unique<OrExpression>(std::vector<index_t>{92, 89});
	case 138: return std::make_unique<AndExpression>(std::vector<index_t>{139, 145});
	case 139: return std::make_unique<OrExpression>(std::vector<index_t>{107, 71, 140});
	case 140: return std::make_unique<AndExpression>(std::vector<index_t>{141, 142});
	case 141: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 142: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{143});
	case 143: return std::make_unique<ParameterExpression>(0, "item", 144);
	case 144: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME), "RG_SONG_OF_TIME"}; }, "RG_SONG_OF_TIME", false);
	case 145: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{146});
	case 146: return std::make_unique<ParameterExpression>(0, "condition", 147);
	case 147: return std::make_unique<ConditionExpression>(148);
	case 148: return std::make_unique<OrExpression>(std::vector<index_t>{82, 149, 154});
	case 149: return std::make_unique<AndExpression>(std::vector<index_t>{55, 150});
	case 150: return std::make_unique<OrExpression>(std::vector<index_t>{142, 141, 151});
	case 151: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{152});
	case 152: return std::make_unique<ParameterExpression>(0, "item", 153);
	case 153: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS), "RG_HOVER_BOOTS"}; }, "RG_HOVER_BOOTS", false);
	case 154: return std::make_unique<AndExpression>(std::vector<index_t>{27, 155});
	case 155: return std::make_unique<OrExpression>(std::vector<index_t>{140, 156});
	case 156: return std::make_unique<AndExpression>(std::vector<index_t>{157, 107});
	case 157: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{158});
	case 158: return std::make_unique<ParameterExpression>(0, "key", 159);
	case 159: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_COMPASS_GS), "RT_DEKU_MQ_COMPASS_GS"}; }, "RT_DEKU_MQ_COMPASS_GS", false);
	case 160: return std::make_unique<FunctionCallExpression>("blast_or_smash", std::vector<index_t>{}, 161);
	case 161: return std::make_unique<OrExpression>(std::vector<index_t>{127, 27});
	case 162: return std::make_unique<AndExpression>(std::vector<index_t>{49, 163});
	case 163: return std::make_unique<OrExpression>(std::vector<index_t>{164, 141, 151});
	case 164: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{165});
	case 165: return std::make_unique<ParameterExpression>(0, "key", 166);
	case 166: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK), "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK"}; }, "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", true);
	case 167: return std::make_unique<AndExpression>(std::vector<index_t>{133, 168, 171});
	case 168: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{169});
	case 169: return std::make_unique<ParameterExpression>(0, "key", 170);
	case 170: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM), "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM"}; }, "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", true);
	case 171: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{172});
	case 172: return std::make_unique<ParameterExpression>(0, "condition", 173);
	case 173: return std::make_unique<ConditionExpression>(49);
	case 174: return std::make_unique<OrExpression>(std::vector<index_t>{141, 175, 178, 164, 151});
	case 175: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{176});
	case 176: return std::make_unique<ParameterExpression>(0, "key", 177);
	case 177: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_B1_SKIP), "RT_DEKU_B1_SKIP"}; }, "RT_DEKU_B1_SKIP", false);
	case 178: return std::make_unique<FunctionCallExpression>("can_ground_jump", std::vector<index_t>{179}, 180);
	case 179: return std::make_unique<ParameterExpression>(0, "hasBombflower", 12);
	case 180: return std::make_unique<AndExpression>(std::vector<index_t>{181, 184, 196});
	case 181: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{182});
	case 182: return std::make_unique<ParameterExpression>(0, "key", 183);
	case 183: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_GROUND_JUMP), "RT_GROUND_JUMP"}; }, "RT_GROUND_JUMP", false);
	case 184: return std::make_unique<FunctionCallExpression>("can_standing_shield", std::vector<index_t>{}, 185);
	case 185: return std::make_unique<OrExpression>(std::vector<index_t>{186, 189, 193});
	case 186: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{187});
	case 187: return std::make_unique<ParameterExpression>(0, "item", 188);
	case 188: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MIRROR_SHIELD), "RG_MIRROR_SHIELD"}; }, "RG_MIRROR_SHIELD", false);
	case 189: return std::make_unique<AndExpression>(std::vector<index_t>{141, 190});
	case 190: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{191});
	case 191: return std::make_unique<ParameterExpression>(0, "item", 192);
	case 192: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_HYLIAN_SHIELD), "RG_HYLIAN_SHIELD"}; }, "RG_HYLIAN_SHIELD", false);
	case 193: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{194});
	case 194: return std::make_unique<ParameterExpression>(0, "item", 195);
	case 195: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD), "RG_DEKU_SHIELD"}; }, "RG_DEKU_SHIELD", false);
	case 196: return std::make_unique<OrExpression>(std::vector<index_t>{55, 197});
	case 197: return std::make_unique<AndExpression>(std::vector<index_t>{198, 199});
	case 198: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("hasBombflower"); }, "hasBombflower", false);
	case 199: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{200});
	case 200: return std::make_unique<ParameterExpression>(0, "item", 201);
	case 201: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GORONS_BRACELET), "RG_GORONS_BRACELET"}; }, "RG_GORONS_BRACELET", false);
	case 202: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{203, 6, 8, 9, 11, 13}, 14);
	case 203: return std::make_unique<ParameterExpression>(0, "e", 204);
	case 204: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_MAD_SCRUB), "RE_MAD_SCRUB"}; }, "RE_MAD_SCRUB", false);
	case 205: return std::make_unique<OrExpression>(std::vector<index_t>{103, 206});
	case 206: return std::make_unique<AndExpression>(std::vector<index_t>{49, 207});
	case 207: return std::make_unique<OrExpression>(std::vector<index_t>{208, 211});
	case 208: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{209});
	case 209: return std::make_unique<ParameterExpression>(0, "key", 210);
	case 210: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_LOG), "RT_DEKU_MQ_LOG"}; }, "RT_DEKU_MQ_LOG", false);
	case 211: return std::make_unique<AndExpression>(std::vector<index_t>{212, 213});
	case 212: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 213: return std::make_unique<FunctionCallExpression>("can_shield", std::vector<index_t>{}, 214);
	case 214: return std::make_unique<OrExpression>(std::vector<index_t>{186, 190, 193});
	case 215: return std::make_unique<OrExpression>(std::vector<index_t>{208, 211, 78, 216});
	case 216: return std::make_unique<AndExpression>(std::vector<index_t>{71, 217});
	case 217: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{218});
	case 218: return std::make_unique<ParameterExpression>(0, "item", 219);
	case 219: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_IRON_BOOTS), "RG_IRON_BOOTS"}; }, "RG_IRON_BOOTS", false);
	case 220: return std::make_unique<AndExpression>(std::vector<index_t>{221, 224});
	case 221: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{222});
	case 222: return std::make_unique<ParameterExpression>(0, "key", 223);
	case 223: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES), "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES"}; }, "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", true);
	case 224: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{225, 227, 8}, 229);
	case 225: return std::make_unique<ParameterExpression>(0, "e", 226);
	case 226: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_BIG_SKULLTULA), "RE_BIG_SKULLTULA"}; }, "RE_BIG_SKULLTULA", false);
	case 227: return std::make_unique<ParameterExpression>(1, "distance", 228);
	case 228: return std::make_unique<TernaryExpression>(142, 7, 32);
	case 229: return std::make_unique<OrExpression>(std::vector<index_t>{230, 234});
	case 230: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{231, 232, 233, 9, 11, 13}, 14);
	case 231: return std::make_unique<ParameterExpression>(0, "e", 15);
	case 232: return std::make_unique<ParameterExpression>(1, "distance", 21);
	case 233: return std::make_unique<ParameterExpression>(2, "wall_or_floor", 23);
	case 234: return std::make_unique<MatchExpression<bool>>(15, std::vector<index_t>{235});
	case 235: return std::make_unique<MatchArm>(17, 2, false);
	case 236: return std::make_unique<OrExpression>(std::vector<index_t>{208, 211, 78, 237, 240});
	case 237: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{238});
	case 238: return std::make_unique<ParameterExpression>(0, "item", 239);
	case 239: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE), "RG_BRONZE_SCALE"}; }, "RG_BRONZE_SCALE", false);
	case 240: return std::make_unique<AndExpression>(std::vector<index_t>{217, 241});
	case 241: return std::make_unique<OrExpression>(std::vector<index_t>{141, 71});
	case 242: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{243});
	case 243: return std::make_unique<ParameterExpression>(0, "condition", 244);
	case 244: return std::make_unique<ConditionExpression>(245);
	case 245: return std::make_unique<AndExpression>(std::vector<index_t>{202, 246});
	case 246: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{247, 6, 8, 9, 11, 13}, 14);
	case 247: return std::make_unique<ParameterExpression>(0, "e", 248);
	case 248: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_KEESE), "RE_KEESE"}; }, "RE_KEESE", false);
	case 249: return std::make_unique<AndExpression>(std::vector<index_t>{250, 253});
	case 250: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{251});
	case 251: return std::make_unique<ParameterExpression>(0, "item", 252);
	case 252: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_CRAWL), "RG_CRAWL"}; }, "RG_CRAWL", false);
	case 253: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{254});
	case 254: return std::make_unique<ParameterExpression>(0, "condition", 255);
	case 255: return std::make_unique<ConditionExpression>(256);
	case 256: return std::make_unique<OrExpression>(std::vector<index_t>{257, 92});
	case 257: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 258);
	case 258: return std::make_unique<OrExpression>(std::vector<index_t>{101, 49});
	case 259: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{260});
	case 260: return std::make_unique<ParameterExpression>(0, "item", 261);
	case 261: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_POWER_BRACELET), "RG_POWER_BRACELET"}; }, "RG_POWER_BRACELET", false);
	case 262: return std::make_unique<AndExpression>(std::vector<index_t>{263, 266});
	case 263: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{264});
	case 264: return std::make_unique<ParameterExpression>(0, "key", 265);
	case 265: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_B1_BROKE_WEB), "LOGIC_DEKU_TREE_B1_BROKE_WEB"}; }, "LOGIC_DEKU_TREE_B1_BROKE_WEB", true);
	case 266: return std::make_unique<OrExpression>(std::vector<index_t>{237, 217});
	case 267: return std::make_unique<AndExpression>(std::vector<index_t>{107, 268});
	case 268: return std::make_unique<OrExpression>(std::vector<index_t>{237, 71});
	case 269: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{270});
	case 270: return std::make_unique<ParameterExpression>(0, "condition", 271);
	case 271: return std::make_unique<ConditionExpression>(272);
	case 272: return std::make_unique<FunctionCallExpression>("can_reflect_nuts", std::vector<index_t>{}, 273);
	case 273: return std::make_unique<OrExpression>(std::vector<index_t>{193, 189});
	case 274: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{225, 6, 8}, 229);
	case 275: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{276});
	case 276: return std::make_unique<ParameterExpression>(0, "condition", 277);
	case 277: return std::make_unique<ConditionExpression>(278);
	case 278: return std::make_unique<OrExpression>(std::vector<index_t>{272, 27});
	case 279: return std::make_unique<OrExpression>(std::vector<index_t>{162, 280});
	case 280: return std::make_unique<AndExpression>(std::vector<index_t>{281, 141, 92});
	case 281: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{282});
	case 282: return std::make_unique<ParameterExpression>(0, "key", 283);
	case 283: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BOW_WEBS), "RT_DEKU_B1_BOW_WEBS"}; }, "RT_DEKU_B1_BOW_WEBS", false);
	case 284: return std::make_unique<OrExpression>(std::vector<index_t>{107, 285});
	case 285: return std::make_unique<AndExpression>(std::vector<index_t>{141, 78});
	case 286: return std::make_unique<OrExpression>(std::vector<index_t>{141, 175, 178, 164});
	case 287: return std::make_unique<OrExpression>(std::vector<index_t>{237, 288});
	case 288: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{289});
	case 289: return std::make_unique<ParameterExpression>(0, "key", 290);
	case 290: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG), "RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG"}; }, "RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG", false);
	case 291: return std::make_unique<AndExpression>(std::vector<index_t>{253, 292});
	case 292: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{293});
	case 293: return std::make_unique<ParameterExpression>(0, "condition", 294);
	case 294: return std::make_unique<ConditionExpression>(160);
	case 295: return std::make_unique<AndExpression>(std::vector<index_t>{253, 250});
	case 296: return std::make_unique<OrExpression>(std::vector<index_t>{297, 305});
	case 297: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 298);
	case 298: return std::make_unique<OrExpression>(std::vector<index_t>{299, 302, 142});
	case 299: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{300});
	case 300: return std::make_unique<ParameterExpression>(0, "item", 301);
	case 301: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ZELDAS_LULLABY), "RG_ZELDAS_LULLABY"}; }, "RG_ZELDAS_LULLABY", false);
	case 302: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{303});
	case 303: return std::make_unique<ParameterExpression>(0, "item", 304);
	case 304: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_EPONAS_SONG), "RG_EPONAS_SONG"}; }, "RG_EPONAS_SONG", false);
	case 305: return std::make_unique<AndExpression>(std::vector<index_t>{212, 306, 309, 312});
	case 306: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{307});
	case 307: return std::make_unique<ParameterExpression>(0, "item", 308);
	case 308: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MAGIC_BEAN), "RG_MAGIC_BEAN"}; }, "RG_MAGIC_BEAN", false);
	case 309: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{310});
	case 310: return std::make_unique<ParameterExpression>(0, "item", 311);
	case 311: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL), "RG_KOKIRI_FOREST_BEAN_SOUL"}; }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 312: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{313});
	case 313: return std::make_unique<ParameterExpression>(0, "item", 314);
	case 314: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS), "RG_SONG_OF_STORMS"}; }, "RG_SONG_OF_STORMS", false);
	case 315: return std::make_unique<AndExpression>(std::vector<index_t>{212, 316, 33, 193});
	case 316: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{317});
	case 317: return std::make_unique<ParameterExpression>(0, "item", 318);
	case 318: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI), "RG_SPEAK_KOKIRI"}; }, "RG_SPEAK_KOKIRI", false);
	case 319: return std::make_unique<TernaryExpression>(212, 320, 326);
	case 320: return std::make_unique<FunctionCallExpression>("can_climb_ladder", std::vector<index_t>{}, 321);
	case 321: return std::make_unique<OrExpression>(std::vector<index_t>{107, 322});
	case 322: return std::make_unique<AndExpression>(std::vector<index_t>{323, 71});
	case 323: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{324});
	case 324: return std::make_unique<ParameterExpression>(0, "key", 325);
	case 325: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_HOOKSHOT_LADDERS), "RT_HOOKSHOT_LADDERS"}; }, "RT_HOOKSHOT_LADDERS", false);
	case 326: return std::make_unique<OrExpression>(std::vector<index_t>{107, 151});
	case 327: return std::make_unique<OrExpression>(std::vector<index_t>{328, 331, 336});
	case 328: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{329});
	case 329: return std::make_unique<ParameterExpression>(0, "key", 330);
	case 330: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD), "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD"}; }, "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", true);
	case 331: return std::make_unique<ComparisonExpression>("==", 332, 335);
	case 332: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{333});
	case 333: return std::make_unique<ParameterExpression>(0, "key", 334);
	case 334: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_FOREST), "RSK_FOREST"}; }, "RSK_FOREST", false);
	case 335: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_CLOSED_FOREST_OFF), "RO_CLOSED_FOREST_OFF"}; }, "RO_CLOSED_FOREST_OFF", false);
	case 336: return std::make_unique<AndExpression>(std::vector<index_t>{141, 337});
	case 337: return std::make_unique<OrExpression>(std::vector<index_t>{274, 338});
	case 338: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{339});
	case 339: return std::make_unique<ParameterExpression>(0, "key", 340);
	case 340: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_FOREST_TEMPLE_CLEAR), "LOGIC_FOREST_TEMPLE_CLEAR"}; }, "LOGIC_FOREST_TEMPLE_CLEAR", true);
	case 341: return std::make_unique<OrExpression>(std::vector<index_t>{107, 71, 342});
	case 342: return std::make_unique<AndExpression>(std::vector<index_t>{141, 343});
	case 343: return std::make_unique<OrExpression>(std::vector<index_t>{344, 348});
	case 344: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{345, 347});
	case 345: return std::make_unique<ParameterExpression>(0, "reg", 346);
	case 346: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST), "RR_KOKIRI_FOREST"}; }, "RR_KOKIRI_FOREST", false);
	case 347: return std::make_unique<ParameterExpression>(1, "bean", 311);
	case 348: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{349});
	case 349: return std::make_unique<ParameterExpression>(0, "key", 350);
	case 350: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_UNINTUITIVE_JUMPS), "RT_UNINTUITIVE_JUMPS"}; }, "RT_UNINTUITIVE_JUMPS", false);
	case 351: return std::make_unique<AndExpression>(std::vector<index_t>{141, 344});
	case 352: return std::make_unique<OrExpression>(std::vector<index_t>{141, 353, 95});
	case 353: return std::make_unique<ComparisonExpression>("!=", 332, 354);
	case 354: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_CLOSED_FOREST_ON), "RO_CLOSED_FOREST_ON"}; }, "RO_CLOSED_FOREST_ON", false);
	case 355: return std::make_unique<AndExpression>(std::vector<index_t>{212, 356});
	case 356: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{357});
	case 357: return std::make_unique<ParameterExpression>(0, "item", 358);
	case 358: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_OPEN_CHEST), "RG_OPEN_CHEST"}; }, "RG_OPEN_CHEST", false);
	case 359: return std::make_unique<OrExpression>(std::vector<index_t>{212, 360});
	case 360: return std::make_unique<AndExpression>(std::vector<index_t>{361, 366});
	case 361: return std::make_unique<ComparisonExpression>("!=", 362, 365);
	case 362: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{363});
	case 363: return std::make_unique<ParameterExpression>(0, "key", 364);
	case 364: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SHUFFLE_DUNGEON_ENTRANCES), "RSK_SHUFFLE_DUNGEON_ENTRANCES"}; }, "RSK_SHUFFLE_DUNGEON_ENTRANCES", false);
	case 365: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF), "RO_DUNGEON_ENTRANCE_SHUFFLE_OFF"}; }, "RO_DUNGEON_ENTRANCE_SHUFFLE_OFF", false);
	case 366: return std::make_unique<OrExpression>(std::vector<index_t>{331, 328});
	case 367: return std::make_unique<OrExpression>(std::vector<index_t>{368, 331, 328});
	case 368: return std::make_unique<AndExpression>(std::vector<index_t>{141, 369});
	case 369: return std::make_unique<OrExpression>(std::vector<index_t>{274, 95});
	case 370: return std::make_unique<AndExpression>(std::vector<index_t>{141, 371});
	case 371: return std::make_unique<OrExpression>(std::vector<index_t>{344, 151});
	case 372: return std::make_unique<FunctionCallExpression>("can_open_storms_grotto", std::vector<index_t>{}, 373);
	case 373: return std::make_unique<AndExpression>(std::vector<index_t>{312, 374});
	case 374: return std::make_unique<OrExpression>(std::vector<index_t>{375, 378});
	case 375: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{376});
	case 376: return std::make_unique<ParameterExpression>(0, "item", 377);
	case 377: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_STONE_OF_AGONY), "RG_STONE_OF_AGONY"}; }, "RG_STONE_OF_AGONY", false);
	case 378: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{379});
	case 379: return std::make_unique<ParameterExpression>(0, "key", 380);
	case 380: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_GROTTOS_WITHOUT_AGONY), "RT_GROTTOS_WITHOUT_AGONY"}; }, "RT_GROTTOS_WITHOUT_AGONY", false);
	case 381: return std::make_unique<OrExpression>(std::vector<index_t>{382, 49});
	case 382: return std::make_unique<FunctionCallExpression>("call_gossip_fairy", std::vector<index_t>{}, 383);
	case 383: return std::make_unique<OrExpression>(std::vector<index_t>{297, 384});
	case 384: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{385});
	case 385: return std::make_unique<ParameterExpression>(0, "item", 386);
	case 386: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SUNS_SONG), "RG_SUNS_SONG"}; }, "RG_SUNS_SONG", false);
	case 387: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 388);
	case 388: return std::make_unique<OrExpression>(std::vector<index_t>{33, 62, 127, 199, 39, 27, 46, 389});
	case 389: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{390});
	case 390: return std::make_unique<ParameterExpression>(0, "item", 391);
	case 391: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GIANTS_KNIFE), "RG_GIANTS_KNIFE"}; }, "RG_GIANTS_KNIFE", false);
	case 392: return std::make_unique<ComparisonExpression>("==", 393, 396);
	case 393: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{394});
	case 394: return std::make_unique<ParameterExpression>(0, "key", 395);
	case 395: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_KAK_GATE), "RSK_KAK_GATE"}; }, "RSK_KAK_GATE", false);
	case 396: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_KAK_GATE_OPEN), "RO_KAK_GATE_OPEN"}; }, "RO_KAK_GATE_OPEN", false);
	case 397: return std::make_unique<ComparisonExpression>("==", 398, 401);
	case 398: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{399});
	case 399: return std::make_unique<ParameterExpression>(0, "key", 400);
	case 400: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_GERUDO_FORTRESS), "RSK_GERUDO_FORTRESS"}; }, "RSK_GERUDO_FORTRESS", false);
	case 401: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_GF_CARPENTERS_FREE), "RO_GF_CARPENTERS_FREE"}; }, "RO_GF_CARPENTERS_FREE", false);
	case 402: return std::make_unique<OrExpression>(std::vector<index_t>{397, 403});
	case 403: return std::make_unique<ComparisonExpression>("==", 398, 404);
	case 404: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_GF_CARPENTERS_FAST), "RO_GF_CARPENTERS_FAST"}; }, "RO_GF_CARPENTERS_FAST", false);
	case 405: return std::make_unique<ComparisonExpression>("==", 406, 409);
	case 406: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{407});
	case 407: return std::make_unique<ParameterExpression>(0, "key", 408);
	case 408: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SKIP_EPONA_RACE), "RSK_SKIP_EPONA_RACE"}; }, "RSK_SKIP_EPONA_RACE", false);
	case 409: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_GENERIC_YES), "RO_GENERIC_YES"}; }, "RO_GENERIC_YES", false);
	case 410: return std::make_unique<ComparisonExpression>(">=", 411, 412);
	case 411: return std::make_unique<ExternFunctionCallExpression>(collected_triforce_pieces_adapter, "collected_triforce_pieces", std::vector<index_t>{});
	case 412: return std::make_unique<ExternFunctionCallExpression>(required_triforce_pieces_adapter, "required_triforce_pieces", std::vector<index_t>{});
	case 413: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{414});
	case 414: return std::make_unique<ParameterExpression>(0, "item", 415);
	case 415: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SARIAS_SONG), "RG_SARIAS_SONG"}; }, "RG_SARIAS_SONG", false);
	case 416: return std::make_unique<ComparisonExpression>("==", 417, 409);
	case 417: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{418});
	case 418: return std::make_unique<ParameterExpression>(0, "key", 419);
	case 419: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SKIP_CHILD_ZELDA), "RSK_SKIP_CHILD_ZELDA"}; }, "RSK_SKIP_CHILD_ZELDA", false);
	case 420: return std::make_unique<ComparisonExpression>("==", 421, 424);
	case 421: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{422});
	case 422: return std::make_unique<ParameterExpression>(0, "key", 423);
	case 423: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SELECTED_STARTING_AGE), "RSK_SELECTED_STARTING_AGE"}; }, "RSK_SELECTED_STARTING_AGE", false);
	case 424: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_AGE_ADULT), "RO_AGE_ADULT"}; }, "RO_AGE_ADULT", false);
	case 425: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{426});
	case 426: return std::make_unique<ParameterExpression>(0, "item", 427);
	case 427: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MINUET_OF_FOREST), "RG_MINUET_OF_FOREST"}; }, "RG_MINUET_OF_FOREST", false);
	case 428: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{429});
	case 429: return std::make_unique<ParameterExpression>(0, "item", 430);
	case 430: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOLERO_OF_FIRE), "RG_BOLERO_OF_FIRE"}; }, "RG_BOLERO_OF_FIRE", false);
	case 431: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{432});
	case 432: return std::make_unique<ParameterExpression>(0, "item", 433);
	case 433: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SERENADE_OF_WATER), "RG_SERENADE_OF_WATER"}; }, "RG_SERENADE_OF_WATER", false);
	case 434: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{435});
	case 435: return std::make_unique<ParameterExpression>(0, "item", 436);
	case 436: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_NOCTURNE_OF_SHADOW), "RG_NOCTURNE_OF_SHADOW"}; }, "RG_NOCTURNE_OF_SHADOW", false);
	case 437: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{438});
	case 438: return std::make_unique<ParameterExpression>(0, "item", 439);
	case 439: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_REQUIEM_OF_SPIRIT), "RG_REQUIEM_OF_SPIRIT"}; }, "RG_REQUIEM_OF_SPIRIT", false);
	case 440: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{441});
	case 441: return std::make_unique<ParameterExpression>(0, "item", 442);
	case 442: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_PRELUDE_OF_LIGHT), "RG_PRELUDE_OF_LIGHT"}; }, "RG_PRELUDE_OF_LIGHT", false);
	case 443: return std::make_unique<FunctionCallExpression>("can_break_lower_beehives", std::vector<index_t>{}, 444);
	case 444: return std::make_unique<OrExpression>(std::vector<index_t>{445, 55});
	case 445: return std::make_unique<FunctionCallExpression>("can_break_upper_beehives", std::vector<index_t>{}, 446);
	case 446: return std::make_unique<OrExpression>(std::vector<index_t>{447, 449, 453});
	case 447: return std::make_unique<FunctionCallExpression>("hookshot_or_boomerang", std::vector<index_t>{}, 448);
	case 448: return std::make_unique<OrExpression>(std::vector<index_t>{71, 62});
	case 449: return std::make_unique<AndExpression>(std::vector<index_t>{450, 82});
	case 450: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{451});
	case 451: return std::make_unique<ParameterExpression>(0, "key", 452);
	case 452: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_BOMBCHU_BEEHIVES), "RT_BOMBCHU_BEEHIVES"}; }, "RT_BOMBCHU_BEEHIVES", false);
	case 453: return std::make_unique<AndExpression>(std::vector<index_t>{454, 137});
	case 454: return std::make_unique<ConversionExpression<bool>>(455);
	case 455: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{456});
	case 456: return std::make_unique<ParameterExpression>(0, "key", 457);
	case 457: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SLINGBOW_BREAK_BEEHIVES), "RSK_SLINGBOW_BREAK_BEEHIVES"}; }, "RSK_SLINGBOW_BREAK_BEEHIVES", false);
	case 458: return std::make_unique<AndExpression>(std::vector<index_t>{141, 302, 459});
	case 459: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{460});
	case 460: return std::make_unique<ParameterExpression>(0, "key", 461);
	case 461: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_LINKS_COW), "LOGIC_LINKS_COW"}; }, "LOGIC_LINKS_COW", true);
	case 462: return std::make_unique<FunctionCallExpression>("has_bottle", std::vector<index_t>{}, 463);
	case 463: return std::make_unique<ComparisonExpression>(">=", 464, 10);
	case 464: return std::make_unique<ExternFunctionCallExpression>(bottle_count_adapter, "bottle_count", std::vector<index_t>{});
	case 465: return std::make_unique<AndExpression>(std::vector<index_t>{141, 466});
	case 466: return std::make_unique<OrExpression>(std::vector<index_t>{344, 151, 62});
	case 467: return std::make_unique<AndExpression>(std::vector<index_t>{141, 62});
	case 468: return std::make_unique<AndExpression>(std::vector<index_t>{141, 151});
	case 469: return std::make_unique<AndExpression>(std::vector<index_t>{212, 470, 472});
	case 470: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{471, 6, 8, 9, 11, 13}, 14);
	case 471: return std::make_unique<ParameterExpression>(0, "e", 18);
	case 472: return std::make_unique<FunctionCallExpression>("can_get_night_time_gs", std::vector<index_t>{}, 473);
	case 473: return std::make_unique<AndExpression>(std::vector<index_t>{474, 475});
	case 474: return std::make_unique<ExternFunctionCallExpression>(at_night_adapter, "at_night", std::vector<index_t>{});
	case 475: return std::make_unique<OrExpression>(std::vector<index_t>{384, 476});
	case 476: return std::make_unique<NotExpression>(477);
	case 477: return std::make_unique<ConversionExpression<bool>>(478);
	case 478: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{479});
	case 479: return std::make_unique<ParameterExpression>(0, "key", 480);
	case 480: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SKULLS_SUNS_SONG), "RSK_SKULLS_SUNS_SONG"}; }, "RSK_SKULLS_SUNS_SONG", false);
	case 481: return std::make_unique<AndExpression>(std::vector<index_t>{482, 470});
	case 482: return std::make_unique<FunctionCallExpression>("can_spawn_soil_skull", std::vector<index_t>{483}, 484);
	case 483: return std::make_unique<ParameterExpression>(0, "bean", 311);
	case 484: return std::make_unique<AndExpression>(std::vector<index_t>{212, 485, 488});
	case 485: return std::make_unique<ExternFunctionCallExpression>(can_use_adapter, "can_use", std::vector<index_t>{486});
	case 486: return std::make_unique<ParameterExpression>(0, "item", 487);
	case 487: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOTTLE_WITH_BUGS), "RG_BOTTLE_WITH_BUGS"}; }, "RG_BOTTLE_WITH_BUGS", false);
	case 488: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{489});
	case 489: return std::make_unique<ParameterExpression>(0, "item", 490);
	case 490: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("bean"); }, "bean", false);
	case 491: return std::make_unique<AndExpression>(std::vector<index_t>{141, 472, 492});
	case 492: return std::make_unique<OrExpression>(std::vector<index_t>{493, 540});
	case 493: return std::make_unique<FunctionCallExpression>("can_get_drop", std::vector<index_t>{471, 494, 495}, 496);
	case 494: return std::make_unique<ParameterExpression>(1, "distance", 60);
	case 495: return std::make_unique<ParameterExpression>(2, "above_link", 12);
	case 496: return std::make_unique<AndExpression>(std::vector<index_t>{497, 498});
	case 497: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{231, 232, 8, 9, 11, 13}, 14);
	case 498: return std::make_unique<OrExpression>(std::vector<index_t>{499, 521});
	case 499: return std::make_unique<ComparisonExpression>("<=", 500, 519);
	case 500: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{20}, 501);
	case 501: return std::make_unique<MatchExpression<int32_t>>(21, std::vector<index_t>{502, 504, 505, 507, 509, 511, 513, 515, 517});
	case 502: return std::make_unique<MatchArm>(26, 503, false);
	case 503: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{0}; }, "0", false);
	case 504: return std::make_unique<MatchArm>(31, 10, false);
	case 505: return std::make_unique<MatchArm>(37, 506, false);
	case 506: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{2}; }, "2", false);
	case 507: return std::make_unique<MatchArm>(43, 508, false);
	case 508: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{3}; }, "3", false);
	case 509: return std::make_unique<MatchArm>(53, 510, false);
	case 510: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{4}; }, "4", false);
	case 511: return std::make_unique<MatchArm>(59, 512, false);
	case 512: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{5}; }, "5", false);
	case 513: return std::make_unique<MatchArm>(69, 514, false);
	case 514: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{6}; }, "6", false);
	case 515: return std::make_unique<MatchArm>(75, 516, false);
	case 516: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{7}; }, "7", false);
	case 517: return std::make_unique<MatchArm>(86, 518, false);
	case 518: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{8}; }, "8", false);
	case 519: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{520}, 501);
	case 520: return std::make_unique<ParameterExpression>(0, "distance", 38);
	case 521: return std::make_unique<MatchExpression<bool>>(15, std::vector<index_t>{522, 528, 532});
	case 522: return std::make_unique<MatchArm>(17, 523, false);
	case 523: return std::make_unique<FunctionCallExpression>("_can_get_drop_gold_skulltula", std::vector<index_t>{20}, 524);
	case 524: return std::make_unique<MatchExpression<bool>>(21, std::vector<index_t>{525, 68, 527});
	case 525: return std::make_unique<MatchArm>(526, 62, true);
	case 526: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{7, 32, 38, 44, 54, 60});
	case 527: return std::make_unique<MatchArm>(75, 78, false);
	case 528: return std::make_unique<MatchArm>(529, 2, false);
	case 529: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{248, 530, 531});
	case 530: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_FIRE_KEESE), "RE_FIRE_KEESE"}; }, "RE_FIRE_KEESE", false);
	case 531: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_GUAY), "RE_GUAY"}; }, "RE_GUAY", false);
	case 532: return std::make_unique<MatchArm>(533, 534, false);
	case 533: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{});
	case 534: return std::make_unique<OrExpression>(std::vector<index_t>{535, 536});
	case 535: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("above_link"); }, "above_link", false);
	case 536: return std::make_unique<AndExpression>(std::vector<index_t>{537, 62});
	case 537: return std::make_unique<ComparisonExpression>("<=", 500, 538);
	case 538: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{539}, 501);
	case 539: return std::make_unique<ParameterExpression>(0, "distance", 60);
	case 540: return std::make_unique<AndExpression>(std::vector<index_t>{541, 151, 544});
	case 541: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{542});
	case 542: return std::make_unique<ParameterExpression>(0, "key", 543);
	case 543: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_KF_ADULT_GS), "RT_KF_ADULT_GS"}; }, "RT_KF_ADULT_GS", false);
	case 544: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{471, 545, 8, 9, 11, 13}, 14);
	case 545: return std::make_unique<ParameterExpression>(1, "distance", 32);
	case 546: return std::make_unique<AndExpression>(std::vector<index_t>{212, 387});
	case 547: return std::make_unique<AndExpression>(std::vector<index_t>{141, 387});
	case 548: return std::make_unique<AndExpression>(std::vector<index_t>{316, 549});
	case 549: return std::make_unique<ComparisonExpression>("<=", 550, 553);
	case 550: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{551});
	case 551: return std::make_unique<ParameterExpression>(0, "check", 552);
	case 552: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK), "RC_UNKNOWN_CHECK"}; }, "RC_UNKNOWN_CHECK", false);
	case 553: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 554);
	case 554: return std::make_unique<TernaryExpression>(555, 558, 559);
	case 555: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{556});
	case 556: return std::make_unique<ParameterExpression>(0, "item", 557);
	case 557: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_TYCOON_WALLET), "RG_TYCOON_WALLET"}; }, "RG_TYCOON_WALLET", false);
	case 558: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{999}; }, "999", false);
	case 559: return std::make_unique<TernaryExpression>(560, 563, 564);
	case 560: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{561});
	case 561: return std::make_unique<ParameterExpression>(0, "item", 562);
	case 562: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GIANT_WALLET), "RG_GIANT_WALLET"}; }, "RG_GIANT_WALLET", false);
	case 563: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{500}; }, "500", false);
	case 564: return std::make_unique<TernaryExpression>(565, 568, 569);
	case 565: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{566});
	case 566: return std::make_unique<ParameterExpression>(0, "item", 567);
	case 567: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ADULT_WALLET), "RG_ADULT_WALLET"}; }, "RG_ADULT_WALLET", false);
	case 568: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{200}; }, "200", false);
	case 569: return std::make_unique<TernaryExpression>(570, 573, 503);
	case 570: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{571});
	case 571: return std::make_unique<ParameterExpression>(0, "item", 572);
	case 572: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_CHILD_WALLET), "RG_CHILD_WALLET"}; }, "RG_CHILD_WALLET", false);
	case 573: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{99}; }, "99", false);
	default: throw std::out_of_range("Invalid tracker expression index.");
	}
}

uint64_t GetExpressionCount() {
	return 574;
}

LogicExpression* GetExpression(index_t index) {
	static LogicExpressionCache cache(GetExpressionCount(), CreateExpression);
	return cache.GetExpression(index);
}

const LogicRootMap& GetLogicRoots() {
	static const LogicRootMap roots{
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_ENTRYWAY", "RR_DEKU_TREE_LOBBY", 0}, 0},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_ENTRYWAY", "RR_DEKU_TREE_MQ_1F", 0}, 1},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_ENTRYWAY", "RR_KF_OUTSIDE_DEKU_TREE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ENTRYWAY", "RR_DEKU_TREE_BOSS_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_EXIT", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0}, 0},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_EXIT", "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", 0}, 1},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BOSS_ROOM", "LOGIC_DEKU_TREE_CLEAR", 0}, 3},
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_QUEEN_GOHMA", 0}, 95},
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_DEKU_TREE_QUEEN_GOHMA_HEART", 0}, 95},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_EXIT", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_KF_OUTSIDE_DEKU_TREE", 0}, 95},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_STICK_ACCESS", 0}, 98},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_NUT_ACCESS", 0}, 101},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_ENTRYWAY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_2F", 0}, 106},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 110},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_2F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0}, 101},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_1F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_3F", 0}, 113},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0}, 114},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0}, 129},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_2F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "RR_DEKU_TREE_MQ_3F", 0}, 130},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0}, 133},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_2F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", 0}, 138},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0}, 160},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 162},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_1F", 0}, 113},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0}, 133},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0}, 167},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0}, 174},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", 0}, 202},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0}, 101},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 168},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", 0}, 205},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0}, 215},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_STICK_ACCESS", 0}, 98},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_NUT_ACCESS", 0}, 101},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0}, 220},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0}, 236},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0}, 242},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0}, 242},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0}, 249},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", 0}, 253},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0}, 259},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 101},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0}, 250},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", 0}, 262},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0}, 267},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 269},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0}, 101},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_ENTRYWAY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_LOBBY_2F", 0}, 106},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BASEMENT_LOWER", 0}, 110},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0}, 12},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 12},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY_3F", 0}, 113},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0}, 274},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_LOBBY_2F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_COMPASS_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_LOBBY", 0}, 275},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_SLINGSHOT_ROOM", 0}, 275},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_SLINGSHOT_ROOM", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_LOBBY", 0}, 256},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 12},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 279},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_LOBBY", 0}, 284},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0}, 253},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_UPPER", 0}, 286},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0}, 12},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_LOWER", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0}, 133},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0}, 287},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0}, 287},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0}, 253},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0}, 253},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_BACK_ROOM", 0}, 291},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_UPPER", 0}, 295},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0}, 259},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 101},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_LOWER", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0}, 250},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0}, 262},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BASEMENT_UPPER", 0}, 267},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 269},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_HOUSE", "RR_KF_LINKS_PORCH", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KF_LINKS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_FAIRY_ACCESS", 0}, 296},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0}, 315},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_BOULDER_LOOP", 0}, 250},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_LINKS_PORCH", 0}, 319},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_MIDOS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_SARIAS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_HOUSE_OF_TWINS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KNOW_IT_ALL_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KOKIRI_SHOP", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_DEKU_TREE", 0}, 327},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_LOST_WOODS", 0}, 341},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_RUPEE_ALCOVE", 0}, 351},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_LW_BRIDGE_FROM_FOREST", 0}, 352},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_KOKIRI_SWORD_CHEST", 0}, 355},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_BOULDER_LOOP", "RR_KOKIRI_FOREST", 0}, 250},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_STICK_ACCESS", 0}, 117},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_NUT_ACCESS", 0}, 121},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_FAIRY_ACCESS", 0}, 297},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0}, 315},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_DEKU_TREE_ENTRYWAY", 0}, 359},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_KOKIRI_FOREST", 0}, 367},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_LEFT_CHEST", 0}, 356},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_RIGHT_CHEST", 0}, 356},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_LEFT_CHEST", 0}, 356},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_RIGHT_CHEST", 0}, 356},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_MIDOS_HOUSE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_SARIAS_HOUSE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_HOUSE_OF_TWINS", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KNOW_IT_ALL_HOUSE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KOKIRI_SHOP", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_THE_LOST_WOODS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_RUPEE_ALCOVE", 0}, 370},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_STORMS_GROTTO", 0}, 372},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_RUPEE_ALCOVE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FAIRY_ACCESS", 0}, 381},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_BUG_ACCESS", 0}, 387},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FISH_ACCESS", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_CHEST", 0}, 356},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_STORMS_GROTTO", "RR_KF_OUTSIDE_LOST_WOODS", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_KAKARIKO_GATE_OPEN", 0}, 392},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_1_TORCH_CARPENTER", 0}, 397},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DOUBLE_CELL_CARPENTER", 0}, 402},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DEAD_END_CARPENTER", 0}, 402},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_SLOPE_CARPENTER", 0}, 402},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_RESCUED_ALL_CARPENTERS", 0}, 397},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_FREED_EPONA", 0}, 405},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_LINKS_POCKET", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TRIFORCE_COMPLETED", 0}, 410},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SARIA_SONG_HINT", 0}, 413},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SONG_FROM_IMPA", 0}, 416},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_MALON_EGG", 0}, 416},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_ZELDAS_LETTER", 0}, 416},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TOT_MASTER_SWORD", 0}, 420},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT", "RR_ROOT_EXITS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_CHILD_SPAWN", 0}, 212},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_ADULT_SPAWN", 0}, 141},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_MINUET_OF_FOREST_WARP", 0}, 425},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_BOLERO_OF_FIRE_WARP", 0}, 428},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_SERENADE_OF_WATER_WARP", 0}, 431},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_NOCTURNE_OF_SHADOW_WARP", 0}, 434},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_REQUIEM_OF_SPIRIT_WARP", 0}, 437},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_PRELUDE_OF_LIGHT_WARP", 0}, 440},
		{LogicRootKey{LogicRootKind::Exit, "RR_CHILD_SPAWN", "RR_KF_LINKS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_ADULT_SPAWN", "RR_TEMPLE_OF_TIME", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_MINUET_OF_FOREST_WARP", "RR_SACRED_FOREST_MEADOW", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_BOLERO_OF_FIRE_WARP", "RR_DMC_PAD_ENTRY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_SERENADE_OF_WATER_WARP", "RR_LAKE_HYLIA", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_REQUIEM_OF_SPIRIT_WARP", "RR_DESERT_COLOSSUS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_NOCTURNE_OF_SHADOW_WARP", "RR_GRAVEYARD_WARP_PAD_REGION", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_PRELUDE_OF_LIGHT_WARP", "RR_TEMPLE_OF_TIME", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_LEFT", 0}, 443},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_RIGHT", 0}, 443},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_COW", 0}, 458},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_1", 0}, 305},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_2", 0}, 305},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_3", 0}, 305},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY", 0}, 297},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY_BIG", 0}, 312},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY", 0}, 297},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY_BIG", 0}, 312},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY", 0}, 297},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY_BIG", 0}, 312},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY", 0}, 382},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY_BIG", 0}, 312},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_FISH", 0}, 462},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BRIDGE_RUPEE", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEHIND_MIDOS_RUPEE", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_WEST_RUPEE", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_EAST_RUPEE", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_WEST_RUPEE", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_EAST_RUPEE", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_WEST_HEART", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_EAST_HEART", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_NORTH_HEART", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_1", 0}, 465},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_2", 0}, 465},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_3", 0}, 465},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_4", 0}, 465},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_5", 0}, 465},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_6", 0}, 465},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RED_RUPEE", 0}, 465},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_1", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_2", 0}, 212},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_LEFT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_RIGHT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_LEFT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_RIGHT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_1", 0}, 467},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_2", 0}, 467},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_3", 0}, 467},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_4", 0}, 467},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_5", 0}, 467},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_6", 0}, 467},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RED_RUPEE", 0}, 467},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_1", 0}, 468},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_2", 0}, 468},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_3", 0}, 468},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_4", 0}, 468},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_5", 0}, 468},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_6", 0}, 468},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RED_RUPEE", 0}, 468},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_KNOW_IT_ALL_HOUSE", 0}, 469},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_BEAN_PATCH", 0}, 481},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_HOUSE_OF_TWINS", 0}, 491},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_1", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_2", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_3", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_4", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_5", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_6", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_7", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_8", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_9", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_10", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_11", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_12", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_1", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_2", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_3", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_4", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_5", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_6", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_7", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_8", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_9", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_10", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_11", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_12", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_13", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_14", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_15", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_16", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_17", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_18", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_19", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_20", 0}, 547},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_1", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_2", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_3", 0}, 546},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_1", 0}, 387},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_2", 0}, 387},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_3", 0}, 387},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_4", 0}, 387},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_POT", 0}, 259},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_1", 0}, 259},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_2", 0}, 259},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_1", 0}, 259},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_2", 0}, 259},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_1", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_2", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_3", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_4", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_5", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_6", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_7", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_8", 0}, 548},
	};
	return roots;
}
