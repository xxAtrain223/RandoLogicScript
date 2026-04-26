#include "soh.h"

#include <optional>
#include <sstream>
#include <unordered_map>

namespace rls::transpilers::soh {

std::string SohTranspiler::GenerateExpression(const rls::ast::BoolLiteral& node) const {
	return node.value ? "true" : "false";
}

std::string SohTranspiler::GenerateExpression(const rls::ast::IntLiteral& node) const {
	return std::to_string(node.value);
}

std::string SohTranspiler::GenerateExpression(const rls::ast::Identifier& node) const {
	return node.name;
}

std::string SohTranspiler::GenerateExpression(const rls::ast::KeywordExpr& node) const {
    switch (node.keyword) {
    case rls::ast::Keyword::IsChild:
        return "is_child()";
    case rls::ast::Keyword::IsAdult:
        return "is_adult()";
    case rls::ast::Keyword::AtDay:
        return "at_day()";
    case rls::ast::Keyword::AtNight:
        return "at_night()";
    case rls::ast::Keyword::IsVanilla:
        return "is_vanilla()";
    case rls::ast::Keyword::IsMq:
        return "is_mq()";
    default:
        return "";
    }
}

// Returns the C++ operator precedence for an expression node.
// Lower values bind tighter. Non-compound nodes return 0 (tightest).
int SohTranspiler::GetCppPrecedence(const rls::ast::ExprPtr& expr) const {
	if (auto* bin = std::get_if<rls::ast::BinaryExpr>(&expr->node)) {
		switch (bin->op) {
		case rls::ast::BinaryOp::Mul:
		case rls::ast::BinaryOp::Div:
            return 5;
		case rls::ast::BinaryOp::Add:
		case rls::ast::BinaryOp::Sub:
            return 6;
		case rls::ast::BinaryOp::Lt:
		case rls::ast::BinaryOp::LtEq:
		case rls::ast::BinaryOp::Gt:
		case rls::ast::BinaryOp::GtEq:
            return 9;
		case rls::ast::BinaryOp::Eq:
		case rls::ast::BinaryOp::NotEq:
            return 10;
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

// Generates an expression, wrapping in parentheses when the child's C++
// precedence is looser than the parent's (or equal on the right side of
// a left-associative operator).
std::string SohTranspiler::GenerateChildExpression(
    const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild)
    const
{
	auto result = GenerateExpression(expr);
	int childPrec = GetCppPrecedence(expr);
	if (childPrec > parentPrec || (isRightChild && childPrec == parentPrec)) {
		return "(" + result + ")";
	}
	return result;
}

std::string SohTranspiler::GenerateExpression(const rls::ast::UnaryExpr& node) const {
	switch (node.op) {
	case rls::ast::UnaryOp::Not:
		return "!" + GenerateChildExpression(node.operand, 3);
	default:
		return "";
	}
}

std::string SohTranspiler::GenerateExpression(const rls::ast::BinaryExpr& node) const {
    switch (node.op) {
    case rls::ast::BinaryOp::And:
        return GenerateChildExpression(node.left, 14) + " && " + GenerateChildExpression(node.right, 14, true);
    case rls::ast::BinaryOp::Or:
        return GenerateChildExpression(node.left, 15) + " || " + GenerateChildExpression(node.right, 15, true);
    case rls::ast::BinaryOp::Eq:
        return GenerateChildExpression(node.left, 10) + " == " + GenerateChildExpression(node.right, 10, true);
    case rls::ast::BinaryOp::NotEq:
        return GenerateChildExpression(node.left, 10) + " != " + GenerateChildExpression(node.right, 10, true);
    case rls::ast::BinaryOp::Lt:
        return GenerateChildExpression(node.left, 9) + " < " + GenerateChildExpression(node.right, 9, true);
    case rls::ast::BinaryOp::LtEq:
        return GenerateChildExpression(node.left, 9) + " <= " + GenerateChildExpression(node.right, 9, true);
    case rls::ast::BinaryOp::Gt:
        return GenerateChildExpression(node.left, 9) + " > " + GenerateChildExpression(node.right, 9, true);
    case rls::ast::BinaryOp::GtEq:
        return GenerateChildExpression(node.left, 9) + " >= " + GenerateChildExpression(node.right, 9, true);
    case rls::ast::BinaryOp::Add:
        return GenerateChildExpression(node.left, 6) + " + " + GenerateChildExpression(node.right, 6, true);
    case rls::ast::BinaryOp::Sub:
        return GenerateChildExpression(node.left, 6) + " - " + GenerateChildExpression(node.right, 6, true);
    case rls::ast::BinaryOp::Mul:
        return GenerateChildExpression(node.left, 5) + " * " + GenerateChildExpression(node.right, 5, true);
    case rls::ast::BinaryOp::Div:
        return GenerateChildExpression(node.left, 5) + " / " + GenerateChildExpression(node.right, 5, true);
    default:
        return "";
    }
}

std::string SohTranspiler::GenerateExpression(const rls::ast::TernaryExpr& node) const {
	return GenerateChildExpression(node.condition, 15) + " ? " +
		   GenerateExpression(node.thenBranch) + " : " +
		   GenerateExpression(node.elseBranch);
}

std::string SohTranspiler::GenerateExpression(const rls::ast::CallExpr& node) const {
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

std::string SohTranspiler::GenerateExpression(const rls::ast::SharedBlock& node) const {
    std::ostringstream oss;

    const auto& firstBranch = node.branches[0];
    oss << "SpiritShared(" << firstBranch.region.value_or("") << ", "
        << "[]{return " << GenerateExpression(firstBranch.condition) << ";}, "
        << (node.anyAge ? "true" : "false");

    for (int i = 1; i < node.branches.size(); i++) {
        oss << ", " << node.branches[i].region.value_or("") << ", "
            << "[]{return " << GenerateExpression(node.branches[i].condition) << ";}";
    }

    oss << ")";

	return oss.str();
}

std::string SohTranspiler::GenerateExpression(const rls::ast::AnyAgeBlock& node) const {
	return "AnyAgeTime([]{return " + GenerateExpression(node.body) + ";})";
}

std::string SohTranspiler::GenerateExpression(const rls::ast::MatchExpr& node) const {
	std::ostringstream oss;
	oss << "rls::match(";

	for (size_t i = 0; i < node.arms.size(); i++) {
		const auto& arm = node.arms[i];

		if (i > 0) oss << ", ";

		// Condition lambda: [&]{ return discriminant == P1 || discriminant == P2; }
        if (arm.isDefault) {
            oss << "[&]{return true;}, ";
        } else {
            oss << "[&]{return ";
            for (size_t j = 0; j < arm.patterns.size(); j++) {
                if (j > 0) oss << " || ";
                oss << node.discriminant << " == " << arm.patterns[j];
            }
            oss << ";}, ";
        }

		// Body lambda: [&]{ return <body_expression>; }
		oss << "[&]{return " << GenerateExpression(arm.body) << ";}, ";

		// Fallthrough flag
		oss << (arm.fallthrough ? "true" : "false");
	}

	oss << ")";
	return oss.str();
}

std::string SohTranspiler::GenerateExpression(const rls::ast::Expr::Variant& node) const {
	return std::visit([&](const auto& node) {
		return GenerateExpression(node);
	}, node);
}

std::string SohTranspiler::GenerateExpression(const rls::ast::ExprPtr& expr) const {
	return GenerateExpression(expr->node);
}

}