#pragma once

#include <string>
#include <string_view>

#include "framework.h"

namespace rls::lsp::detail::endpoints {

/// Returns the input value when it is a JSON object, otherwise nullptr.
inline const json* asObject(const json* value) {
    if (value == nullptr || !value->is_object()) {
        return nullptr;
    }
    return value;
}

/// Looks up a named member in a JSON object and returns nullptr when the
/// object is missing, not an object, or the member does not exist.
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

/// Looks up a named member and returns it only when the member is itself a
/// JSON object.
inline const json* findObjectMember(const json* object, std::string_view key) {
    return asObject(findMember(object, key));
}

/// Looks up a named member and returns it only when the member is a JSON
/// array.
inline const json* findArrayMember(const json* object, std::string_view key) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_array()) {
        return nullptr;
    }
    return value;
}

/// Reads a string member from an object, falling back when the member is
/// missing or has a different JSON type.
inline std::string stringMemberOr(const json* object, std::string_view key, std::string fallback) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_string()) {
        return fallback;
    }
    return value->get<std::string>();
}

/// Reads an integer member from an object, falling back when the member is
/// missing or not an integer value.
inline int intMemberOr(const json* object, std::string_view key, int fallback) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_number_integer()) {
        return fallback;
    }
    return value->get<int>();
}

/// Reads a boolean member from an object, falling back when the member is
/// missing or not a boolean value.
inline bool boolMemberOr(const json* object, std::string_view key, bool fallback) {
    const json* value = findMember(object, key);
    if (value == nullptr || !value->is_boolean()) {
        return fallback;
    }
    return value->get<bool>();
}

} // namespace rls::lsp::detail::endpoints
