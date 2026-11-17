#pragma once

#include "ast.h"
#include "soh/enum_mappings.h"

#include <string>

namespace rls::transpilers::soh {

template <typename T>
std::string nodeType(const rls::ast::Project& project, const T* node) {
    const auto type = project.getType(node);
    if (!type) return "missing_type";

    switch (*type) {
    case rls::ast::Type::Bool: return "bool";
    case rls::ast::Type::Int: return "int";
    case rls::ast::Type::Callable:
    case rls::ast::Type::Condition: return "std::function<bool()>";
    case rls::ast::Type::Enum:
        if (const auto enumType = project.getEnumType(node)) {
            if (const auto* mapping = findHostEnumMapping(*enumType)) {
                return std::string(mapping->cppType);
            }
            return std::string(*enumType);
        }
        return "int";
    case rls::ast::Type::Region: return "RandomizerRegion";
    case rls::ast::Type::Event: return "LogicVal";
    case rls::ast::Type::Location: return "RandomizerCheck";
    default: return "unsupported_type";
    }
}

} // namespace rls::transpilers::soh
