#pragma once

#include <string>
#include <string_view>

#include "framework.h"

namespace rls::lsp::detail::endpoints {

inline const json* asObject(const json* value) {
    if (value == nullptr || !value->is_object()) {
        return nullptr;
    }
    return value;
}

inline const json* findMember(const json* object, std::string_view key) {
    if (object == nullptr || !object->is_object()) {
        return nullptr;
    }

    const auto it = object->find(std::string(key));
    if (it == object->end()) {
        return nullptr;
    }

    return &(*it);
}

inline const json* findObjectMember(const json* object, std::string_view key) {
    return asObject(findMember(object, key));
}

inline const json* findArrayMember(const json* object, std::string_view key) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_array()) {
        return nullptr;
    }
    return value;
}

inline std::string stringMemberOr(const json* object, std::string_view key, std::string fallback) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_string()) {
        return fallback;
    }
    return value->get<std::string>();
}

inline int intMemberOr(const json* object, std::string_view key, int fallback) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_number_integer()) {
        return fallback;
    }
    return value->get<int>();
}

inline bool boolMemberOr(const json* object, std::string_view key, bool fallback) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_boolean()) {
        return fallback;
    }
    return value->get<bool>();
}

} // namespace rls::lsp::detail::endpoints
