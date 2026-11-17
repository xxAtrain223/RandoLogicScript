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

static value_t bombchus_enabled_adapter(std::span<EvalResult>) {
	return bombchus_enabled();
}

static value_t bottle_count_adapter(std::span<EvalResult>) {
	return CheckedHostInt(bottle_count());
}

static value_t can_plant_bean_adapter(std::span<EvalResult> arguments) {
	return can_plant_bean(static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[0].Result)), static_cast<RandomizerGet>(ToInt<int32_t>(arguments[1].Result)));
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
	case 24: return std::make_unique<MatchExpression<bool>>(21, std::vector<index_t>{25, 253, 258, 263, 271, 276, 284, 289, 298});
	case 25: return std::make_unique<MatchArm>(26, 27, true);
	case 26: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{7});
	case 27: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{28}, 30);
	case 28: return std::make_unique<ParameterExpression>(0, "item", 29);
	case 29: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MEGATON_HAMMER), "RG_MEGATON_HAMMER"}; }, "RG_MEGATON_HAMMER", false);
	case 30: return std::make_unique<AndExpression>(std::vector<index_t>{31, 34});
	case 31: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{32});
	case 32: return std::make_unique<ParameterExpression>(0, "item", 33);
	case 33: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("item"); }, "item", false);
	case 34: return std::make_unique<MatchExpression<bool>>(33, std::vector<index_t>{35, 38, 46, 55, 58, 60, 63, 66, 70, 73, 76, 79, 82, 86, 90, 102, 106, 109, 112, 118, 125, 128, 132, 156, 160, 163, 167, 185, 192, 196, 205, 209, 213, 218, 224, 227, 233, 239, 245, 251});
	case 35: return std::make_unique<MatchArm>(36, 2, false);
	case 36: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{37});
	case 37: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MAGIC_SINGLE), "RG_MAGIC_SINGLE"}; }, "RG_MAGIC_SINGLE", false);
	case 38: return std::make_unique<MatchArm>(39, 44, false);
	case 39: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{40, 41, 42, 43});
	case 40: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_DINS_FIRE), "RG_DINS_FIRE"}; }, "RG_DINS_FIRE", false);
	case 41: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FARORES_WIND), "RG_FARORES_WIND"}; }, "RG_FARORES_WIND", false);
	case 42: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_NAYRUS_LOVE), "RG_NAYRUS_LOVE"}; }, "RG_NAYRUS_LOVE", false);
	case 43: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_LENS_OF_TRUTH), "RG_LENS_OF_TRUTH"}; }, "RG_LENS_OF_TRUTH", false);
	case 44: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{45}, 30);
	case 45: return std::make_unique<ParameterExpression>(0, "item", 37);
	case 46: return std::make_unique<MatchArm>(47, 51, false);
	case 47: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{48, 49, 50});
	case 48: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FIRE_ARROWS), "RG_FIRE_ARROWS"}; }, "RG_FIRE_ARROWS", false);
	case 49: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ICE_ARROWS), "RG_ICE_ARROWS"}; }, "RG_ICE_ARROWS", false);
	case 50: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_LIGHT_ARROWS), "RG_LIGHT_ARROWS"}; }, "RG_LIGHT_ARROWS", false);
	case 51: return std::make_unique<AndExpression>(std::vector<index_t>{44, 52});
	case 52: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{53}, 30);
	case 53: return std::make_unique<ParameterExpression>(0, "item", 54);
	case 54: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FAIRY_BOW), "RG_FAIRY_BOW"}; }, "RG_FAIRY_BOW", false);
	case 55: return std::make_unique<MatchArm>(56, 57, false);
	case 56: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{54});
	case 57: return std::make_unique<ExternFunctionCallExpression>(is_adult_adapter, "is_adult", std::vector<index_t>{});
	case 58: return std::make_unique<MatchArm>(59, 57, false);
	case 59: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{29});
	case 60: return std::make_unique<MatchArm>(61, 57, false);
	case 61: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{62});
	case 62: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_IRON_BOOTS), "RG_IRON_BOOTS"}; }, "RG_IRON_BOOTS", false);
	case 63: return std::make_unique<MatchArm>(64, 57, false);
	case 64: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{65});
	case 65: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_HOVER_BOOTS), "RG_HOVER_BOOTS"}; }, "RG_HOVER_BOOTS", false);
	case 66: return std::make_unique<MatchArm>(67, 57, false);
	case 67: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{68, 69});
	case 68: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_HOOKSHOT), "RG_HOOKSHOT"}; }, "RG_HOOKSHOT", false);
	case 69: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_LONGSHOT), "RG_LONGSHOT"}; }, "RG_LONGSHOT", false);
	case 70: return std::make_unique<MatchArm>(71, 57, false);
	case 71: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{72});
	case 72: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GORON_TUNIC), "RG_GORON_TUNIC"}; }, "RG_GORON_TUNIC", false);
	case 73: return std::make_unique<MatchArm>(74, 57, false);
	case 74: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{75});
	case 75: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ZORA_TUNIC), "RG_ZORA_TUNIC"}; }, "RG_ZORA_TUNIC", false);
	case 76: return std::make_unique<MatchArm>(77, 57, false);
	case 77: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{78});
	case 78: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MIRROR_SHIELD), "RG_MIRROR_SHIELD"}; }, "RG_MIRROR_SHIELD", false);
	case 79: return std::make_unique<MatchArm>(80, 57, false);
	case 80: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{81});
	case 81: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MASTER_SWORD), "RG_MASTER_SWORD"}; }, "RG_MASTER_SWORD", false);
	case 82: return std::make_unique<MatchArm>(83, 57, false);
	case 83: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{84, 85});
	case 84: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GIANTS_KNIFE), "RG_GIANTS_KNIFE"}; }, "RG_GIANTS_KNIFE", false);
	case 85: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BIGGORON_SWORD), "RG_BIGGORON_SWORD"}; }, "RG_BIGGORON_SWORD", false);
	case 86: return std::make_unique<MatchArm>(87, 57, false);
	case 87: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{88, 89});
	case 88: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SILVER_GAUNTLETS), "RG_SILVER_GAUNTLETS"}; }, "RG_SILVER_GAUNTLETS", false);
	case 89: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GOLDEN_GAUNTLETS), "RG_GOLDEN_GAUNTLETS"}; }, "RG_GOLDEN_GAUNTLETS", false);
	case 90: return std::make_unique<MatchArm>(91, 57, false);
	case 91: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{92, 93, 94, 95, 96, 97, 98, 99, 100, 101});
	case 92: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_POCKET_EGG), "RG_POCKET_EGG"}; }, "RG_POCKET_EGG", false);
	case 93: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_COJIRO), "RG_COJIRO"}; }, "RG_COJIRO", false);
	case 94: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ODD_MUSHROOM), "RG_ODD_MUSHROOM"}; }, "RG_ODD_MUSHROOM", false);
	case 95: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ODD_POTION), "RG_ODD_POTION"}; }, "RG_ODD_POTION", false);
	case 96: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_POACHERS_SAW), "RG_POACHERS_SAW"}; }, "RG_POACHERS_SAW", false);
	case 97: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BROKEN_SWORD), "RG_BROKEN_SWORD"}; }, "RG_BROKEN_SWORD", false);
	case 98: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_PRESCRIPTION), "RG_PRESCRIPTION"}; }, "RG_PRESCRIPTION", false);
	case 99: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_EYEBALL_FROG), "RG_EYEBALL_FROG"}; }, "RG_EYEBALL_FROG", false);
	case 100: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_EYEDROPS), "RG_EYEDROPS"}; }, "RG_EYEDROPS", false);
	case 101: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_CLAIM_CHECK), "RG_CLAIM_CHECK"}; }, "RG_CLAIM_CHECK", false);
	case 102: return std::make_unique<MatchArm>(103, 105, false);
	case 103: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{104});
	case 104: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FAIRY_SLINGSHOT), "RG_FAIRY_SLINGSHOT"}; }, "RG_FAIRY_SLINGSHOT", false);
	case 105: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
	case 106: return std::make_unique<MatchArm>(107, 105, false);
	case 107: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{108});
	case 108: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOOMERANG), "RG_BOOMERANG"}; }, "RG_BOOMERANG", false);
	case 109: return std::make_unique<MatchArm>(110, 105, false);
	case 110: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{111});
	case 111: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_KOKIRI_SWORD), "RG_KOKIRI_SWORD"}; }, "RG_KOKIRI_SWORD", false);
	case 112: return std::make_unique<MatchArm>(113, 115, false);
	case 113: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{114});
	case 114: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_NUTS), "RG_NUTS"}; }, "RG_NUTS", false);
	case 115: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{116});
	case 116: return std::make_unique<ParameterExpression>(0, "key", 117);
	case 117: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_NUT_ACCESS), "LOGIC_NUT_ACCESS"}; }, "LOGIC_NUT_ACCESS", true);
	case 118: return std::make_unique<MatchArm>(119, 121, false);
	case 119: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{120});
	case 120: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_STICKS), "RG_STICKS"}; }, "RG_STICKS", false);
	case 121: return std::make_unique<AndExpression>(std::vector<index_t>{105, 122});
	case 122: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{123});
	case 123: return std::make_unique<ParameterExpression>(0, "key", 124);
	case 124: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_STICK_ACCESS), "LOGIC_STICK_ACCESS"}; }, "LOGIC_STICK_ACCESS", true);
	case 125: return std::make_unique<MatchArm>(126, 105, false);
	case 126: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{127});
	case 127: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_DEKU_SHIELD), "RG_DEKU_SHIELD"}; }, "RG_DEKU_SHIELD", false);
	case 128: return std::make_unique<MatchArm>(129, 2, false);
	case 129: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{130, 131});
	case 130: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_PROGRESSIVE_BOMB_BAG), "RG_PROGRESSIVE_BOMB_BAG"}; }, "RG_PROGRESSIVE_BOMB_BAG", false);
	case 131: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOMB_BAG), "RG_BOMB_BAG"}; }, "RG_BOMB_BAG", false);
	case 132: return std::make_unique<MatchArm>(133, 138, false);
	case 133: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{134, 135, 136, 137});
	case 134: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_PROGRESSIVE_BOMBCHU_BAG), "RG_PROGRESSIVE_BOMBCHU_BAG"}; }, "RG_PROGRESSIVE_BOMBCHU_BAG", false);
	case 135: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOMBCHU_5), "RG_BOMBCHU_5"}; }, "RG_BOMBCHU_5", false);
	case 136: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOMBCHU_10), "RG_BOMBCHU_10"}; }, "RG_BOMBCHU_10", false);
	case 137: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOMBCHU_20), "RG_BOMBCHU_20"}; }, "RG_BOMBCHU_20", false);
	case 138: return std::make_unique<AndExpression>(std::vector<index_t>{139, 155});
	case 139: return std::make_unique<FunctionCallExpression>("bombchu_refill", std::vector<index_t>{}, 140);
	case 140: return std::make_unique<OrExpression>(std::vector<index_t>{141, 144, 147, 150});
	case 141: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{142});
	case 142: return std::make_unique<ParameterExpression>(0, "key", 143);
	case 143: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_BUY_BOMBCHUS), "LOGIC_BUY_BOMBCHUS"}; }, "LOGIC_BUY_BOMBCHUS", true);
	case 144: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{145});
	case 145: return std::make_unique<ParameterExpression>(0, "key", 146);
	case 146: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_COULD_PLAY_BOWLING), "LOGIC_COULD_PLAY_BOWLING"}; }, "LOGIC_COULD_PLAY_BOWLING", true);
	case 147: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{148});
	case 148: return std::make_unique<ParameterExpression>(0, "key", 149);
	case 149: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_CARPET_MERCHANT), "LOGIC_CARPET_MERCHANT"}; }, "LOGIC_CARPET_MERCHANT", true);
	case 150: return std::make_unique<ComparisonExpression>("==", 151, 154);
	case 151: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{152});
	case 152: return std::make_unique<ParameterExpression>(0, "key", 153);
	case 153: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_ENABLE_BOMBCHU_DROPS), "RSK_ENABLE_BOMBCHU_DROPS"}; }, "RSK_ENABLE_BOMBCHU_DROPS", false);
	case 154: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_AMMO_DROPS_ON), "RO_AMMO_DROPS_ON"}; }, "RO_AMMO_DROPS_ON", false);
	case 155: return std::make_unique<ExternFunctionCallExpression>(bombchus_enabled_adapter, "bombchus_enabled", std::vector<index_t>{});
	case 156: return std::make_unique<MatchArm>(157, 105, false);
	case 157: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{158, 159});
	case 158: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_WEIRD_EGG), "RG_WEIRD_EGG"}; }, "RG_WEIRD_EGG", false);
	case 159: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_RUTOS_LETTER), "RG_RUTOS_LETTER"}; }, "RG_RUTOS_LETTER", false);
	case 160: return std::make_unique<MatchArm>(161, 105, false);
	case 161: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{162});
	case 162: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MAGIC_BEAN), "RG_MAGIC_BEAN"}; }, "RG_MAGIC_BEAN", false);
	case 163: return std::make_unique<MatchArm>(164, 105, false);
	case 164: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{165, 166});
	case 165: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SKULL_MASK), "RG_SKULL_MASK"}; }, "RG_SKULL_MASK", false);
	case 166: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MASK_OF_TRUTH), "RG_MASK_OF_TRUTH"}; }, "RG_MASK_OF_TRUTH", false);
	case 167: return std::make_unique<MatchArm>(168, 172, false);
	case 168: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{169, 170, 171});
	case 169: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ZELDAS_LULLABY), "RG_ZELDAS_LULLABY"}; }, "RG_ZELDAS_LULLABY", false);
	case 170: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_EPONAS_SONG), "RG_EPONAS_SONG"}; }, "RG_EPONAS_SONG", false);
	case 171: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_PRELUDE_OF_LIGHT), "RG_PRELUDE_OF_LIGHT"}; }, "RG_PRELUDE_OF_LIGHT", false);
	case 172: return std::make_unique<AndExpression>(std::vector<index_t>{173, 176, 179, 182});
	case 173: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{174});
	case 174: return std::make_unique<ParameterExpression>(0, "item", 175);
	case 175: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FAIRY_OCARINA), "RG_FAIRY_OCARINA"}; }, "RG_FAIRY_OCARINA", false);
	case 176: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{177});
	case 177: return std::make_unique<ParameterExpression>(0, "item", 178);
	case 178: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_OCARINA_C_LEFT_BUTTON), "RG_OCARINA_C_LEFT_BUTTON"}; }, "RG_OCARINA_C_LEFT_BUTTON", false);
	case 179: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{180});
	case 180: return std::make_unique<ParameterExpression>(0, "item", 181);
	case 181: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_OCARINA_C_RIGHT_BUTTON), "RG_OCARINA_C_RIGHT_BUTTON"}; }, "RG_OCARINA_C_RIGHT_BUTTON", false);
	case 182: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{183});
	case 183: return std::make_unique<ParameterExpression>(0, "item", 184);
	case 184: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_OCARINA_C_UP_BUTTON), "RG_OCARINA_C_UP_BUTTON"}; }, "RG_OCARINA_C_UP_BUTTON", false);
	case 185: return std::make_unique<MatchArm>(186, 188, false);
	case 186: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{187});
	case 187: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SARIAS_SONG), "RG_SARIAS_SONG"}; }, "RG_SARIAS_SONG", false);
	case 188: return std::make_unique<AndExpression>(std::vector<index_t>{173, 176, 179, 189});
	case 189: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{190});
	case 190: return std::make_unique<ParameterExpression>(0, "item", 191);
	case 191: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_OCARINA_C_DOWN_BUTTON), "RG_OCARINA_C_DOWN_BUTTON"}; }, "RG_OCARINA_C_DOWN_BUTTON", false);
	case 192: return std::make_unique<MatchArm>(193, 195, false);
	case 193: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{194});
	case 194: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SUNS_SONG), "RG_SUNS_SONG"}; }, "RG_SUNS_SONG", false);
	case 195: return std::make_unique<AndExpression>(std::vector<index_t>{173, 179, 182, 189});
	case 196: return std::make_unique<MatchArm>(197, 201, false);
	case 197: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{198, 199, 200});
	case 198: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SONG_OF_TIME), "RG_SONG_OF_TIME"}; }, "RG_SONG_OF_TIME", false);
	case 199: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOLERO_OF_FIRE), "RG_BOLERO_OF_FIRE"}; }, "RG_BOLERO_OF_FIRE", false);
	case 200: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_REQUIEM_OF_SPIRIT), "RG_REQUIEM_OF_SPIRIT"}; }, "RG_REQUIEM_OF_SPIRIT", false);
	case 201: return std::make_unique<AndExpression>(std::vector<index_t>{173, 202, 179, 189});
	case 202: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{203});
	case 203: return std::make_unique<ParameterExpression>(0, "item", 204);
	case 204: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_OCARINA_A_BUTTON), "RG_OCARINA_A_BUTTON"}; }, "RG_OCARINA_A_BUTTON", false);
	case 205: return std::make_unique<MatchArm>(206, 208, false);
	case 206: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{207});
	case 207: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SONG_OF_STORMS), "RG_SONG_OF_STORMS"}; }, "RG_SONG_OF_STORMS", false);
	case 208: return std::make_unique<AndExpression>(std::vector<index_t>{173, 202, 182, 189});
	case 209: return std::make_unique<MatchArm>(210, 212, false);
	case 210: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{211});
	case 211: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_MINUET_OF_FOREST), "RG_MINUET_OF_FOREST"}; }, "RG_MINUET_OF_FOREST", false);
	case 212: return std::make_unique<AndExpression>(std::vector<index_t>{173, 202, 176, 179, 182});
	case 213: return std::make_unique<MatchArm>(214, 217, false);
	case 214: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{215, 216});
	case 215: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SERENADE_OF_WATER), "RG_SERENADE_OF_WATER"}; }, "RG_SERENADE_OF_WATER", false);
	case 216: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_NOCTURNE_OF_SHADOW), "RG_NOCTURNE_OF_SHADOW"}; }, "RG_NOCTURNE_OF_SHADOW", false);
	case 217: return std::make_unique<AndExpression>(std::vector<index_t>{173, 202, 176, 179, 189});
	case 218: return std::make_unique<MatchArm>(219, 221, false);
	case 219: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{220});
	case 220: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_FISHING_POLE), "RG_FISHING_POLE"}; }, "RG_FISHING_POLE", false);
	case 221: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{222});
	case 222: return std::make_unique<ParameterExpression>(0, "item", 223);
	case 223: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_CHILD_WALLET), "RG_CHILD_WALLET"}; }, "RG_CHILD_WALLET", false);
	case 224: return std::make_unique<MatchArm>(225, 105, false);
	case 225: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{226});
	case 226: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_CRAWL), "RG_CRAWL"}; }, "RG_CRAWL", false);
	case 227: return std::make_unique<MatchArm>(228, 230, false);
	case 228: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{229});
	case 229: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOTTLE_WITH_BUGS), "RG_BOTTLE_WITH_BUGS"}; }, "RG_BOTTLE_WITH_BUGS", false);
	case 230: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{231});
	case 231: return std::make_unique<ParameterExpression>(0, "key", 232);
	case 232: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_BUG_ACCESS), "LOGIC_BUG_ACCESS"}; }, "LOGIC_BUG_ACCESS", true);
	case 233: return std::make_unique<MatchArm>(234, 236, false);
	case 234: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{235});
	case 235: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOTTLE_WITH_FISH), "RG_BOTTLE_WITH_FISH"}; }, "RG_BOTTLE_WITH_FISH", false);
	case 236: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{237});
	case 237: return std::make_unique<ParameterExpression>(0, "key", 238);
	case 238: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_FISH_ACCESS), "LOGIC_FISH_ACCESS"}; }, "LOGIC_FISH_ACCESS", true);
	case 239: return std::make_unique<MatchArm>(240, 242, false);
	case 240: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{241});
	case 241: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOTTLE_WITH_BLUE_FIRE), "RG_BOTTLE_WITH_BLUE_FIRE"}; }, "RG_BOTTLE_WITH_BLUE_FIRE", false);
	case 242: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{243});
	case 243: return std::make_unique<ParameterExpression>(0, "key", 244);
	case 244: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_BLUE_FIRE_ACCESS), "LOGIC_BLUE_FIRE_ACCESS"}; }, "LOGIC_BLUE_FIRE_ACCESS", true);
	case 245: return std::make_unique<MatchArm>(246, 248, false);
	case 246: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{247});
	case 247: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BOTTLE_WITH_FAIRY), "RG_BOTTLE_WITH_FAIRY"}; }, "RG_BOTTLE_WITH_FAIRY", false);
	case 248: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{249});
	case 249: return std::make_unique<ParameterExpression>(0, "key", 250);
	case 250: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_FAIRY_ACCESS), "LOGIC_FAIRY_ACCESS"}; }, "LOGIC_FAIRY_ACCESS", true);
	case 251: return std::make_unique<MatchArm>(252, 2, false);
	case 252: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{});
	case 253: return std::make_unique<MatchArm>(254, 256, true);
	case 254: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{255});
	case 255: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_SHORT_JUMPSLASH), "ED_SHORT_JUMPSLASH"}; }, "ED_SHORT_JUMPSLASH", false);
	case 256: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{257}, 30);
	case 257: return std::make_unique<ParameterExpression>(0, "item", 111);
	case 258: return std::make_unique<MatchArm>(259, 261, true);
	case 259: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{260});
	case 260: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_MASTER_SWORD_JUMPSLASH), "ED_MASTER_SWORD_JUMPSLASH"}; }, "ED_MASTER_SWORD_JUMPSLASH", false);
	case 261: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{262}, 30);
	case 262: return std::make_unique<ParameterExpression>(0, "item", 81);
	case 263: return std::make_unique<MatchArm>(264, 266, true);
	case 264: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{265});
	case 265: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_LONG_JUMPSLASH), "ED_LONG_JUMPSLASH"}; }, "ED_LONG_JUMPSLASH", false);
	case 266: return std::make_unique<OrExpression>(std::vector<index_t>{267, 269});
	case 267: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{268}, 30);
	case 268: return std::make_unique<ParameterExpression>(0, "item", 85);
	case 269: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{270}, 30);
	case 270: return std::make_unique<ParameterExpression>(0, "item", 120);
	case 271: return std::make_unique<MatchArm>(272, 274, true);
	case 272: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{273});
	case 273: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_BOMB_THROW), "ED_BOMB_THROW"}; }, "ED_BOMB_THROW", false);
	case 274: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{275}, 30);
	case 275: return std::make_unique<ParameterExpression>(0, "item", 131);
	case 276: return std::make_unique<MatchArm>(277, 279, true);
	case 277: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{278});
	case 278: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_BOOMERANG), "ED_BOOMERANG"}; }, "ED_BOOMERANG", false);
	case 279: return std::make_unique<OrExpression>(std::vector<index_t>{280, 282});
	case 280: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{281}, 30);
	case 281: return std::make_unique<ParameterExpression>(0, "item", 108);
	case 282: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{283}, 30);
	case 283: return std::make_unique<ParameterExpression>(0, "item", 40);
	case 284: return std::make_unique<MatchArm>(285, 287, true);
	case 285: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{286});
	case 286: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_HOOKSHOT), "ED_HOOKSHOT"}; }, "ED_HOOKSHOT", false);
	case 287: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{288}, 30);
	case 288: return std::make_unique<ParameterExpression>(0, "item", 68);
	case 289: return std::make_unique<MatchArm>(290, 292, true);
	case 290: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{291});
	case 291: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_LONGSHOT), "ED_LONGSHOT"}; }, "ED_LONGSHOT", false);
	case 292: return std::make_unique<OrExpression>(std::vector<index_t>{293, 295});
	case 293: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{294}, 30);
	case 294: return std::make_unique<ParameterExpression>(0, "item", 69);
	case 295: return std::make_unique<AndExpression>(std::vector<index_t>{23, 296});
	case 296: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{297}, 30);
	case 297: return std::make_unique<ParameterExpression>(0, "item", 135);
	case 298: return std::make_unique<MatchArm>(299, 301, false);
	case 299: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{300});
	case 300: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(EnemyDistance::ED_FAR), "ED_FAR"}; }, "ED_FAR", false);
	case 301: return std::make_unique<OrExpression>(std::vector<index_t>{302, 52});
	case 302: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{303}, 30);
	case 303: return std::make_unique<ParameterExpression>(0, "item", 104);
	case 304: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{305});
	case 305: return std::make_unique<ParameterExpression>(0, "key", 306);
	case 306: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_CLEAR), "LOGIC_DEKU_TREE_CLEAR"}; }, "LOGIC_DEKU_TREE_CLEAR", true);
	case 307: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{308, 6, 8, 9, 11, 13}, 14);
	case 308: return std::make_unique<ParameterExpression>(0, "e", 309);
	case 309: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_WITHERED_DEKU_BABA), "RE_WITHERED_DEKU_BABA"}; }, "RE_WITHERED_DEKU_BABA", false);
	case 310: return std::make_unique<FunctionCallExpression>("has_fire_source", std::vector<index_t>{}, 311);
	case 311: return std::make_unique<OrExpression>(std::vector<index_t>{282, 312});
	case 312: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{313}, 30);
	case 313: return std::make_unique<ParameterExpression>(0, "item", 48);
	case 314: return std::make_unique<OrExpression>(std::vector<index_t>{315, 287});
	case 315: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{316});
	case 316: return std::make_unique<ParameterExpression>(0, "item", 317);
	case 317: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_CLIMB), "RG_CLIMB"}; }, "RG_CLIMB", false);
	case 318: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{319});
	case 319: return std::make_unique<ParameterExpression>(0, "key", 320);
	case 320: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_1F_BROKE_WEB), "LOGIC_DEKU_TREE_1F_BROKE_WEB"}; }, "LOGIC_DEKU_TREE_1F_BROKE_WEB", true);
	case 321: return std::make_unique<OrExpression>(std::vector<index_t>{315, 293});
	case 322: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{323});
	case 323: return std::make_unique<ParameterExpression>(0, "key", 324);
	case 324: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB), "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB"}; }, "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", true);
	case 325: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_sticks", std::vector<index_t>{}, 326);
	case 326: return std::make_unique<OrExpression>(std::vector<index_t>{327, 280});
	case 327: return std::make_unique<FunctionCallExpression>("can_use_sword", std::vector<index_t>{}, 328);
	case 328: return std::make_unique<OrExpression>(std::vector<index_t>{256, 261, 267});
	case 329: return std::make_unique<FunctionCallExpression>("can_get_deku_baba_nuts", std::vector<index_t>{}, 330);
	case 330: return std::make_unique<OrExpression>(std::vector<index_t>{331, 302, 52, 335, 282});
	case 331: return std::make_unique<FunctionCallExpression>("can_jumpslash", std::vector<index_t>{}, 332);
	case 332: return std::make_unique<OrExpression>(std::vector<index_t>{333, 27});
	case 333: return std::make_unique<FunctionCallExpression>("can_jumpslash_except_hammer", std::vector<index_t>{}, 334);
	case 334: return std::make_unique<OrExpression>(std::vector<index_t>{269, 327});
	case 335: return std::make_unique<FunctionCallExpression>("has_explosives", std::vector<index_t>{}, 336);
	case 336: return std::make_unique<OrExpression>(std::vector<index_t>{274, 296});
	case 337: return std::make_unique<OrExpression>(std::vector<index_t>{269, 52});
	case 338: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{339, 6, 8, 9, 11, 13}, 14);
	case 339: return std::make_unique<ParameterExpression>(0, "e", 340);
	case 340: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_DEKU_BABA), "RE_DEKU_BABA"}; }, "RE_DEKU_BABA", false);
	case 341: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{342});
	case 342: return std::make_unique<ParameterExpression>(0, "condition", 343);
	case 343: return std::make_unique<ConditionExpression>(344);
	case 344: return std::make_unique<FunctionCallExpression>("can_hit_eye_targets", std::vector<index_t>{}, 345);
	case 345: return std::make_unique<OrExpression>(std::vector<index_t>{52, 302});
	case 346: return std::make_unique<AndExpression>(std::vector<index_t>{347, 351});
	case 347: return std::make_unique<OrExpression>(std::vector<index_t>{315, 287, 348});
	case 348: return std::make_unique<AndExpression>(std::vector<index_t>{57, 349});
	case 349: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{350}, 30);
	case 350: return std::make_unique<ParameterExpression>(0, "item", 198);
	case 351: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{352});
	case 352: return std::make_unique<ParameterExpression>(0, "condition", 353);
	case 353: return std::make_unique<ConditionExpression>(354);
	case 354: return std::make_unique<OrExpression>(std::vector<index_t>{296, 355, 359});
	case 355: return std::make_unique<AndExpression>(std::vector<index_t>{274, 356});
	case 356: return std::make_unique<OrExpression>(std::vector<index_t>{349, 57, 357});
	case 357: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{358}, 30);
	case 358: return std::make_unique<ParameterExpression>(0, "item", 65);
	case 359: return std::make_unique<AndExpression>(std::vector<index_t>{27, 360});
	case 360: return std::make_unique<OrExpression>(std::vector<index_t>{348, 361});
	case 361: return std::make_unique<AndExpression>(std::vector<index_t>{362, 315});
	case 362: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{363});
	case 363: return std::make_unique<ParameterExpression>(0, "key", 364);
	case 364: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_COMPASS_GS), "RT_DEKU_MQ_COMPASS_GS"}; }, "RT_DEKU_MQ_COMPASS_GS", false);
	case 365: return std::make_unique<FunctionCallExpression>("blast_or_smash", std::vector<index_t>{}, 366);
	case 366: return std::make_unique<OrExpression>(std::vector<index_t>{335, 27});
	case 367: return std::make_unique<AndExpression>(std::vector<index_t>{269, 368});
	case 368: return std::make_unique<OrExpression>(std::vector<index_t>{369, 57, 357});
	case 369: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{370});
	case 370: return std::make_unique<ParameterExpression>(0, "key", 371);
	case 371: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK), "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK"}; }, "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", true);
	case 372: return std::make_unique<AndExpression>(std::vector<index_t>{341, 373, 376});
	case 373: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{374});
	case 374: return std::make_unique<ParameterExpression>(0, "key", 375);
	case 375: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM), "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM"}; }, "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", true);
	case 376: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{377});
	case 377: return std::make_unique<ParameterExpression>(0, "condition", 378);
	case 378: return std::make_unique<ConditionExpression>(269);
	case 379: return std::make_unique<OrExpression>(std::vector<index_t>{57, 380, 383, 369, 357});
	case 380: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{381});
	case 381: return std::make_unique<ParameterExpression>(0, "key", 382);
	case 382: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_B1_SKIP), "RT_DEKU_B1_SKIP"}; }, "RT_DEKU_B1_SKIP", false);
	case 383: return std::make_unique<FunctionCallExpression>("can_ground_jump", std::vector<index_t>{384}, 385);
	case 384: return std::make_unique<ParameterExpression>(0, "hasBombflower", 12);
	case 385: return std::make_unique<AndExpression>(std::vector<index_t>{386, 389, 399});
	case 386: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{387});
	case 387: return std::make_unique<ParameterExpression>(0, "key", 388);
	case 388: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_GROUND_JUMP), "RT_GROUND_JUMP"}; }, "RT_GROUND_JUMP", false);
	case 389: return std::make_unique<FunctionCallExpression>("can_standing_shield", std::vector<index_t>{}, 390);
	case 390: return std::make_unique<OrExpression>(std::vector<index_t>{391, 393, 397});
	case 391: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{392}, 30);
	case 392: return std::make_unique<ParameterExpression>(0, "item", 78);
	case 393: return std::make_unique<AndExpression>(std::vector<index_t>{57, 394});
	case 394: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{395});
	case 395: return std::make_unique<ParameterExpression>(0, "item", 396);
	case 396: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_HYLIAN_SHIELD), "RG_HYLIAN_SHIELD"}; }, "RG_HYLIAN_SHIELD", false);
	case 397: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{398}, 30);
	case 398: return std::make_unique<ParameterExpression>(0, "item", 127);
	case 399: return std::make_unique<OrExpression>(std::vector<index_t>{274, 400});
	case 400: return std::make_unique<AndExpression>(std::vector<index_t>{401, 402});
	case 401: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("hasBombflower"); }, "hasBombflower", false);
	case 402: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{403});
	case 403: return std::make_unique<ParameterExpression>(0, "item", 404);
	case 404: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GORONS_BRACELET), "RG_GORONS_BRACELET"}; }, "RG_GORONS_BRACELET", false);
	case 405: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{406, 6, 8, 9, 11, 13}, 14);
	case 406: return std::make_unique<ParameterExpression>(0, "e", 407);
	case 407: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_MAD_SCRUB), "RE_MAD_SCRUB"}; }, "RE_MAD_SCRUB", false);
	case 408: return std::make_unique<OrExpression>(std::vector<index_t>{312, 409});
	case 409: return std::make_unique<AndExpression>(std::vector<index_t>{269, 410});
	case 410: return std::make_unique<OrExpression>(std::vector<index_t>{411, 414});
	case 411: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{412});
	case 412: return std::make_unique<ParameterExpression>(0, "key", 413);
	case 413: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_MQ_LOG), "RT_DEKU_MQ_LOG"}; }, "RT_DEKU_MQ_LOG", false);
	case 414: return std::make_unique<AndExpression>(std::vector<index_t>{105, 415});
	case 415: return std::make_unique<FunctionCallExpression>("can_shield", std::vector<index_t>{}, 416);
	case 416: return std::make_unique<OrExpression>(std::vector<index_t>{391, 394, 397});
	case 417: return std::make_unique<OrExpression>(std::vector<index_t>{411, 414, 293, 418});
	case 418: return std::make_unique<AndExpression>(std::vector<index_t>{287, 419});
	case 419: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{420}, 30);
	case 420: return std::make_unique<ParameterExpression>(0, "item", 62);
	case 421: return std::make_unique<AndExpression>(std::vector<index_t>{422, 425});
	case 422: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{423});
	case 423: return std::make_unique<ParameterExpression>(0, "key", 424);
	case 424: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES), "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES"}; }, "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", true);
	case 425: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{426, 428, 8}, 430);
	case 426: return std::make_unique<ParameterExpression>(0, "e", 427);
	case 427: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_BIG_SKULLTULA), "RE_BIG_SKULLTULA"}; }, "RE_BIG_SKULLTULA", false);
	case 428: return std::make_unique<ParameterExpression>(1, "distance", 429);
	case 429: return std::make_unique<TernaryExpression>(349, 7, 255);
	case 430: return std::make_unique<OrExpression>(std::vector<index_t>{431, 435});
	case 431: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{432, 433, 434, 9, 11, 13}, 14);
	case 432: return std::make_unique<ParameterExpression>(0, "e", 15);
	case 433: return std::make_unique<ParameterExpression>(1, "distance", 21);
	case 434: return std::make_unique<ParameterExpression>(2, "wall_or_floor", 23);
	case 435: return std::make_unique<MatchExpression<bool>>(15, std::vector<index_t>{436});
	case 436: return std::make_unique<MatchArm>(17, 2, false);
	case 437: return std::make_unique<OrExpression>(std::vector<index_t>{411, 414, 293, 438, 441});
	case 438: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{439});
	case 439: return std::make_unique<ParameterExpression>(0, "item", 440);
	case 440: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_BRONZE_SCALE), "RG_BRONZE_SCALE"}; }, "RG_BRONZE_SCALE", false);
	case 441: return std::make_unique<AndExpression>(std::vector<index_t>{419, 442});
	case 442: return std::make_unique<OrExpression>(std::vector<index_t>{57, 287});
	case 443: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{444});
	case 444: return std::make_unique<ParameterExpression>(0, "condition", 445);
	case 445: return std::make_unique<ConditionExpression>(446);
	case 446: return std::make_unique<AndExpression>(std::vector<index_t>{405, 447});
	case 447: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{448, 6, 8, 9, 11, 13}, 14);
	case 448: return std::make_unique<ParameterExpression>(0, "e", 449);
	case 449: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_KEESE), "RE_KEESE"}; }, "RE_KEESE", false);
	case 450: return std::make_unique<AndExpression>(std::vector<index_t>{451, 453});
	case 451: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{452}, 30);
	case 452: return std::make_unique<ParameterExpression>(0, "item", 226);
	case 453: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{454});
	case 454: return std::make_unique<ParameterExpression>(0, "condition", 455);
	case 455: return std::make_unique<ConditionExpression>(456);
	case 456: return std::make_unique<OrExpression>(std::vector<index_t>{457, 52});
	case 457: return std::make_unique<FunctionCallExpression>("has_fire_source_with_torch", std::vector<index_t>{}, 458);
	case 458: return std::make_unique<OrExpression>(std::vector<index_t>{310, 269});
	case 459: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{460});
	case 460: return std::make_unique<ParameterExpression>(0, "item", 461);
	case 461: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_POWER_BRACELET), "RG_POWER_BRACELET"}; }, "RG_POWER_BRACELET", false);
	case 462: return std::make_unique<AndExpression>(std::vector<index_t>{463, 466});
	case 463: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{464});
	case 464: return std::make_unique<ParameterExpression>(0, "key", 465);
	case 465: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_DEKU_TREE_B1_BROKE_WEB), "LOGIC_DEKU_TREE_B1_BROKE_WEB"}; }, "LOGIC_DEKU_TREE_B1_BROKE_WEB", true);
	case 466: return std::make_unique<OrExpression>(std::vector<index_t>{438, 419});
	case 467: return std::make_unique<AndExpression>(std::vector<index_t>{315, 468});
	case 468: return std::make_unique<OrExpression>(std::vector<index_t>{438, 287});
	case 469: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{470});
	case 470: return std::make_unique<ParameterExpression>(0, "condition", 471);
	case 471: return std::make_unique<ConditionExpression>(472);
	case 472: return std::make_unique<FunctionCallExpression>("can_reflect_nuts", std::vector<index_t>{}, 473);
	case 473: return std::make_unique<OrExpression>(std::vector<index_t>{397, 393});
	case 474: return std::make_unique<FunctionCallExpression>("can_pass", std::vector<index_t>{426, 6, 8}, 430);
	case 475: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{476});
	case 476: return std::make_unique<ParameterExpression>(0, "condition", 477);
	case 477: return std::make_unique<ConditionExpression>(478);
	case 478: return std::make_unique<OrExpression>(std::vector<index_t>{472, 27});
	case 479: return std::make_unique<OrExpression>(std::vector<index_t>{367, 480});
	case 480: return std::make_unique<AndExpression>(std::vector<index_t>{481, 57, 52});
	case 481: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{482});
	case 482: return std::make_unique<ParameterExpression>(0, "key", 483);
	case 483: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BOW_WEBS), "RT_DEKU_B1_BOW_WEBS"}; }, "RT_DEKU_B1_BOW_WEBS", false);
	case 484: return std::make_unique<OrExpression>(std::vector<index_t>{315, 485});
	case 485: return std::make_unique<AndExpression>(std::vector<index_t>{57, 293});
	case 486: return std::make_unique<OrExpression>(std::vector<index_t>{57, 380, 383, 369});
	case 487: return std::make_unique<OrExpression>(std::vector<index_t>{438, 488});
	case 488: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{489});
	case 489: return std::make_unique<ParameterExpression>(0, "key", 490);
	case 490: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG), "RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG"}; }, "RT_DEKU_B1_BACKFLIP_OVER_SPIKED_LOG", false);
	case 491: return std::make_unique<AndExpression>(std::vector<index_t>{453, 492});
	case 492: return std::make_unique<ExternFunctionCallExpression>(any_age_adapter, "any_age", std::vector<index_t>{493});
	case 493: return std::make_unique<ParameterExpression>(0, "condition", 494);
	case 494: return std::make_unique<ConditionExpression>(365);
	case 495: return std::make_unique<AndExpression>(std::vector<index_t>{453, 451});
	case 496: return std::make_unique<OrExpression>(std::vector<index_t>{497, 503});
	case 497: return std::make_unique<FunctionCallExpression>("call_gossip_fairy_except_suns", std::vector<index_t>{}, 498);
	case 498: return std::make_unique<OrExpression>(std::vector<index_t>{499, 501, 349});
	case 499: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{500}, 30);
	case 500: return std::make_unique<ParameterExpression>(0, "item", 169);
	case 501: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{502}, 30);
	case 502: return std::make_unique<ParameterExpression>(0, "item", 170);
	case 503: return std::make_unique<AndExpression>(std::vector<index_t>{105, 504, 506, 509});
	case 504: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{505}, 30);
	case 505: return std::make_unique<ParameterExpression>(0, "item", 162);
	case 506: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{507});
	case 507: return std::make_unique<ParameterExpression>(0, "item", 508);
	case 508: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_KOKIRI_FOREST_BEAN_SOUL), "RG_KOKIRI_FOREST_BEAN_SOUL"}; }, "RG_KOKIRI_FOREST_BEAN_SOUL", false);
	case 509: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{510}, 30);
	case 510: return std::make_unique<ParameterExpression>(0, "item", 207);
	case 511: return std::make_unique<AndExpression>(std::vector<index_t>{105, 512, 256, 397});
	case 512: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{513});
	case 513: return std::make_unique<ParameterExpression>(0, "item", 514);
	case 514: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_SPEAK_KOKIRI), "RG_SPEAK_KOKIRI"}; }, "RG_SPEAK_KOKIRI", false);
	case 515: return std::make_unique<TernaryExpression>(105, 516, 522);
	case 516: return std::make_unique<FunctionCallExpression>("can_climb_ladder", std::vector<index_t>{}, 517);
	case 517: return std::make_unique<OrExpression>(std::vector<index_t>{315, 518});
	case 518: return std::make_unique<AndExpression>(std::vector<index_t>{519, 287});
	case 519: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{520});
	case 520: return std::make_unique<ParameterExpression>(0, "key", 521);
	case 521: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_HOOKSHOT_LADDERS), "RT_HOOKSHOT_LADDERS"}; }, "RT_HOOKSHOT_LADDERS", false);
	case 522: return std::make_unique<OrExpression>(std::vector<index_t>{315, 357});
	case 523: return std::make_unique<OrExpression>(std::vector<index_t>{524, 527, 532});
	case 524: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{525});
	case 525: return std::make_unique<ParameterExpression>(0, "key", 526);
	case 526: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD), "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD"}; }, "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", true);
	case 527: return std::make_unique<ComparisonExpression>("==", 528, 531);
	case 528: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{529});
	case 529: return std::make_unique<ParameterExpression>(0, "key", 530);
	case 530: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_FOREST), "RSK_FOREST"}; }, "RSK_FOREST", false);
	case 531: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_CLOSED_FOREST_OFF), "RO_CLOSED_FOREST_OFF"}; }, "RO_CLOSED_FOREST_OFF", false);
	case 532: return std::make_unique<AndExpression>(std::vector<index_t>{57, 533});
	case 533: return std::make_unique<OrExpression>(std::vector<index_t>{474, 534});
	case 534: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{535});
	case 535: return std::make_unique<ParameterExpression>(0, "key", 536);
	case 536: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_FOREST_TEMPLE_CLEAR), "LOGIC_FOREST_TEMPLE_CLEAR"}; }, "LOGIC_FOREST_TEMPLE_CLEAR", true);
	case 537: return std::make_unique<OrExpression>(std::vector<index_t>{315, 287, 538});
	case 538: return std::make_unique<AndExpression>(std::vector<index_t>{57, 539});
	case 539: return std::make_unique<OrExpression>(std::vector<index_t>{540, 544});
	case 540: return std::make_unique<ExternFunctionCallExpression>(can_plant_bean_adapter, "can_plant_bean", std::vector<index_t>{541, 543});
	case 541: return std::make_unique<ParameterExpression>(0, "reg", 542);
	case 542: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerRegion::RR_KOKIRI_FOREST), "RR_KOKIRI_FOREST"}; }, "RR_KOKIRI_FOREST", false);
	case 543: return std::make_unique<ParameterExpression>(1, "bean", 508);
	case 544: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{545});
	case 545: return std::make_unique<ParameterExpression>(0, "key", 546);
	case 546: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_UNINTUITIVE_JUMPS), "RT_UNINTUITIVE_JUMPS"}; }, "RT_UNINTUITIVE_JUMPS", false);
	case 547: return std::make_unique<AndExpression>(std::vector<index_t>{57, 540});
	case 548: return std::make_unique<OrExpression>(std::vector<index_t>{57, 549, 304});
	case 549: return std::make_unique<ComparisonExpression>("!=", 528, 550);
	case 550: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_CLOSED_FOREST_ON), "RO_CLOSED_FOREST_ON"}; }, "RO_CLOSED_FOREST_ON", false);
	case 551: return std::make_unique<AndExpression>(std::vector<index_t>{105, 552});
	case 552: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{553});
	case 553: return std::make_unique<ParameterExpression>(0, "item", 554);
	case 554: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_OPEN_CHEST), "RG_OPEN_CHEST"}; }, "RG_OPEN_CHEST", false);
	case 555: return std::make_unique<OrExpression>(std::vector<index_t>{105, 556});
	case 556: return std::make_unique<AndExpression>(std::vector<index_t>{557, 562});
	case 557: return std::make_unique<ComparisonExpression>("!=", 558, 561);
	case 558: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{559});
	case 559: return std::make_unique<ParameterExpression>(0, "key", 560);
	case 560: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SHUFFLE_DUNGEON_ENTRANCES), "RSK_SHUFFLE_DUNGEON_ENTRANCES"}; }, "RSK_SHUFFLE_DUNGEON_ENTRANCES", false);
	case 561: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF), "RO_DUNGEON_ENTRANCE_SHUFFLE_OFF"}; }, "RO_DUNGEON_ENTRANCE_SHUFFLE_OFF", false);
	case 562: return std::make_unique<OrExpression>(std::vector<index_t>{527, 524});
	case 563: return std::make_unique<OrExpression>(std::vector<index_t>{564, 527, 524});
	case 564: return std::make_unique<AndExpression>(std::vector<index_t>{57, 565});
	case 565: return std::make_unique<OrExpression>(std::vector<index_t>{474, 304});
	case 566: return std::make_unique<AndExpression>(std::vector<index_t>{57, 567});
	case 567: return std::make_unique<OrExpression>(std::vector<index_t>{540, 357});
	case 568: return std::make_unique<FunctionCallExpression>("can_open_storms_grotto", std::vector<index_t>{}, 569);
	case 569: return std::make_unique<AndExpression>(std::vector<index_t>{509, 570});
	case 570: return std::make_unique<OrExpression>(std::vector<index_t>{571, 574});
	case 571: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{572});
	case 572: return std::make_unique<ParameterExpression>(0, "item", 573);
	case 573: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_STONE_OF_AGONY), "RG_STONE_OF_AGONY"}; }, "RG_STONE_OF_AGONY", false);
	case 574: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{575});
	case 575: return std::make_unique<ParameterExpression>(0, "key", 576);
	case 576: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_GROTTOS_WITHOUT_AGONY), "RT_GROTTOS_WITHOUT_AGONY"}; }, "RT_GROTTOS_WITHOUT_AGONY", false);
	case 577: return std::make_unique<OrExpression>(std::vector<index_t>{578, 269});
	case 578: return std::make_unique<FunctionCallExpression>("call_gossip_fairy", std::vector<index_t>{}, 579);
	case 579: return std::make_unique<OrExpression>(std::vector<index_t>{497, 580});
	case 580: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{581}, 30);
	case 581: return std::make_unique<ParameterExpression>(0, "item", 194);
	case 582: return std::make_unique<FunctionCallExpression>("can_cut_shrubs", std::vector<index_t>{}, 583);
	case 583: return std::make_unique<OrExpression>(std::vector<index_t>{256, 280, 335, 402, 261, 27, 267, 584});
	case 584: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{585}, 30);
	case 585: return std::make_unique<ParameterExpression>(0, "item", 84);
	case 586: return std::make_unique<ComparisonExpression>("==", 587, 590);
	case 587: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{588});
	case 588: return std::make_unique<ParameterExpression>(0, "key", 589);
	case 589: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_KAK_GATE), "RSK_KAK_GATE"}; }, "RSK_KAK_GATE", false);
	case 590: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_KAK_GATE_OPEN), "RO_KAK_GATE_OPEN"}; }, "RO_KAK_GATE_OPEN", false);
	case 591: return std::make_unique<ComparisonExpression>("==", 592, 595);
	case 592: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{593});
	case 593: return std::make_unique<ParameterExpression>(0, "key", 594);
	case 594: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_GERUDO_FORTRESS), "RSK_GERUDO_FORTRESS"}; }, "RSK_GERUDO_FORTRESS", false);
	case 595: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_GF_CARPENTERS_FREE), "RO_GF_CARPENTERS_FREE"}; }, "RO_GF_CARPENTERS_FREE", false);
	case 596: return std::make_unique<OrExpression>(std::vector<index_t>{591, 597});
	case 597: return std::make_unique<ComparisonExpression>("==", 592, 598);
	case 598: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_GF_CARPENTERS_FAST), "RO_GF_CARPENTERS_FAST"}; }, "RO_GF_CARPENTERS_FAST", false);
	case 599: return std::make_unique<ComparisonExpression>("==", 600, 603);
	case 600: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{601});
	case 601: return std::make_unique<ParameterExpression>(0, "key", 602);
	case 602: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SKIP_EPONA_RACE), "RSK_SKIP_EPONA_RACE"}; }, "RSK_SKIP_EPONA_RACE", false);
	case 603: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_GENERIC_YES), "RO_GENERIC_YES"}; }, "RO_GENERIC_YES", false);
	case 604: return std::make_unique<ComparisonExpression>(">=", 605, 606);
	case 605: return std::make_unique<ExternFunctionCallExpression>(collected_triforce_pieces_adapter, "collected_triforce_pieces", std::vector<index_t>{});
	case 606: return std::make_unique<ExternFunctionCallExpression>(required_triforce_pieces_adapter, "required_triforce_pieces", std::vector<index_t>{});
	case 607: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{608}, 30);
	case 608: return std::make_unique<ParameterExpression>(0, "item", 187);
	case 609: return std::make_unique<ComparisonExpression>("==", 610, 603);
	case 610: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{611});
	case 611: return std::make_unique<ParameterExpression>(0, "key", 612);
	case 612: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SKIP_CHILD_ZELDA), "RSK_SKIP_CHILD_ZELDA"}; }, "RSK_SKIP_CHILD_ZELDA", false);
	case 613: return std::make_unique<ComparisonExpression>("==", 614, 617);
	case 614: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{615});
	case 615: return std::make_unique<ParameterExpression>(0, "key", 616);
	case 616: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SELECTED_STARTING_AGE), "RSK_SELECTED_STARTING_AGE"}; }, "RSK_SELECTED_STARTING_AGE", false);
	case 617: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RO_AGE_ADULT), "RO_AGE_ADULT"}; }, "RO_AGE_ADULT", false);
	case 618: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{619}, 30);
	case 619: return std::make_unique<ParameterExpression>(0, "item", 211);
	case 620: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{621}, 30);
	case 621: return std::make_unique<ParameterExpression>(0, "item", 199);
	case 622: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{623}, 30);
	case 623: return std::make_unique<ParameterExpression>(0, "item", 215);
	case 624: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{625}, 30);
	case 625: return std::make_unique<ParameterExpression>(0, "item", 216);
	case 626: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{627}, 30);
	case 627: return std::make_unique<ParameterExpression>(0, "item", 200);
	case 628: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{629}, 30);
	case 629: return std::make_unique<ParameterExpression>(0, "item", 171);
	case 630: return std::make_unique<FunctionCallExpression>("can_break_lower_beehives", std::vector<index_t>{}, 631);
	case 631: return std::make_unique<OrExpression>(std::vector<index_t>{632, 274});
	case 632: return std::make_unique<FunctionCallExpression>("can_break_upper_beehives", std::vector<index_t>{}, 633);
	case 633: return std::make_unique<OrExpression>(std::vector<index_t>{634, 636, 640});
	case 634: return std::make_unique<FunctionCallExpression>("hookshot_or_boomerang", std::vector<index_t>{}, 635);
	case 635: return std::make_unique<OrExpression>(std::vector<index_t>{287, 280});
	case 636: return std::make_unique<AndExpression>(std::vector<index_t>{637, 296});
	case 637: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{638});
	case 638: return std::make_unique<ParameterExpression>(0, "key", 639);
	case 639: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_BOMBCHU_BEEHIVES), "RT_BOMBCHU_BEEHIVES"}; }, "RT_BOMBCHU_BEEHIVES", false);
	case 640: return std::make_unique<AndExpression>(std::vector<index_t>{641, 345});
	case 641: return std::make_unique<ConversionExpression<bool>>(642);
	case 642: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{643});
	case 643: return std::make_unique<ParameterExpression>(0, "key", 644);
	case 644: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SLINGBOW_BREAK_BEEHIVES), "RSK_SLINGBOW_BREAK_BEEHIVES"}; }, "RSK_SLINGBOW_BREAK_BEEHIVES", false);
	case 645: return std::make_unique<AndExpression>(std::vector<index_t>{57, 501, 646});
	case 646: return std::make_unique<ExternFunctionCallExpression>(flag_adapter, "flag", std::vector<index_t>{647});
	case 647: return std::make_unique<ParameterExpression>(0, "key", 648);
	case 648: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(LogicVal::LOGIC_LINKS_COW), "LOGIC_LINKS_COW"}; }, "LOGIC_LINKS_COW", true);
	case 649: return std::make_unique<FunctionCallExpression>("has_bottle", std::vector<index_t>{}, 650);
	case 650: return std::make_unique<ComparisonExpression>(">=", 651, 10);
	case 651: return std::make_unique<ExternFunctionCallExpression>(bottle_count_adapter, "bottle_count", std::vector<index_t>{});
	case 652: return std::make_unique<AndExpression>(std::vector<index_t>{57, 653});
	case 653: return std::make_unique<OrExpression>(std::vector<index_t>{540, 357, 280});
	case 654: return std::make_unique<AndExpression>(std::vector<index_t>{57, 280});
	case 655: return std::make_unique<AndExpression>(std::vector<index_t>{57, 357});
	case 656: return std::make_unique<AndExpression>(std::vector<index_t>{105, 657, 659});
	case 657: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{658, 6, 8, 9, 11, 13}, 14);
	case 658: return std::make_unique<ParameterExpression>(0, "e", 18);
	case 659: return std::make_unique<FunctionCallExpression>("can_get_night_time_gs", std::vector<index_t>{}, 660);
	case 660: return std::make_unique<AndExpression>(std::vector<index_t>{661, 662});
	case 661: return std::make_unique<ExternFunctionCallExpression>(at_night_adapter, "at_night", std::vector<index_t>{});
	case 662: return std::make_unique<OrExpression>(std::vector<index_t>{580, 663});
	case 663: return std::make_unique<NotExpression>(664);
	case 664: return std::make_unique<ConversionExpression<bool>>(665);
	case 665: return std::make_unique<ExternFunctionCallExpression>(setting_adapter, "setting", std::vector<index_t>{666});
	case 666: return std::make_unique<ParameterExpression>(0, "key", 667);
	case 667: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RSK_SKULLS_SUNS_SONG), "RSK_SKULLS_SUNS_SONG"}; }, "RSK_SKULLS_SUNS_SONG", false);
	case 668: return std::make_unique<AndExpression>(std::vector<index_t>{669, 657});
	case 669: return std::make_unique<FunctionCallExpression>("can_spawn_soil_skull", std::vector<index_t>{670}, 671);
	case 670: return std::make_unique<ParameterExpression>(0, "bean", 508);
	case 671: return std::make_unique<AndExpression>(std::vector<index_t>{105, 672, 674});
	case 672: return std::make_unique<FunctionCallExpression>("can_use", std::vector<index_t>{673}, 30);
	case 673: return std::make_unique<ParameterExpression>(0, "item", 229);
	case 674: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{675});
	case 675: return std::make_unique<ParameterExpression>(0, "item", 676);
	case 676: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("bean"); }, "bean", false);
	case 677: return std::make_unique<AndExpression>(std::vector<index_t>{57, 659, 678});
	case 678: return std::make_unique<OrExpression>(std::vector<index_t>{679, 725});
	case 679: return std::make_unique<FunctionCallExpression>("can_get_drop", std::vector<index_t>{658, 680, 681}, 682);
	case 680: return std::make_unique<ParameterExpression>(1, "distance", 278);
	case 681: return std::make_unique<ParameterExpression>(2, "above_link", 12);
	case 682: return std::make_unique<AndExpression>(std::vector<index_t>{683, 684});
	case 683: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{432, 433, 8, 9, 11, 13}, 14);
	case 684: return std::make_unique<OrExpression>(std::vector<index_t>{685, 707});
	case 685: return std::make_unique<ComparisonExpression>("<=", 686, 705);
	case 686: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{20}, 687);
	case 687: return std::make_unique<MatchExpression<int32_t>>(21, std::vector<index_t>{688, 690, 691, 693, 695, 697, 699, 701, 703});
	case 688: return std::make_unique<MatchArm>(26, 689, false);
	case 689: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{0}; }, "0", false);
	case 690: return std::make_unique<MatchArm>(254, 10, false);
	case 691: return std::make_unique<MatchArm>(259, 692, false);
	case 692: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{2}; }, "2", false);
	case 693: return std::make_unique<MatchArm>(264, 694, false);
	case 694: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{3}; }, "3", false);
	case 695: return std::make_unique<MatchArm>(272, 696, false);
	case 696: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{4}; }, "4", false);
	case 697: return std::make_unique<MatchArm>(277, 698, false);
	case 698: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{5}; }, "5", false);
	case 699: return std::make_unique<MatchArm>(285, 700, false);
	case 700: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{6}; }, "6", false);
	case 701: return std::make_unique<MatchArm>(290, 702, false);
	case 702: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{7}; }, "7", false);
	case 703: return std::make_unique<MatchArm>(299, 704, false);
	case 704: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{8}; }, "8", false);
	case 705: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{706}, 687);
	case 706: return std::make_unique<ParameterExpression>(0, "distance", 260);
	case 707: return std::make_unique<MatchExpression<bool>>(15, std::vector<index_t>{708, 714, 718});
	case 708: return std::make_unique<MatchArm>(17, 709, false);
	case 709: return std::make_unique<FunctionCallExpression>("_can_get_drop_gold_skulltula", std::vector<index_t>{20}, 710);
	case 710: return std::make_unique<MatchExpression<bool>>(21, std::vector<index_t>{711, 284, 713});
	case 711: return std::make_unique<MatchArm>(712, 280, true);
	case 712: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{7, 255, 260, 265, 273, 278});
	case 713: return std::make_unique<MatchArm>(290, 293, false);
	case 714: return std::make_unique<MatchArm>(715, 2, false);
	case 715: return std::make_unique<MatchArmPatterns>(std::vector<index_t>{449, 716, 717});
	case 716: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_FIRE_KEESE), "RE_FIRE_KEESE"}; }, "RE_FIRE_KEESE", false);
	case 717: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerEnemy::RE_GUAY), "RE_GUAY"}; }, "RE_GUAY", false);
	case 718: return std::make_unique<MatchArm>(252, 719, false);
	case 719: return std::make_unique<OrExpression>(std::vector<index_t>{720, 721});
	case 720: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t { return scope.get("above_link"); }, "above_link", false);
	case 721: return std::make_unique<AndExpression>(std::vector<index_t>{722, 280});
	case 722: return std::make_unique<ComparisonExpression>("<=", 686, 723);
	case 723: return std::make_unique<FunctionCallExpression>("distance_to_int", std::vector<index_t>{724}, 687);
	case 724: return std::make_unique<ParameterExpression>(0, "distance", 278);
	case 725: return std::make_unique<AndExpression>(std::vector<index_t>{726, 357, 729});
	case 726: return std::make_unique<ExternFunctionCallExpression>(trick_adapter, "trick", std::vector<index_t>{727});
	case 727: return std::make_unique<ParameterExpression>(0, "key", 728);
	case 728: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerTrick::RT_KF_ADULT_GS), "RT_KF_ADULT_GS"}; }, "RT_KF_ADULT_GS", false);
	case 729: return std::make_unique<FunctionCallExpression>("can_kill", std::vector<index_t>{658, 730, 8, 9, 11, 13}, 14);
	case 730: return std::make_unique<ParameterExpression>(1, "distance", 255);
	case 731: return std::make_unique<AndExpression>(std::vector<index_t>{105, 582});
	case 732: return std::make_unique<AndExpression>(std::vector<index_t>{57, 582});
	case 733: return std::make_unique<AndExpression>(std::vector<index_t>{512, 734});
	case 734: return std::make_unique<ComparisonExpression>("<=", 735, 738);
	case 735: return std::make_unique<ExternFunctionCallExpression>(check_price_adapter, "check_price", std::vector<index_t>{736});
	case 736: return std::make_unique<ParameterExpression>(0, "check", 737);
	case 737: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerCheck::RC_UNKNOWN_CHECK), "RC_UNKNOWN_CHECK"}; }, "RC_UNKNOWN_CHECK", false);
	case 738: return std::make_unique<FunctionCallExpression>("wallet_capacity", std::vector<index_t>{}, 739);
	case 739: return std::make_unique<TernaryExpression>(740, 743, 744);
	case 740: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{741});
	case 741: return std::make_unique<ParameterExpression>(0, "item", 742);
	case 742: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_TYCOON_WALLET), "RG_TYCOON_WALLET"}; }, "RG_TYCOON_WALLET", false);
	case 743: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{999}; }, "999", false);
	case 744: return std::make_unique<TernaryExpression>(745, 748, 749);
	case 745: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{746});
	case 746: return std::make_unique<ParameterExpression>(0, "item", 747);
	case 747: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_GIANT_WALLET), "RG_GIANT_WALLET"}; }, "RG_GIANT_WALLET", false);
	case 748: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{500}; }, "500", false);
	case 749: return std::make_unique<TernaryExpression>(750, 753, 754);
	case 750: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{751});
	case 751: return std::make_unique<ParameterExpression>(0, "item", 752);
	case 752: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return EnumValue{CheckedHostInt(RandomizerGet::RG_ADULT_WALLET), "RG_ADULT_WALLET"}; }, "RG_ADULT_WALLET", false);
	case 753: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{200}; }, "200", false);
	case 754: return std::make_unique<TernaryExpression>(221, 755, 689);
	case 755: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t { return int32_t{99}; }, "99", false);
	default: throw std::out_of_range("Invalid tracker expression index.");
	}
}

uint64_t GetExpressionCount() {
	return 756;
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
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_QUEEN_GOHMA", 0}, 304},
		{LogicRootKey{LogicRootKind::Location, "RR_DEKU_TREE_BOSS_ROOM", "RC_DEKU_TREE_QUEEN_GOHMA_HEART", 0}, 304},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_EXIT", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BOSS_ROOM", "RR_KF_OUTSIDE_DEKU_TREE", 0}, 304},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_STICK_ACCESS", 0}, 307},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_1F", "LOGIC_NUT_ACCESS", 0}, 310},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_ENTRYWAY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_2F", 0}, 314},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_1F", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 318},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_2F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0}, 310},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_1F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_3F", 0}, 321},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_2F", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0}, 322},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_3F", "LOGIC_DEKU_TREE_MQ_2F_BURNED_WEB", 0}, 337},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_2F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_3F", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_SLINGSHOT_ROOM", "RR_DEKU_TREE_MQ_3F", 0}, 338},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0}, 341},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", "RR_DEKU_TREE_MQ_2F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_EYE_TARGET_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_COMPASS_ROOM", "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", 0}, 346},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_PAST_BOULDER_VINES", "RR_DEKU_TREE_MQ_COMPASS_ROOM", 0}, 365},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 367},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_1F", 0}, 321},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0}, 341},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0}, 372},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0}, 379},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "LOGIC_DEKU_TREE_MQ_CLEARED_SE_ROOM", 0}, 405},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0}, 310},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 373},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "LOGIC_DEKU_TREE_MQ_WATER_ROOM_TORCHES", 0}, 408},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0}, 417},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHEAST_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_STICK_ACCESS", 0}, 307},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "LOGIC_NUT_ACCESS", 0}, 310},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0}, 421},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_FRONT", 0}, 437},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0}, 443},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_WATER_ROOM_BACK", 0}, 443},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0}, 450},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_SOUTHWEST_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", 0}, 453},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0}, 459},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 310},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT_GRAVE_ROOM", 0}, 451},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_BASEMENT", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", 0}, 462},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_MQ_BASEMENT_LEDGE", 0}, 467},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_MQ_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 469},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0}, 310},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_ENTRYWAY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_LOBBY_2F", 0}, 314},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BASEMENT_LOWER", 0}, 318},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0}, 12},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 12},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_LOBBY_3F", 0}, 321},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_2F", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_LOBBY_3F", "LOGIC_DEKU_TREE_1F_BROKE_WEB", 0}, 474},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_LOBBY_2F", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_LOBBY_3F", "RR_DEKU_TREE_COMPASS_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_LOBBY", 0}, 475},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_2F_MIDDLE_ROOM", "RR_DEKU_TREE_SLINGSHOT_ROOM", 0}, 475},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_SLINGSHOT_ROOM", "RR_DEKU_TREE_2F_MIDDLE_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_COMPASS_ROOM", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_LOBBY", 0}, 456},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_COMPASS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 12},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_LOWER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 479},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_LOBBY", 0}, 484},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0}, 453},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_BASEMENT_UPPER", 0}, 486},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_LOWER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0}, 12},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_LOWER", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0}, 341},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_SCRUB_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0}, 487},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_FRONT", 0}, 487},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_WATER_ROOM_BACK", 0}, 453},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0}, 453},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_TORCH_ROOM", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_BACK_ROOM", 0}, 491},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", "RR_DEKU_TREE_BASEMENT_UPPER", 0}, 495},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_BACK_ROOM", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_PUSHED_BASEMENT_BLOCK", 0}, 459},
		{LogicRootKey{LogicRootKind::Event, "RR_DEKU_TREE_BASEMENT_UPPER", "LOGIC_DEKU_TREE_B1_BROKE_WEB", 0}, 310},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_LOWER", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_BASEMENT_BACK_LOBBY", 0}, 451},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_BASEMENT_UPPER", "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", 0}, 462},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BASEMENT_UPPER", 0}, 467},
		{LogicRootKey{LogicRootKind::Exit, "RR_DEKU_TREE_OUTSIDE_BOSS_ROOM", "RR_DEKU_TREE_BOSS_ENTRYWAY", 0}, 469},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_HOUSE", "RR_KF_LINKS_PORCH", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KF_LINKS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_LINKS_PORCH", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_FAIRY_ACCESS", 0}, 496},
		{LogicRootKey{LogicRootKind::Event, "RR_KOKIRI_FOREST", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0}, 511},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_BOULDER_LOOP", 0}, 451},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_LINKS_PORCH", 0}, 515},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_MIDOS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_SARIAS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_HOUSE_OF_TWINS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KNOW_IT_ALL_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_KOKIRI_SHOP", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_DEKU_TREE", 0}, 523},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_OUTSIDE_LOST_WOODS", 0}, 537},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_KF_RUPEE_ALCOVE", 0}, 547},
		{LogicRootKey{LogicRootKind::Exit, "RR_KOKIRI_FOREST", "RR_LW_BRIDGE_FROM_FOREST", 0}, 548},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_KOKIRI_SWORD_CHEST", 0}, 551},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_BOULDER_LOOP", "RR_KOKIRI_FOREST", 0}, 451},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_STICK_ACCESS", 0}, 325},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_NUT_ACCESS", 0}, 329},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_FAIRY_ACCESS", 0}, 497},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_OUTSIDE_DEKU_TREE", "LOGIC_SHOWED_MIDO_SWORD_AND_SHIELD", 0}, 511},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_DEKU_TREE_ENTRYWAY", 0}, 555},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_DEKU_TREE", "RR_KOKIRI_FOREST", 0}, 563},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_LEFT_CHEST", 0}, 552},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_TOP_RIGHT_CHEST", 0}, 552},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_LEFT_CHEST", 0}, 552},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_MIDOS_HOUSE", "RC_KF_MIDOS_BOTTOM_RIGHT_CHEST", 0}, 552},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_MIDOS_HOUSE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_SARIAS_HOUSE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_HOUSE_OF_TWINS", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KNOW_IT_ALL_HOUSE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_KOKIRI_SHOP", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_THE_LOST_WOODS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_RUPEE_ALCOVE", 0}, 566},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_OUTSIDE_LOST_WOODS", "RR_KF_STORMS_GROTTO", 0}, 568},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_RUPEE_ALCOVE", "RR_KOKIRI_FOREST", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FAIRY_ACCESS", 0}, 577},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_BUG_ACCESS", 0}, 582},
		{LogicRootKey{LogicRootKind::Event, "RR_KF_STORMS_GROTTO", "LOGIC_FISH_ACCESS", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_CHEST", 0}, 552},
		{LogicRootKey{LogicRootKind::Exit, "RR_KF_STORMS_GROTTO", "RR_KF_OUTSIDE_LOST_WOODS", 0}, 2},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_KAKARIKO_GATE_OPEN", 0}, 586},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_1_TORCH_CARPENTER", 0}, 591},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DOUBLE_CELL_CARPENTER", 0}, 596},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_DEAD_END_CARPENTER", 0}, 596},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_COULD_FREE_SLOPE_CARPENTER", 0}, 596},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_TH_RESCUED_ALL_CARPENTERS", 0}, 591},
		{LogicRootKey{LogicRootKind::Event, "RR_ROOT", "LOGIC_FREED_EPONA", 0}, 599},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_LINKS_POCKET", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TRIFORCE_COMPLETED", 0}, 604},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SARIA_SONG_HINT", 0}, 607},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_SONG_FROM_IMPA", 0}, 609},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_MALON_EGG", 0}, 609},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_HC_ZELDAS_LETTER", 0}, 609},
		{LogicRootKey{LogicRootKind::Location, "RR_ROOT", "RC_TOT_MASTER_SWORD", 0}, 613},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT", "RR_ROOT_EXITS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_CHILD_SPAWN", 0}, 105},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_ADULT_SPAWN", 0}, 57},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_MINUET_OF_FOREST_WARP", 0}, 618},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_BOLERO_OF_FIRE_WARP", 0}, 620},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_SERENADE_OF_WATER_WARP", 0}, 622},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_NOCTURNE_OF_SHADOW_WARP", 0}, 624},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_REQUIEM_OF_SPIRIT_WARP", 0}, 626},
		{LogicRootKey{LogicRootKind::Exit, "RR_ROOT_EXITS", "RR_PRELUDE_OF_LIGHT_WARP", 0}, 628},
		{LogicRootKey{LogicRootKind::Exit, "RR_CHILD_SPAWN", "RR_KF_LINKS_HOUSE", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_ADULT_SPAWN", "RR_TEMPLE_OF_TIME", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_MINUET_OF_FOREST_WARP", "RR_SACRED_FOREST_MEADOW", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_BOLERO_OF_FIRE_WARP", "RR_DMC_PAD_ENTRY", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_SERENADE_OF_WATER_WARP", "RR_LAKE_HYLIA", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_REQUIEM_OF_SPIRIT_WARP", "RR_DESERT_COLOSSUS", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_NOCTURNE_OF_SHADOW_WARP", "RR_GRAVEYARD_WARP_PAD_REGION", 0}, 2},
		{LogicRootKey{LogicRootKind::Exit, "RR_PRELUDE_OF_LIGHT_WARP", "RR_TEMPLE_OF_TIME", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_LEFT", 0}, 630},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_BEEHIVE_RIGHT", 0}, 630},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_COW", 0}, 645},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_1", 0}, 503},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_2", 0}, 503},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_SPROUT_FAIRY_3", 0}, 503},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY", 0}, 497},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE_FAIRY_BIG", 0}, 509},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY", 0}, 497},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE_FAIRY_BIG", 0}, 509},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY", 0}, 497},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE_FAIRY_BIG", 0}, 509},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY", 0}, 578},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE_FAIRY_BIG", 0}, 509},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_FISH", 0}, 649},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BRIDGE_RUPEE", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEHIND_MIDOS_RUPEE", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_WEST_RUPEE", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SOUTH_GRASS_EAST_RUPEE", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_WEST_RUPEE", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_NORTH_GRASS_EAST_RUPEE", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_WEST_HEART", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_EAST_HEART", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_SARIAS_ROOF_NORTH_HEART", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_1", 0}, 652},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_2", 0}, 652},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_3", 0}, 652},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_4", 0}, 652},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_5", 0}, 652},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RUPEE_6", 0}, 652},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_BEAN_RED_RUPEE", 0}, 652},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_1", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_BOULDER_RUPEE_2", 0}, 105},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_LEFT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_TOP_RIGHT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_LEFT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_SARIAS_HOUSE", "RC_KF_SARIAS_BOTTOM_RIGHT_HEART", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_1", 0}, 654},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_2", 0}, 654},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_3", 0}, 654},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_4", 0}, 654},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_5", 0}, 654},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RUPEE_6", 0}, 654},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_BEAN_RED_RUPEE", 0}, 654},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_1", 0}, 655},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_2", 0}, 655},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_3", 0}, 655},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_4", 0}, 655},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_5", 0}, 655},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RUPEE_6", 0}, 655},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_RUPEE_ALCOVE", "RC_KF_BEAN_RED_RUPEE", 0}, 655},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_KNOW_IT_ALL_HOUSE", 0}, 656},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_BEAN_PATCH", 0}, 668},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_GS_HOUSE_OF_TWINS", 0}, 677},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_1", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_2", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_3", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_4", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_5", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_6", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_7", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_8", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_9", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_10", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_11", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_CHILD_GRASS_12", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_1", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_2", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_3", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_4", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_5", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_6", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_7", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_8", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_9", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_10", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_11", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_12", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_13", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_14", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_15", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_16", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_17", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_18", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_19", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KOKIRI_FOREST", "RC_KF_ADULT_GRASS_20", 0}, 732},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_1", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_2", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_BOULDER_LOOP", "RC_KF_CHILD_GRASS_MAZE_3", 0}, 731},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_1", 0}, 582},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_2", 0}, 582},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_3", 0}, 582},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GRASS_4", 0}, 582},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_LEFT_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_DEKU_TREE", "RC_KF_DEKU_TREE_RIGHT_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_OUTSIDE_LOST_WOODS", "RC_KF_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_STORMS_GROTTO", "RC_KF_STORMS_GROTTO_GOSSIP_STONE", 0}, 2},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_LINKS_HOUSE", "RC_KF_LINKS_HOUSE_POT", 0}, 459},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_1", 0}, 459},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_HOUSE_OF_TWINS", "RC_KF_TWINS_HOUSE_POT_2", 0}, 459},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_1", 0}, 459},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KNOW_IT_ALL_HOUSE", "RC_KF_BROTHERS_HOUSE_POT_2", 0}, 459},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_1", 0}, 733},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_2", 0}, 733},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_3", 0}, 733},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_4", 0}, 733},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_5", 0}, 733},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_6", 0}, 733},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_7", 0}, 733},
		{LogicRootKey{LogicRootKind::Location, "RR_KF_KOKIRI_SHOP", "RC_KF_SHOP_ITEM_8", 0}, 733},
	};
	return roots;
}
