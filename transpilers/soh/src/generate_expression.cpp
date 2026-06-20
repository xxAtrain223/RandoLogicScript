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
    if (node.kind == rls::ast::IdentifierKind::EnumValue) {
        auto type = project.getType(&node);
        if (!type.has_value()) {
            return node.name.text;
        }
        switch (type.value()) {
            case rls::ast::Type::Item: return "RandomizerGet::" + node.name.text;
            case rls::ast::Type::Enemy: return "RandomizerEnemy::" + node.name.text;
            case rls::ast::Type::Distance: return "EnemyDistance::" + node.name.text;
            case rls::ast::Type::Trick: return "RandomizerTrick::" + node.name.text;
            // We current map RSK_ and RO_ settings to a single enum, but SOH has 1 RSK_ enum about 55 RO_ enums,
            // so we can't map to one type and back out to the correct prefix without losing information.
            // This is a gap we'll have to address. For now, C++ accepts the unqualified name which we'll take advantage of..
            //case rls::ast::Type::Setting: return "RandomizerSettingKey::" + node.name.text;
            case rls::ast::Type::Region: return "RandomizerRegion::" + node.name.text;
            case rls::ast::Type::Check: return "RandomizerCheck::" + node.name.text;
            case rls::ast::Type::Logic: return "LogicVal::" + node.name.text;
            case rls::ast::Type::Scene: return "SceneID::" + node.name.text;
            case rls::ast::Type::Dungeon: return "DungeonKey::" + node.name.text;
            case rls::ast::Type::Area: return "RandomizerArea::" + node.name.text;
            case rls::ast::Type::Trial: return "TrialKey::" + node.name.text;
            case rls::ast::Type::WaterLevel: return "RandoWaterLevel::" + node.name.text;
            default: return node.name.text;
        }
    } else if (node.kind == rls::ast::IdentifierKind::Parameter) {
        return node.name.text;
    } else {
        // Unresolved identifiers should have been blocked earlier in sema; emit empty as a defensive fallback.
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

std::optional<rls::ast::Type> SohTranspiler::ResolveCallParamType(
    const rls::ast::CallExpr& node,
    size_t index) const
{
    if (auto externIt = project.ExternDefineDecls.find(node.callee.text);
        externIt != project.ExternDefineDecls.end() && index < externIt->second->params.size()) {
        return project.getType(&externIt->second->params[index]);
    }

    if (auto defineIt = project.DefineDecls.find(node.callee.text);
        defineIt != project.DefineDecls.end() && index < defineIt->second->params.size()) {
        return project.getType(&defineIt->second->params[index]);
    }

    return std::nullopt;
}

std::string SohTranspiler::GenerateCallArgument(
    const rls::ast::Expr* argExpr,
    std::optional<rls::ast::Type> paramType) const
{
    auto argType = project.getType(argExpr);
    bool passConditionByValue = paramType.has_value()
        && paramType.value() == rls::ast::Type::Condition
        && argType.has_value()
        && argType.value() == rls::ast::Type::Condition;

    bool emitConditionThunk = paramType.has_value()
        && paramType.value() == rls::ast::Type::Condition
        && !passConditionByValue;

    if (passConditionByValue) {
        if (auto id = std::get_if<rls::ast::Identifier>(&argExpr->node);
            id != nullptr && id->kind == rls::ast::IdentifierKind::Parameter) {
            return id->name.text;
        }
        return GenerateExpression(argExpr->node);
    }

    if (emitConditionThunk) {
        return "[]{return " + GenerateExpression(argExpr->node) + ";}";
    }

    return GenerateExpression(argExpr->node);
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
    oss << node.callee.text << "(";
    for (size_t i = 0; i < resolved.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }

        auto paramType = ResolveCallParamType(node, i);
        oss << GenerateCallArgument(resolved[i], paramType);
    }
    oss << ")";
    return oss.str();
}

std::string SohTranspiler::GenerateExpression(const rls::ast::InvokeExpr& node) const {
    return GenerateExpression(node.callee) + "()";
}

std::string SohTranspiler::GenerateExpression(const rls::ast::SharedBlock& node) const {
    std::ostringstream oss;

    const auto& firstBranch = node.branches[0];
        oss << "SpiritShared(" << firstBranch.region->text << ", "
        << "[]{return " << GenerateExpression(firstBranch.condition) << ";}, "
        << (node.anyAge ? "true" : "false");

    for (int i = 1; i < node.branches.size(); i++) {
		oss << ", " << node.branches[i].region->text << ", "
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
                oss << GenerateExpression(node.discriminant) << " == "
                    << GenerateExpression(arm.patterns[j]);
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