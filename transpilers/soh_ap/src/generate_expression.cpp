#include "soh_ap.h"

#include <format>
#include <sstream>
#include <optional>
#include <unordered_map>

namespace rls::transpilers::soh_ap {

std::string SohApTranspiler::GenerateExpression(const rls::ast::BoolLiteral& node) const {
	return node.value ? "True" : "False";
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::IntLiteral& node) const {
	return std::to_string(node.value);
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::Identifier& node) const {
	return node.name;
}

// Returns the Python operator precedence for an expression node.
// Lower values bind tighter. Non-compound nodes return 0 (tightest).
// Precedence is from https://docs.python.org/3/reference/expressions.html#operator-precedence
int SohApTranspiler::GetPythonPrecedence(const rls::ast::ExprPtr& expr) const {
	if (auto* bin = std::get_if<rls::ast::BinaryExpr>(&expr->node)) {
		switch (bin->op) {
		case rls::ast::BinaryOp::Mul:
		case rls::ast::BinaryOp::Div:
            return 6;
		case rls::ast::BinaryOp::Add:
		case rls::ast::BinaryOp::Sub:
            return 7;
		case rls::ast::BinaryOp::Lt:
		case rls::ast::BinaryOp::LtEq:
		case rls::ast::BinaryOp::Gt:
		case rls::ast::BinaryOp::GtEq:
		case rls::ast::BinaryOp::Eq:
		case rls::ast::BinaryOp::NotEq:
            return 12;
		case rls::ast::BinaryOp::And:
            return 14;
		case rls::ast::BinaryOp::Or:
            return 15;
		default: return 0;
		}
	}
	if (std::holds_alternative<rls::ast::TernaryExpr>(expr->node)) {
		return 16;
	}
	return 0;
}

// Generates an expression, wrapping in parentheses when the child's Python
// precedence is looser than the parent's (or equal on the right side of
// a left-associative operator).
std::string SohApTranspiler::GenerateChildExpression(
    const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild) 
    const 
{
	auto result = GenerateExpression(expr);
	int childPrec = GetPythonPrecedence(expr);
	if (childPrec > parentPrec || (isRightChild && childPrec == parentPrec)) {
		return "(" + result + ")";
	}
	return result;
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::UnaryExpr& node) const {
	switch (node.op) {
	case rls::ast::UnaryOp::Not:
		return "!" + GenerateChildExpression(node.operand, 3);
	default:
		return "";
	}
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::BinaryExpr& node) const {
    switch (node.op) {
    case rls::ast::BinaryOp::And:
        return GenerateChildExpression(node.left, 14) + " and " + GenerateChildExpression(node.right, 14, true);
    case rls::ast::BinaryOp::Or:
        return GenerateChildExpression(node.left, 15) + " or " + GenerateChildExpression(node.right, 15, true);
    case rls::ast::BinaryOp::Eq:
        return GenerateChildExpression(node.left, 12) + " == " + GenerateChildExpression(node.right, 12, true);
    case rls::ast::BinaryOp::NotEq:
        return GenerateChildExpression(node.left, 12) + " != " + GenerateChildExpression(node.right, 12, true);
    case rls::ast::BinaryOp::Lt:
        return GenerateChildExpression(node.left, 12) + " < " + GenerateChildExpression(node.right, 12, true);
    case rls::ast::BinaryOp::LtEq:
        return GenerateChildExpression(node.left, 12) + " <= " + GenerateChildExpression(node.right, 12, true);
    case rls::ast::BinaryOp::Gt:
        return GenerateChildExpression(node.left, 12) + " > " + GenerateChildExpression(node.right, 12, true);
    case rls::ast::BinaryOp::GtEq:
        return GenerateChildExpression(node.left, 12) + " >= " + GenerateChildExpression(node.right, 12, true);
    case rls::ast::BinaryOp::Add:
        return GenerateChildExpression(node.left, 7) + " + " + GenerateChildExpression(node.right, 7, true);
    case rls::ast::BinaryOp::Sub:
        return GenerateChildExpression(node.left, 7) + " - " + GenerateChildExpression(node.right, 7, true);
    case rls::ast::BinaryOp::Mul:
        return GenerateChildExpression(node.left, 6) + " * " + GenerateChildExpression(node.right, 6, true);
    case rls::ast::BinaryOp::Div:
        return GenerateChildExpression(node.left, 6) + " / " + GenerateChildExpression(node.right, 6, true);
    default:
        return "";
    }
}

// Python ternary syntax is "a if test else b"
std::string SohApTranspiler::GenerateExpression(const rls::ast::TernaryExpr& node) const {
    return GenerateExpression(node.thenBranch) + " if " +
           GenerateChildExpression(node.condition, 15) + " else " +
		   GenerateExpression(node.elseBranch);
}

// TODO Handle Host Functions
std::string SohApTranspiler::GenerateExpression(const rls::ast::CallExpr& node) const {
    auto resolvedPtr = project.getResolvedCallArgs(&node);
    if (resolvedPtr == nullptr) {
        // Unknown calls or calls with semantic errors are blocked earlier in sema;
        // emit empty as a defensive fallback so generation does not invent call forms.
        return "";
    }
    const auto& resolved = *resolvedPtr;

    std::ostringstream oss;
    oss << node.function << "(";
    for (size_t i = 0; i < resolved.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << GenerateExpression(resolved[i]->node);
    }
    oss << ")";
    return oss.str();
}

// TODO Figure out Shared blocks
std::string SohApTranspiler::GenerateExpression(const rls::ast::SharedBlock& node) const {
  return "NOT IMPLEMENTED";
}

// TODO Figure out AnyAge Blocks
// The Python AP implementation doesn't currently have an AnyAge function
std::string SohApTranspiler::GenerateExpression(const rls::ast::AnyAgeBlock& node) const {
	return "NOT IMPLEMENTED";
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::MatchExpr& node) const {
	std::ostringstream oss;
	oss << "soh_match(";

	for (size_t i = 0; i < node.arms.size(); i++) {
		const auto& arm = node.arms[i];

		if (i > 0) oss << ", ";

        // Condition - lambda discriminant: discriminant == P1 or discriminant == P2
        if (arm.isDefault) {
            oss << "(lambda: true), ";
        } else {
            oss << "(lambda " << node.discriminant << ": ";
            for (size_t j = 0; j < arm.patterns.size(); j++) {
                if (j > 0) oss << " or ";
                oss << node.discriminant << " == " << arm.patterns[j];
            }
            oss << "), ";
        }

        // Body - lambda: <body_expression>
		oss << "(lambda: " << GenerateExpression(arm.body) << "), ";

		// Fallthrough flag
		oss << (arm.fallthrough ? "True" : "False");
	}

	oss << ")";
	return oss.str();
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::Expr::Variant& node) const {
	return std::visit([&](const auto& node) {
		return SohApTranspiler::GenerateExpression(node);
	}, node);
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::ExprPtr& expr) const {
	return GenerateExpression(expr->node);
}

} // rls::transpilers::soh_ap
