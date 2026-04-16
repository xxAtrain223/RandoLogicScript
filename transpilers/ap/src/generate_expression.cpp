namespace rls::transpilers::ap {

#include "generate_expression.h"

#include <format>
#include <sstream>

namespace rls::transpilers::soh {

std::string GenerateExpression(const rls::ast::Expr::Variant& node);
  
static std::string GenerateExpression(const rls::ast::BoolLiteral& node) {
	return node.value ? "True" : "False";
}

static std::string GenerateExpression(const rls::ast::IntLiteral& node) {
	return std::to_string(node.value);
}

static std::string GenerateExpression(const rls::ast::Identifier& node) {
	return node.name;
}

static std::string GenerateExpression(const rls::ast::KeywordExpr& node) {
    switch (node.keyword) {
    case rls::ast::Keyword::IsChild:
        return "is_child(bundle)";
    case rls::ast::Keyword::IsAdult:
        return "is_adult(bundle)";
    case rls::ast::Keyword::AtDay:
        return "at_day(bundle)";
    case rls::ast::Keyword::AtNight:
        return "at_night(bundle)";
    // TODO Handle IsVanilla and IsMq
    case rls::ast::Keyword::IsVanilla:
        return "NOT IMPLEMENTED";
    case rls::ast::Keyword::IsMq:
        return "NOT IMPLEMENTED";
    default:
        return "";
    }
}

// Returns the Python operator precedence for an expression node.
// Lower values bind tighter. Non-compound nodes return 0 (tightest).
// Precedence is from https://docs.python.org/3/reference/expressions.html#operator-precedence
static int GetPythonPrecedence(const rls::ast::ExprPtr& expr) {
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
static std::string GenerateChildExpression(
	const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false)
{
	auto result = GenerateExpression(expr);
	int childPrec = GetPythonPrecedence(expr);
	if (childPrec > parentPrec || (isRightChild && childPrec == parentPrec)) {
		return "(" + result + ")";
	}
	return result;
}

static std::string GenerateExpression(const rls::ast::UnaryExpr& node) {
	switch (node.op) {
	case rls::ast::UnaryOp::Not:
		return "!" + GenerateChildExpression(node.operand, 3);
	default:
		return "";
	}
}

static std::string GenerateExpression(const rls::ast::UnaryExpr& node) {
	switch (node.op) {
	case rls::ast::UnaryOp::Not:
		return "not " + GenerateChildExpression(node.operand, 13);
	default:
		return "";
	}
}

static std::string GenerateExpression(const rls::ast::BinaryExpr& node) {
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
static std::string GenerateExpression(const rls::ast::TernaryExpr& node) {
    return GenerateExpression(node.thenBranch) + " if " +
           GenertateChildExpresssion(node.condition, 15) + " else " +
		   GenerateExpression(node.elseBranch);
}

// TODO Handle Host Functions
static std::string GenerateExpression(const rls::ast::CallExpr& node) {
  return "NOT IMPLEMENTED";
}

// TODO Figure out Shared blocks
static std::string GenerateExpression(const rls::ast::SharedBlock& node) {
  return "NOT IMPLEMENTED";
}

// TODO Figure out AnyAge Blocks
// The Python AP implementation doesn't currently have an AnyAge function
static std::string GenerateExpression(const rls::ast::AnyAgeBlock& node) {
	return "NOT IMPLEMENTED";

// TODO Figure out Match Statements
static std::string GenerateExpression(const rls::ast::MatchExpr& node) {
    return "NOT IMPLEMENTED";
}

static std::string GenerateExpression(const rls::ast::Expr::Variant& node) {
	return std::visit([&](const auto& node) {
		return GenerateExpression(node);
	}, node);
}

std::string GenerateExpression(const rls::ast::ExprPtr& expr) {
	return GenerateExpression(expr->node);
}

} // rls::transpilers::ap
