#include "generate_expression.h"

#include <format>
#include <sstream>

namespace rls::transpilers::soh {

std::string GenerateExpression(const rls::ast::Expr::Variant& node);

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
    case rls::ast::Keyword::IsVanilla:
        return "logic->IsVanilla()";
    case rls::ast::Keyword::IsMq:
        return "logic->IsMQ()";
    default:
        return "";
    }
}

// Returns the C++ operator precedence for an expression node.
// Lower values bind tighter. Non-compound nodes return 0 (tightest).
static int GetCppPrecedence(const rls::ast::ExprPtr& expr) {
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
static std::string GenerateChildExpression(
	const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false)
{
	auto result = GenerateExpression(expr);
	int childPrec = GetCppPrecedence(expr);
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

static std::string GenerateExpression(const rls::ast::BinaryExpr& node) {
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

static std::string GenerateExpression(const rls::ast::TernaryExpr& node) {
	return GenerateChildExpression(node.condition, 15) + " ? " +
		   GenerateExpression(node.thenBranch) + " : " +
		   GenerateExpression(node.elseBranch);
}

using AT = rls::ast::Type;

struct FunctionRegistryItemParam {
    AT type;
    std::string name;
    std::string defaultValue;
};

struct FunctionRegistryItem {
    std::string output;
    bool isFunction;
    std::vector<FunctionRegistryItemParam> parameters;
};

static const FunctionRegistryItem* GetFunction(const std::string& name) {
    static std::unordered_map<std::string, FunctionRegistryItem> functions = {
        { "has", { "logic->HasItem", true, { { AT::Item, "itemName", ""}}}},
        { "can_use", { "logic->CanUse", true, { { AT::Item, "itemName", "" } } } },
        { "keys", { "logic->SmallKeys", true, { { AT::Scene, "scene", "" }, { AT::Int, "requiredAmount", "" } } } },
        { "flag", { "logic->Get", true, { { AT::Logic, "logicVal", "" } } } },
        { "setting", { "ctx->GetOption", true, { { AT::Logic, "key", "" } } } },
        { "trick", { "ctx->GetTrickOption", true, { { AT::Logic, "key", "" } } } },
        { "hearts", { "logic->Hearts", true, { } } },
        { "effective_health", { "logic->EffectiveHealth", true, { } } },
        { "trial_skipped", { "ctx->GetTrial({0})->IsSkipped()", false, { { AT::Trial, "key", "" } } } },
        { "check_price", { "GetCheckPrice", true, { { AT::Check, "check", "RC_UNKNOWN_CHECK" } } } },
        { "can_plant_bean", { "CanPlantBean", true, { { AT::Region, "region", "" }, { AT::Item, "bean", "" } } } },
        { "triforce_pieces", { "logic->GetSaveContext()->ship.quest.data.randomizer.triforcePiecesCollected", false, { } } },
        { "big_poes", { "logic->BigPoes", false, { } } },
        { "can_kill", { "CanKillEnemy", true, { { AT::Enemy, "enemy", "" }, { AT::Distance, "distance", "ED_CLOSE" }, { AT::Bool, "wallOrFloor", "true" }, { AT::Int, "quantity", "1" }, { AT::Bool, "timer", "false" }, { AT::Bool, "inWater", "false" } } } },
        { "can_pass", { "CanPassEnemy", true, { { AT::Enemy, "enemy", "" }, { AT::Distance, "distance", "ED_CLOSE" }, { AT::Bool, "wallOrFloor", "true" } } } },
        { "can_get_drop", { "CanGetEnemyDrop", true, { { AT::Enemy, "enemy", "" }, { AT::Distance, "distance", "ED_CLOSE" }, { AT::Bool, "aboveLink", "true" } } } },
        { "can_avoid", { "CanAvoidEnemy", true, { { AT::Enemy, "enemy", "" }, { AT::Bool, "grounded", "false" }, { AT::Int, "quantity", "1" } } } }
    };

    if (auto it = functions.find(name); it != functions.end()) {
        return &it->second;
    }

    return nullptr;
}

static std::string GenerateExpression(const rls::ast::CallExpr& node) {
    std::ostringstream oss;

    if (const auto* func = GetFunction(node.function)) {
        // Resolve arguments: map positional and named args to registry positions
        std::vector<std::optional<std::string>> resolvedArgs(func->parameters.size());

        int positionalIndex = 0;
        for (const auto& arg : node.args) {
            if (arg.name.has_value()) {
                for (int j = 0; j < func->parameters.size(); j++) {
                    if (arg.name.value() == func->parameters[j].name) {
                        resolvedArgs[j] = GenerateExpression(arg.value);
                        break;
                    }
                }
            } else {
                if (positionalIndex < static_cast<int>(func->parameters.size())) {
                    resolvedArgs[positionalIndex] = GenerateExpression(arg.value);
                }
                positionalIndex++;
            }
        }

        // Find the last explicitly provided argument
        int lastProvidedIndex = -1;
        for (int i = static_cast<int>(resolvedArgs.size()) - 1; i >= 0; i--) {
            if (resolvedArgs[i].has_value()) {
                lastProvidedIndex = i;
                break;
            }
        }

        // Apply placeholder replacements in the output template
        std::string output = func->output;
        for (int i = 0; i <= lastProvidedIndex; i++) {
            std::string placeholder = "{" + std::to_string(i) + "}";
            std::string replacement = resolvedArgs[i].value_or(func->parameters[i].defaultValue);
            size_t pos = 0;
            while ((pos = output.find(placeholder, pos)) != std::string::npos) {
                output.replace(pos, placeholder.size(), replacement);
                pos += replacement.size();
            }
        }
        oss << output;

        if (func->isFunction) {
            oss << "(";
            for (int i = 0; i <= lastProvidedIndex; i++) {
                if (i > 0) {
                    oss << ", ";
                }
                oss << resolvedArgs[i].value_or(func->parameters[i].defaultValue);
            }
            oss << ")";
        }
    }
    else {
        oss << node.function << "(";
        for (int i = 0; i < node.args.size(); i++) {
            if (i > 0) {
                oss << ", ";
            }
            oss << GenerateExpression(node.args[i].value);
        }
        oss << ")";
    }

    return oss.str();
}

static std::string GenerateExpression(const rls::ast::SharedBlock& node) {
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

static std::string GenerateExpression(const rls::ast::AnyAgeBlock& node) {
	return "AnyAgeTime([]{return " + GenerateExpression(node.body) + ";})";
}

static std::string GenerateExpression(const rls::ast::MatchExpr& node) {
	std::ostringstream oss;
	oss << "rls::match(";

	for (size_t i = 0; i < node.arms.size(); i++) {
		const auto& arm = node.arms[i];

		if (i > 0) oss << ", ";

		// Condition lambda: [&]{ return discriminant == P1 || discriminant == P2; }
		oss << "[&]{return ";
		for (size_t j = 0; j < arm.patterns.size(); j++) {
			if (j > 0) oss << " || ";
			oss << node.discriminant << " == " << arm.patterns[j];
		}
		oss << ";}, ";

		// Body lambda: [&]{ return <body_expression>; }
		oss << "[&]{return " << GenerateExpression(arm.body) << ";}, ";

		// Fallthrough flag
		oss << (arm.fallthrough ? "true" : "false");
	}

	oss << ")";
	return oss.str();
}

static std::string GenerateExpression(const rls::ast::Expr::Variant& node) {
	return std::visit([&](const auto& node) {
		return GenerateExpression(node);
	}, node);
}

std::string GenerateExpression(const rls::ast::ExprPtr& expr) {
	return GenerateExpression(expr->node);
}

}