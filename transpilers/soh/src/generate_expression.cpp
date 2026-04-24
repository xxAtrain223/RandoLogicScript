#include "soh.h"

#include <format>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace {

struct EnemyHelperParam {
    std::string name;
    std::optional<std::string> defaultValue;
};

struct EnemyHelperMetadata {
    std::string emittedName;
    std::vector<EnemyHelperParam> params;
};

const EnemyHelperMetadata* getEnemyHelperMetadata(const std::string& name) {
    static const std::unordered_map<std::string, EnemyHelperMetadata> helpers = {
        { "can_kill", { "CanKillEnemy", {
            { "enemy", std::nullopt },
            { "distance", "ED_CLOSE" },
            { "wallOrFloor", "true" },
            { "quantity", "1" },
            { "timer", "false" },
            { "inWater", "false" },
        } } },
        { "can_pass", { "CanPassEnemy", {
            { "enemy", std::nullopt },
            { "distance", "ED_CLOSE" },
            { "wallOrFloor", "true" },
        } } },
        { "can_get_drop", { "CanGetEnemyDrop", {
            { "enemy", std::nullopt },
            { "distance", "ED_CLOSE" },
            { "aboveLink", "false" },
        } } },
        { "can_avoid", { "CanAvoidEnemy", {
            { "enemy", std::nullopt },
            { "grounded", "false" },
            { "quantity", "1" },
        } } },
    };

    if (auto it = helpers.find(name); it != helpers.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace

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
    // Resolves call arguments into parameter slots using sema-compatible
    // named/positional binding order. Unknown/duplicate args should have been
    // reported by sema already, so this function only maps what it can.
    auto resolveByNames = [&](const std::vector<std::string>& paramNames) {
        std::vector<std::optional<std::string>> resolved(paramNames.size());
        std::vector<bool> bound(paramNames.size(), false);
        std::unordered_map<std::string, size_t> indexByName;
        for (size_t i = 0; i < paramNames.size(); ++i) {
            indexByName.emplace(paramNames[i], i);
        }

        size_t nextPositional = 0;
        for (const auto& arg : node.args) {
            if (arg.name) {
                auto it = indexByName.find(*arg.name);
                if (it == indexByName.end()) {
                    continue;
                }
                size_t i = it->second;
                if (bound[i]) {
                    continue;
                }
                resolved[i] = GenerateExpression(arg.value);
                bound[i] = true;
                continue;
            }

            while (nextPositional < paramNames.size() && bound[nextPositional]) {
                ++nextPositional;
            }
            if (nextPositional >= paramNames.size()) {
                continue;
            }

            resolved[nextPositional] = GenerateExpression(arg.value);
            bound[nextPositional] = true;
            ++nextPositional;
        }

        return resolved;
    };

    // Emits a direct function-style call from a final ordered argument list.
    auto emitCall = [](const std::string& functionName, const std::vector<std::string>& args) {
        std::ostringstream oss;
        oss << functionName << "(";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << args[i];
        }
        oss << ")";
        return oss.str();
    };

    // Enemy built-ins keep a dedicated C++ helper mapping.
    if (const auto* enemy = getEnemyHelperMetadata(node.function)) {
        std::vector<std::string> paramNames;
        paramNames.reserve(enemy->params.size());
        for (const auto& p : enemy->params) {
            paramNames.push_back(p.name);
        }

        auto resolved = resolveByNames(paramNames);
        int lastProvided = -1;
        for (int i = static_cast<int>(resolved.size()) - 1; i >= 0; --i) {
            if (resolved[i].has_value()) {
                lastProvided = i;
                break;
            }
        }

        std::vector<std::string> emittedArgs;
        for (int i = 0; i <= lastProvided; ++i) {
            if (resolved[i]) {
                emittedArgs.push_back(*resolved[i]);
                continue;
            }
            if (enemy->params[i].defaultValue) {
                emittedArgs.push_back(*enemy->params[i].defaultValue);
            }
        }

        return emitCall(enemy->emittedName, emittedArgs);
    }

    // Extern-defined host calls emit by declared function name with resolved
    // argument order and declaration defaults.
    if (auto it = project.ExternDefineDecls.find(node.function); it != project.ExternDefineDecls.end()) {
        const auto& ext = *it->second;
        std::vector<std::string> paramNames;
        paramNames.reserve(ext.params.size());
        for (const auto& p : ext.params) {
            paramNames.push_back(p.name);
        }

        auto resolved = resolveByNames(paramNames);
        std::vector<std::string> emittedArgs;
        emittedArgs.reserve(ext.params.size());
        for (size_t i = 0; i < ext.params.size(); ++i) {
            if (resolved[i]) {
                emittedArgs.push_back(*resolved[i]);
                continue;
            }
            if (ext.params[i].defaultValue) {
                emittedArgs.push_back(GenerateExpression(ext.params[i].defaultValue));
            }
        }

        return emitCall(node.function, emittedArgs);
    }

    // User-defined calls use the same binding/default expansion rules so call
    // emission is canonical across define + extern define.
    if (auto it = project.DefineDecls.find(node.function); it != project.DefineDecls.end()) {
        const auto& def = *it->second;
        std::vector<std::string> paramNames;
        paramNames.reserve(def.params.size());
        for (const auto& p : def.params) {
            paramNames.push_back(p.name);
        }

        auto resolved = resolveByNames(paramNames);
        std::vector<std::string> emittedArgs;
        emittedArgs.reserve(def.params.size());
        for (size_t i = 0; i < def.params.size(); ++i) {
            if (resolved[i]) {
                emittedArgs.push_back(*resolved[i]);
                continue;
            }
            if (def.params[i].defaultValue) {
                emittedArgs.push_back(GenerateExpression(def.params[i].defaultValue));
            }
        }

        return emitCall(node.function, emittedArgs);
    }

    // Unknown calls are blocked earlier in sema; emit empty as a defensive
    // fallback so generation does not invent passthrough call forms.
    return "";
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

std::string SohTranspiler::GenerateExpression(const rls::ast::Expr::Variant& node) const {
	return std::visit([&](const auto& node) {
		return GenerateExpression(node);
	}, node);
}

std::string SohTranspiler::GenerateExpression(const rls::ast::ExprPtr& expr) const {
	return GenerateExpression(expr->node);
}

}