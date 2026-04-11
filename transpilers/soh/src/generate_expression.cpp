#include "generate_expression.h"

#include <sstream>

namespace rls::transpilers::soh {

static std::string GenerateExpression(const rls::ast::BoolLiteral& node) {
	return node.value ? "true" : "false";
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
        return "logic->IsChild";
    case rls::ast::Keyword::IsAdult:
        return "logic->IsAdult";
    case rls::ast::Keyword::AtDay:
        return "logic->AtDay";
    case rls::ast::Keyword::AtNight:
        return "logic->AtNight";
    default:
        return "";
    }
}

static std::string GenerateExpression(const rls::ast::UnaryExpr& node) {
	switch (node.op) {
	case rls::ast::UnaryOp::Not:
		return "!" + GenerateExpression(node.operand);
	default:
		return "";
	}
}

static std::string GenerateExpression(const rls::ast::BinaryExpr& node) {
    switch (node.op) {
    case rls::ast::BinaryOp::And:
        return GenerateExpression(node.left) + " && " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Or:
        return GenerateExpression(node.left) + " || " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Eq:
        return GenerateExpression(node.left) + " == " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::NotEq:
        return GenerateExpression(node.left) + " != " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Lt:
        return GenerateExpression(node.left) + " < " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::LtEq:
        return GenerateExpression(node.left) + " <= " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Gt:
        return GenerateExpression(node.left) + " > " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::GtEq:
        return GenerateExpression(node.left) + " >= " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Add:
        return GenerateExpression(node.left) + " + " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Sub:
        return GenerateExpression(node.left) + " - " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Mul:
        return GenerateExpression(node.left) + " * " + GenerateExpression(node.right);
    case rls::ast::BinaryOp::Div:
        return GenerateExpression(node.left) + " / " + GenerateExpression(node.right);
    default:
        return "";
    }
}

static std::string GenerateExpression(const rls::ast::TernaryExpr& node) {
	return "";
}

static std::string GenerateExpression(const rls::ast::CallExpr& node) {
	return "";
}

static std::string GenerateExpression(const rls::ast::SharedBlock& node) {
	return "";
}

static std::string GenerateExpression(const rls::ast::AnyAgeBlock& node) {
	return "";
}

static std::string GenerateExpression(const rls::ast::MatchExpr& node) {
	return "";
}

std::string GenerateExpression(const rls::ast::ExprPtr& expr) {
	return std::visit([&](const auto& node) {
		return GenerateExpression(node);
	}, expr->node);
}

}