#pragma once

/// Shared type-related helpers used by multiple sema passes.

#include "ast.h"

#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <variant>

namespace rls::sema {

/// Human-readable name for a Type enum value.
inline std::string_view typeName(ast::Type t) {
	switch (t) {
	case ast::Type::Bool:       return "Bool";
	case ast::Type::Int:        return "Int";
	case ast::Type::String:     return "String";
	case ast::Type::List:       return "List";
	case ast::Type::Callable:   return "Callable";
	case ast::Type::Condition:  return "Condition";
	case ast::Type::Enum:       return "Enum";
	case ast::Type::Region:     return "Region";
	case ast::Type::Event:      return "Event";
	case ast::Type::Location:   return "Location";
	case ast::Type::Void:       return "Void";
	case ast::Type::Error:      return "<error>";
	}
	return "<unknown>";
}

/// Returns true if the type can be implicitly used where Bool is expected.
/// Int values have truthiness (zero/non-zero).
inline bool isBoolCompatible(ast::Type t) {
	return t == ast::Type::Bool
		|| t == ast::Type::Int;
}

inline bool isDomainEnumCompatible(
	ast::Type domainType, std::optional<std::string_view> enumName) {
	if (!enumName) return false;
	switch (domainType) {
	case ast::Type::Region:
		return *enumName == "Region";
	case ast::Type::Event:
		return *enumName == "Event";
	case ast::Type::Location:
		return *enumName == "Location";
	default:
		return false;
	}
}

inline bool areDomainAndEnumCompatible(
	ast::Type expected, std::optional<std::string_view> expectedEnum,
	ast::Type actual, std::optional<std::string_view> actualEnum) {
	return (actual == ast::Type::Enum && isDomainEnumCompatible(expected, actualEnum))
		|| (expected == ast::Type::Enum && isDomainEnumCompatible(actual, expectedEnum));
}

/// Parse a built-in type annotation string (e.g. "Bool") to a Type enum value.
/// Named enum annotations are resolved from the Project's enum registry.
/// Returns nullopt if the annotation is not a recognized type name.
inline std::optional<ast::Type> typeFromAnnotation(std::string_view annotation) {
	struct Entry {
		std::string_view name;
		ast::Type type;
	};

	static constexpr Entry table[] = {
		{"Bool",       ast::Type::Bool},
		{"Int",        ast::Type::Int},
		{"String",     ast::Type::String},
		{"List",       ast::Type::List},
		{"Callable",   ast::Type::Callable},
		{"Condition",  ast::Type::Condition},
		{"Enum",       ast::Type::Enum},
		{"Region",     ast::Type::Region},
		{"Event",      ast::Type::Event},
		{"Location",   ast::Type::Location},
	};

	for (const auto& [name, type] : table) {
		if (annotation == name) return type;
	}
	return std::nullopt;
}

/// A resolved type annotation, including the identity of a named enum.
struct ResolvedTypeAnnotation {
	ast::Type type;
	std::optional<std::string_view> enumName;
};

/// Resolve a type annotation against the built-in types and project enum names.
inline std::optional<ResolvedTypeAnnotation> resolveTypeAnnotation(
	const ast::Project& project, std::string_view annotation)
{
	if (auto type = typeFromAnnotation(annotation)) {
		return ResolvedTypeAnnotation{*type, std::nullopt};
	}
	if (project.getEnumInfo(annotation) != nullptr) {
		return ResolvedTypeAnnotation{ast::Type::Enum, annotation};
	}
	return std::nullopt;
}

/// Simple glob pattern matcher supporting '*' wildcard.
/// '*' matches zero or more characters.
inline bool globMatches(std::string_view pattern, std::string_view value) {
	size_t p = 0;
	size_t v = 0;
	size_t star = std::string_view::npos;
	size_t backtrack = 0;

	while (v < value.size()) {
		if (p < pattern.size() && pattern[p] == value[v]) {
			++p;
			++v;
			continue;
		}
		if (p < pattern.size() && pattern[p] == '*') {
			star = p++;
			backtrack = v;
			continue;
		}
		if (star != std::string_view::npos) {
			p = star + 1;
			v = ++backtrack;
			continue;
		}
		return false;
	}

	while (p < pattern.size() && pattern[p] == '*') {
		++p;
	}

	return p == pattern.size();
}

/// Recursively walk an expression tree and collect the names of every
/// function referenced by a CallExpr node.
inline void collectCallNames(
	const ast::Expr& expr,
	std::unordered_set<std::string>& out)
{
	std::visit([&](const auto& node) {
		using N = std::decay_t<decltype(node)>;
		if constexpr (std::is_same_v<N, ast::UnaryExpr>) {
			collectCallNames(*node.operand, out);
		} else if constexpr (std::is_same_v<N, ast::BinaryExpr>) {
			collectCallNames(*node.left, out);
			collectCallNames(*node.right, out);
		} else if constexpr (std::is_same_v<N, ast::TernaryExpr>) {
			collectCallNames(*node.condition, out);
			collectCallNames(*node.thenBranch, out);
			collectCallNames(*node.elseBranch, out);
		} else if constexpr (std::is_same_v<N, ast::CallExpr>) {
			out.insert(node.callee.text);
			for (const auto& arg : node.args) {
				collectCallNames(*arg.value, out);
			}
		} else if constexpr (std::is_same_v<N, ast::InvokeExpr>) {
			collectCallNames(*node.callee, out);
		} else if constexpr (std::is_same_v<N, ast::MatchExpr>) {
			for (const auto& arm : node.arms) {
				collectCallNames(*arm.body, out);
			}
		} else if constexpr (std::is_same_v<N, ast::ListExpr>) {
			for (const auto& element : node.elements) {
				collectCallNames(*element, out);
			}
		}
		// Leaf nodes have no child expressions.
	}, expr.node);
}

} // namespace rls::sema
