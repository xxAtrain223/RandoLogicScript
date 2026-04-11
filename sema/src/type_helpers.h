#pragma once

/// Shared type-related helpers used by multiple sema passes.

#include "ast.h"

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
	case ast::Type::Item:       return "Item";
	case ast::Type::Enemy:      return "Enemy";
	case ast::Type::Distance:   return "Distance";
	case ast::Type::Trick:      return "Trick";
	case ast::Type::Setting:    return "Setting";
	case ast::Type::Region:     return "Region";
	case ast::Type::Check:      return "Check";
	case ast::Type::Logic:      return "Logic";
	case ast::Type::Scene:      return "Scene";
	case ast::Type::Dungeon:    return "Dungeon";
	case ast::Type::Area:       return "Area";
	case ast::Type::Trial:      return "Trial";
	case ast::Type::WaterLevel: return "WaterLevel";
	case ast::Type::Void:       return "Void";
	case ast::Type::Error:      return "<error>";
	}
	return "<unknown>";
}

/// Returns true if the type can be implicitly used where Bool is expected.
/// Int and Setting both have truthiness (zero/non-zero).
inline bool isBoolCompatible(ast::Type t) {
	return t == ast::Type::Bool
		|| t == ast::Type::Int
		|| t == ast::Type::Setting;
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
			out.insert(node.function);
			for (const auto& arg : node.args) {
				collectCallNames(*arg.value, out);
			}
		} else if constexpr (std::is_same_v<N, ast::SharedBlock>) {
			for (const auto& branch : node.branches) {
				collectCallNames(*branch.condition, out);
			}
		} else if constexpr (std::is_same_v<N, ast::AnyAgeBlock>) {
			collectCallNames(*node.body, out);
		} else if constexpr (std::is_same_v<N, ast::MatchExpr>) {
			for (const auto& arm : node.arms) {
				collectCallNames(*arm.body, out);
			}
		}
		// Leaf nodes (BoolLiteral, IntLiteral, Identifier, KeywordExpr)
		// have no child expressions.
	}, expr.node);
}

} // namespace rls::sema
