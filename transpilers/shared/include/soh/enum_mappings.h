#pragma once

#include <string_view>

namespace rls::transpilers::soh {

struct HostEnumMapping {
	std::string_view rlsName;
	std::string_view cppType;
	std::string_view cppValueNamespace;
};

inline const HostEnumMapping* findHostEnumMapping(std::string_view enumName) {
	static constexpr HostEnumMapping mappings[] = {
		{"Item",       "RandomizerGet",        "RandomizerGet"},
		{"Enemy",      "RandomizerEnemy",      "RandomizerEnemy"},
		{"Distance",   "EnemyDistance",        "EnemyDistance"},
		{"Trick",      "RandomizerTrick",      "RandomizerTrick"},
		{"Setting",    "RandomizerSettingKey", ""},
		{"Region",     "RandomizerRegion",     "RandomizerRegion"},
		{"Location",   "RandomizerCheck",      "RandomizerCheck"},
		{"Event",      "LogicVal",             "LogicVal"},
		{"Scene",      "SceneID",              "SceneID"},
		{"Dungeon",    "DungeonKey",           "DungeonKey"},
		{"Area",       "RandomizerArea",       "RandomizerArea"},
		{"Trial",      "TrialKey",             "TrialKey"},
	};

	for (const auto& mapping : mappings) {
		if (mapping.rlsName == enumName) {
			return &mapping;
		}
	}
	return nullptr;
}

} // namespace rls::transpilers::soh