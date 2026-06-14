#include "soh_ap.h"

#include <format>
#include <sstream>
#include <optional>
#include <unordered_map>

namespace rls::transpilers::soh_ap {

// Try to generate an OptionFilter expression for setting comparisons.
// Returns empty string if not a setting comparison; caller should use standard binary expression.
std::string SohApTranspiler::TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const {
	// Only handle == and != comparisons
	if (node.op != rls::ast::BinaryOp::Eq && node.op != rls::ast::BinaryOp::NotEq) {
		return "";
	}

	// Check if left side is a setting() call and right is an identifier (the enum value)
	auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
	auto* rightId = std::get_if<rls::ast::Identifier>(&node.right->node);

	if (!leftCall || !rightId || leftCall->callee.text != "setting") {
		return "";
	}

	// Get the setting key argument (RSK_*)
	auto resolvedPtr = project.getResolvedCallArgs(leftCall);
	if (!resolvedPtr || resolvedPtr->empty()) {
		return "";
	}
	
	auto* settingKeyId = std::get_if<rls::ast::Identifier>(&resolvedPtr->front()->node);
	if (!settingKeyId) {
		return "";
	}

	std::ostringstream result;
	result << "OptionFilter(" << settingKeyId->name.text << ", " << GenerateExpression(*rightId);
	if (node.op == rls::ast::BinaryOp::NotEq) {
		result << ", \"ne\"";
	}
	result << ")";
	
	return result.str();
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::BoolLiteral& node) const {
	return node.value ? "True_()" : "False_()";
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::IntLiteral& node) const {
	return std::to_string(node.value);
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::Identifier& node) const {
    if (node.kind == rls::ast::IdentifierKind::EnumValue) {
        auto type = project.getType(&node);
        if (!type.has_value()) {
            return node.name.text;
        }
        switch (type.value()) {
            case rls::ast::Type::Item: return "Items." + node.name.text;
            case rls::ast::Type::Enemy: return "Enemies." + node.name.text;
            case rls::ast::Type::Distance: return "EnemyDistance." + node.name.text;
            case rls::ast::Type::Trick: return "Tricks." + node.name.text;
            case rls::ast::Type::Logic: return "Events." + node.name.text;
            case rls::ast::Type::Setting: return "RandomizerSettingKey." + node.name.text;
            case rls::ast::Type::Region: return "Regions." + node.name.text;
            case rls::ast::Type::Check: return "Locations." + node.name.text;
            case rls::ast::Type::Trial: return "TrialKey." + node.name.text;
            default: return node.name.text;
        }
    } else if (node.kind == rls::ast::IdentifierKind::Parameter) {
        return node.name.text;
    } else {
        // Unresolved identifiers should have been blocked earlier in sema; emit empty as a defensive fallback.
        return "";
    }
}

// Returns the RuleBuilder operator precedence for an expression node.
// Precedence adjusted for RuleBuilder bitwise operators (&, |, ~) as used in Archipelago:
// - Arithmetic (*, /): 6
// - Arithmetic (+, -): 7
// - Bitwise AND (&): 9
// - Bitwise OR (|): 11
// - Comparisons (==, !=, <, etc.): 12
// - Ternary: 16
// Unary bitwise NOT (~) and function calls have precedence 3 (very tight).
int SohApTranspiler::GetPythonPrecedence(const rls::ast::ExprPtr& expr) const {
	if (auto* bin = std::get_if<rls::ast::BinaryExpr>(&expr->node)) {
		switch (bin->op) {
		case rls::ast::BinaryOp::Mul:
		case rls::ast::BinaryOp::Div:
            return 6;
		case rls::ast::BinaryOp::Add:
		case rls::ast::BinaryOp::Sub:
            return 7;
		case rls::ast::BinaryOp::And:
            return 9;  // Bitwise AND (&)
		case rls::ast::BinaryOp::Lt:
		case rls::ast::BinaryOp::LtEq:
		case rls::ast::BinaryOp::Gt:
		case rls::ast::BinaryOp::GtEq:
		case rls::ast::BinaryOp::Eq:
		case rls::ast::BinaryOp::NotEq:
            return 12;
		case rls::ast::BinaryOp::Or:
            return 11;  // Bitwise OR (|)
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
	case rls::ast::UnaryOp::Not: {
		// RuleBuilder doesn't support negation of rules. Negation on settings is handled
		// via OptionFilter with a false value for direct setting() calls.
		if (auto* call = std::get_if<rls::ast::CallExpr>(&node.operand->node);
			call && call->callee.text == "setting") {
			auto resolvedPtr = project.getResolvedCallArgs(call);
			if (resolvedPtr && !resolvedPtr->empty()) {
				auto* settingKeyId = std::get_if<rls::ast::Identifier>(&resolvedPtr->front()->node);
				if (settingKeyId) {
					return std::string("OptionFilter(") + settingKeyId->name.text + ", False)";
				}
			}
		}
		return GenerateExpression(node.operand);
	}
	default:
		return "";
	}
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::BinaryExpr& node) const {
    // Try to generate OptionFilter for setting comparisons
    std::string optionFilter = TryGenerateOptionFilter(node);
    if (!optionFilter.empty()) {
        return optionFilter;
    }

    // Special case: check_price(...) <= wallet_capacity(...) should only output check_price(...)
    if (node.op == rls::ast::BinaryOp::LtEq) {
        auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
        auto* rightCall = std::get_if<rls::ast::CallExpr>(&node.right->node);
        
        if (leftCall && rightCall && 
            leftCall->callee.text == "check_price" && 
            rightCall->callee.text == "wallet_capacity") {
            return GenerateExpression(node.left);
        }
    }

    // Special case: collected_triforce_pieces(...) >= required_triforce_pieces(...) should output CanWinTriforceHunt()
    if (node.op == rls::ast::BinaryOp::GtEq) {
        auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
        auto* rightCall = std::get_if<rls::ast::CallExpr>(&node.right->node);
        
        if (leftCall && rightCall && 
            leftCall->callee.text == "collected_triforce_pieces" && 
            rightCall->callee.text == "required_triforce_pieces") {
            return "CanWinTriforceHunt()";
        }
    }

    switch (node.op) {
    case rls::ast::BinaryOp::And:
        return GenerateChildExpression(node.left, 9) + " & " + GenerateChildExpression(node.right, 9, true);
    case rls::ast::BinaryOp::Or:
        return GenerateChildExpression(node.left, 11) + " | " + GenerateChildExpression(node.right, 11, true);
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

std::string SohApTranspiler::GenerateExpression(const rls::ast::CallExpr& node) const {
    auto resolvedPtr = project.getResolvedCallArgs(&node);
    if (resolvedPtr == nullptr) {
        // Unknown calls or calls with semantic errors are blocked earlier in sema;
        // emit empty as a defensive fallback so generation does not invent call forms.
        return "";
    }
    const auto& resolved = *resolvedPtr;

    std::ostringstream oss;

    // Handle settings differently
    if (node.callee.text == "setting") {
        if (auto* id = std::get_if<rls::ast::Identifier>(&resolved[0]->node)) {
            oss << "OptionFilter(" << id->name.text << ", True)";
        }
    } else if (node.callee.text == "has" || node.callee.text == "flag") {
        oss << "has_item(bundle, " << GenerateExpression(resolved[0]->node) << ")";
    } else if (node.callee.text == "trick") {
        oss << "can_do_trick(bundle, " << GenerateExpression(resolved[0]->node) << ")";
    } else if (node.callee.text == "check_price") {
        oss << "can_afford_slot(" << GenerateExpression(resolved[0]->node) << ")";
    } else {
        oss << node.callee.text << "(bundle";
        for (size_t i = 0; i < resolved.size(); ++i) {
            // if (i > 0) {
            //     oss << ", ";
            // }
            oss << ", ";

            // Special case functions parameters use Python's True/False instead of True_()/False_(). 
            if (auto* id = std::get_if<rls::ast::BoolLiteral>(&resolved[i]->node)) {
                if (id->value) {
                    oss << "True";
                } else {
                    oss << "False";
                }
                continue;
            }
            oss << GenerateExpression(resolved[i]->node);
        }
        oss << ")";
    }
    return oss.str();
}

// Going to have to fiddle with this. 
// I think as long as we define the SharedSpirit functions and the SharedSpiritData map this could be done 
std::string SohApTranspiler::GenerateExpression(const rls::ast::SharedBlock& node) const {
    std::ostringstream oss;

    const auto& firstBranch = node.branches[0];
    oss << "spirit_shared(" << firstBranch.region->text << ", "
        << "(lambda: " << GenerateExpression(firstBranch.condition) << "), "
        << (node.anyAge ? "True" : "False");

    for (int i = 1; i < node.branches.size(); i++) {
        oss << ", " << node.branches[i].region->text << ", "
            << "(lambda:" << GenerateExpression(node.branches[i].condition) << ")";
    }

    oss << ")";

	return oss.str();
}

// This one I'm not quite sure how it works. Seems like Ship calls this recursively for the current region
std::string SohApTranspiler::GenerateExpression(const rls::ast::AnyAgeBlock& node) const {
	return "";
    // return "AnyAgeTime((lambda:" + GenerateExpression(node.body) + "))";
}

std::string SohApTranspiler::GenerateExpression(const rls::ast::MatchExpr& node) const {
	std::ostringstream oss;
	oss << "rls_match(";

	for (size_t i = 0; i < node.arms.size(); i++) {
		const auto& arm = node.arms[i];

		if (i > 0) oss << ", ";

        // Condition - lambda discriminant: discriminant == P1 or discriminant == P2
        if (arm.isDefault) {
            oss << "(lambda: true), ";
        } else {
            oss << "(lambda " << GenerateExpression(node.discriminant) << "=" << GenerateExpression(node.discriminant) << ": ";
            for (size_t j = 0; j < arm.patterns.size(); j++) {
                if (j > 0) oss << " or ";
                oss << GenerateExpression(node.discriminant) << " == " << GenerateExpression(arm.patterns[j]);
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
