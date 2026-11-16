#include "soh.h"
#include "cpp_type.h"
#include "soh/enum_mappings.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace rls::transpilers::soh {

namespace {

std::string enumCppType(std::string_view enumName) {
    if (const auto* mapping = findHostEnumMapping(enumName)) {
        return std::string(mapping->cppType);
    }
    return std::string(enumName);
}

bool isEnumLikeType(rls::ast::Type type) {
    return type == rls::ast::Type::Enum;
}

std::string qualifyEnumValue(std::string_view enumName, std::string_view valueName) {
    if (const auto* mapping = findHostEnumMapping(enumName)) {
        if (!mapping->cppValueNamespace.empty()) {
            return std::string(mapping->cppValueNamespace) + "::" + std::string(valueName);
        }
        return std::string(valueName);
    }
    return std::string(enumName) + "::" + std::string(valueName);
}

struct MatchArmCode {
    std::string condition;
    std::string body;
    bool fallthrough;
};

std::string matchLocalName(std::string name, std::string_view armCode) {
    while (armCode.find(name) != std::string_view::npos) {
        name += "_";
    }
    return name;
}

std::string indentMatchBody(std::string code) {
    size_t newline = code.find('\n');
    while (newline != std::string::npos) {
        code.insert(newline + 1, "        ");
        newline = code.find('\n', newline + 9);
    }
    return code;
}

} // namespace

std::string SohTranspiler::GenerateExpression(const rls::ast::BoolLiteral& node) const {
	return node.value ? "true" : "false";
}

std::string SohTranspiler::GenerateExpression(const rls::ast::IntLiteral& node) const {
	return std::to_string(node.value);
}

std::string SohTranspiler::GenerateExpression(const rls::ast::StringLiteral& node) const {
    std::string result{"\""};
    for (char character : node.value) {
        if (character == '\\' || character == '\"') {
            result += '\\';
        }
        result += character;
    }
    return result + "\"";
}

std::string SohTranspiler::GenerateExpression(const rls::ast::Identifier& node) const {
    if (node.kind == rls::ast::IdentifierKind::EnumValue) {
        if (auto enumName = project.getEnumType(&node); enumName.has_value()) {
            return qualifyEnumValue(*enumName, node.name.text);
        }

        return node.name.text;
    } else if (node.kind == rls::ast::IdentifierKind::Parameter) {
        return node.name.text;
    } else if (node.kind == rls::ast::IdentifierKind::DeclaredValue) {
        if (project.RegionDecls.contains(node.name.text)) {
            return qualifyEnumValue("Region", node.name.text);
        }
        if (project.EventDecls.contains(node.name.text)) {
            return qualifyEnumValue("Event", node.name.text);
        }
        if (project.LocationDecls.contains(node.name.text)) {
            return qualifyEnumValue("Location", node.name.text);
        }
        return "";
    } else if (node.kind == rls::ast::IdentifierKind::FunctionRef) {
        return node.name.text;
    } else {
        // Unresolved identifiers should have been blocked earlier in sema; emit empty as a defensive fallback.
        return "";
    }
}

// Returns the C++ operator precedence for an expression node.
// Lower values bind tighter. Non-compound nodes return 0 (tightest).
int SohTranspiler::GetCppPrecedence(const rls::ast::ExprPtr& expr) const {
    return GetCppPrecedence(expr.get());
}

int SohTranspiler::GetCppPrecedence(const rls::ast::Expr* expr) const {
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
        return "!" + GenerateBoolChildExpression(node.operand, 3);
	default:
		return "";
	}
}

std::string SohTranspiler::GenerateExpression(const rls::ast::BinaryExpr& node) const {
    auto generateIntegerCompatibleOperand = [&](const rls::ast::ExprPtr& operand,
                                                 int precedence,
                                                 bool isRightChild = false) {
        auto code = GenerateChildExpression(operand, precedence, isRightChild);
        if (project.getType(operand.get()) == rls::ast::Type::Enum) {
            return "static_cast<int>(" + code + ")";
        }
        return code;
    };

    const auto leftType = project.getType(node.left.get());
    const auto rightType = project.getType(node.right.get());
    const bool isMixedEnumInt = (leftType == rls::ast::Type::Enum && rightType == rls::ast::Type::Int)
        || (leftType == rls::ast::Type::Int && rightType == rls::ast::Type::Enum);

    switch (node.op) {
    case rls::ast::BinaryOp::And:
        return GenerateBoolChildExpression(node.left, 14) + " && "
            + GenerateBoolChildExpression(node.right, 14, true);
    case rls::ast::BinaryOp::Or:
        return GenerateBoolChildExpression(node.left, 15) + " || "
            + GenerateBoolChildExpression(node.right, 15, true);
    case rls::ast::BinaryOp::Eq:
        if (isMixedEnumInt) {
            return generateIntegerCompatibleOperand(node.left, 10) + " == "
                + generateIntegerCompatibleOperand(node.right, 10, true);
        }
        return GenerateChildExpression(node.left, 10) + " == " + GenerateChildExpression(node.right, 10, true);
    case rls::ast::BinaryOp::NotEq:
        if (isMixedEnumInt) {
            return generateIntegerCompatibleOperand(node.left, 10) + " != "
                + generateIntegerCompatibleOperand(node.right, 10, true);
        }
        return GenerateChildExpression(node.left, 10) + " != " + GenerateChildExpression(node.right, 10, true);
    case rls::ast::BinaryOp::Lt:
        return generateIntegerCompatibleOperand(node.left, 9) + " < "
            + generateIntegerCompatibleOperand(node.right, 9, true);
    case rls::ast::BinaryOp::LtEq:
        return generateIntegerCompatibleOperand(node.left, 9) + " <= "
            + generateIntegerCompatibleOperand(node.right, 9, true);
    case rls::ast::BinaryOp::Gt:
        return generateIntegerCompatibleOperand(node.left, 9) + " > "
            + generateIntegerCompatibleOperand(node.right, 9, true);
    case rls::ast::BinaryOp::GtEq:
        return generateIntegerCompatibleOperand(node.left, 9) + " >= "
            + generateIntegerCompatibleOperand(node.right, 9, true);
    case rls::ast::BinaryOp::Add:
        return generateIntegerCompatibleOperand(node.left, 6) + " + "
            + generateIntegerCompatibleOperand(node.right, 6, true);
    case rls::ast::BinaryOp::Sub:
        return generateIntegerCompatibleOperand(node.left, 6) + " - "
            + generateIntegerCompatibleOperand(node.right, 6, true);
    case rls::ast::BinaryOp::Mul:
        return generateIntegerCompatibleOperand(node.left, 5) + " * "
            + generateIntegerCompatibleOperand(node.right, 5, true);
    case rls::ast::BinaryOp::Div:
        return generateIntegerCompatibleOperand(node.left, 5) + " / "
            + generateIntegerCompatibleOperand(node.right, 5, true);
    default:
        return "";
    }
}

std::string SohTranspiler::GenerateExpression(const rls::ast::TernaryExpr& node) const {
    const auto thenType = project.getType(node.thenBranch.get());
    const auto elseType = project.getType(node.elseBranch.get());
    const bool hasMixedBoolIntBranches = thenType.has_value() && elseType.has_value()
        && ((thenType.value() == rls::ast::Type::Bool && elseType.value() == rls::ast::Type::Int)
            || (thenType.value() == rls::ast::Type::Int && elseType.value() == rls::ast::Type::Bool));

    return GenerateBoolChildExpression(node.condition, 15) + " ? " +
           (hasMixedBoolIntBranches ? GenerateBoolExpression(node.thenBranch) : GenerateExpression(node.thenBranch)) + " : " +
           (hasMixedBoolIntBranches ? GenerateBoolExpression(node.elseBranch) : GenerateExpression(node.elseBranch));
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

std::optional<std::string> SohTranspiler::ResolveCallParamEnumCppType(
    const rls::ast::CallExpr& node,
    size_t index) const
{
    if (auto externIt = project.ExternDefineDecls.find(node.callee.text);
        externIt != project.ExternDefineDecls.end() && index < externIt->second->params.size()) {
        auto enumType = project.getEnumType(&externIt->second->params[index]);
        if (enumType.has_value()) {
            return enumCppType(*enumType);
        }
        return std::nullopt;
    }

    if (auto defineIt = project.DefineDecls.find(node.callee.text);
        defineIt != project.DefineDecls.end() && index < defineIt->second->params.size()) {
        auto enumType = project.getEnumType(&defineIt->second->params[index]);
        if (enumType.has_value()) {
            return enumCppType(*enumType);
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::string SohTranspiler::GenerateCallArgument(
    const rls::ast::Expr* argExpr,
    std::optional<rls::ast::Type> paramType,
    std::optional<std::string> paramEnumCppType) const
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
        return GenerateExpression(argExpr);
    }

    if (emitConditionThunk) {
        return "[]{return " + GenerateBoolExpression(argExpr) + ";}";
    }

    const auto argCode = GenerateExpression(argExpr);
    if (!paramType.has_value() || !argType.has_value()) {
        return argCode;
    }

    if (paramType.value() == rls::ast::Type::Bool && argType.value() == rls::ast::Type::Int) {
        return argCode + " != 0";
    }

    if (paramType.value() == rls::ast::Type::Int && isEnumLikeType(argType.value())) {
        return "static_cast<int>(" + argCode + ")";
    }

    if (isEnumLikeType(paramType.value()) && argType.value() == rls::ast::Type::Int) {
        if (paramEnumCppType.has_value()) {
            return "static_cast<" + *paramEnumCppType + ">(" + argCode + ")";
        }
    }

    return argCode;
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
        auto paramEnumCppType = ResolveCallParamEnumCppType(node, i);
        oss << GenerateCallArgument(resolved[i], paramType, paramEnumCppType);
    }
    oss << ")";
    return oss.str();
}

std::string SohTranspiler::GenerateExpression(const rls::ast::InvokeExpr& node) const {
    return GenerateExpression(node.callee) + "()";
}

std::string SohTranspiler::GenerateExpression(const rls::ast::MemberExpr& node) const {
    return qualifyEnumValue(node.object.text, node.member.text);
}

std::string SohTranspiler::GenerateExpression(const rls::ast::HereRef& node) const {
    return "RandomizerRegion::" + node.resolvedRegion.text;
}

std::string SohTranspiler::GenerateExpression(
    const rls::ast::MatchExpr& node, const rls::ast::Expr& expr) const
{
    const auto resultType = project.getType(&expr);
    const auto cppType = nodeType(project, &expr);
    if (!resultType || cppType == "missing_type" || cppType == "unsupported_type"
        || (*resultType == rls::ast::Type::Enum && !project.getEnumType(&expr))) {
        throw std::logic_error("SoH match lowering requires a supported resolved result type");
    }

    std::vector<MatchArmCode> arms;
    std::string armCode;
    for (size_t index = 0; index < node.arms.size(); ++index) {
        const auto& arm = node.arms[index];
        std::ostringstream condition;
        if (arm.isDefault) {
            condition << "true";
        } else {
            for (size_t pattern = 0; pattern < arm.patterns.size(); ++pattern) {
                if (pattern != 0) condition << " || ";
                condition << GenerateExpression(node.discriminant) << " == "
                          << GenerateExpression(arm.patterns[pattern]);
            }
        }
        auto body = *resultType == rls::ast::Type::Bool
            ? GenerateBoolExpression(arm.body) : GenerateExpression(arm.body);
        armCode += condition.str() + "\n" + body + "\n";
        arms.push_back({condition.str(), std::move(body),
            arm.fallthrough && index + 1 < node.arms.size()});
    }

    const bool hasContinuation = std::ranges::any_of(arms, [](const auto& arm) {
        return arm.fallthrough;
    });
    const auto activeName = matchLocalName("rls_match_active", armCode);
    const auto resultName = matchLocalName("rls_match_result", armCode);
    std::ostringstream output;
    output << "([&]() -> " << cppType << " {\n";
    if (hasContinuation) {
        output << "    bool " << activeName << " = false;\n";
    }
    for (const auto& arm : arms) {
        output << "    if (";
        if (hasContinuation) output << activeName << " || ";
        output << arm.condition << ") {\n";
        const auto body = indentMatchBody(arm.body);
        if (arm.fallthrough) {
            output << "        const " << cppType << " " << resultName << " = " << body << ";\n"
                   << "        if (static_cast<bool>(" << resultName << ")) return " << resultName << ";\n"
                   << "        " << activeName << " = true;\n";
        } else {
            output << "        return " << body << ";\n";
        }
        output << "    }\n";
    }
    output << "    return " << cppType << "{};\n}())";
    return output.str();
}

std::string SohTranspiler::GenerateExpression(const rls::ast::ListExpr& node) const {
    std::ostringstream oss;
    oss << "{";
    for (size_t index = 0; index < node.elements.size(); ++index) {
        if (index > 0) {
            oss << ", ";
        }
        oss << GenerateExpression(node.elements[index]);
    }
    oss << "}";
    return oss.str();
}

std::string SohTranspiler::GenerateExpression(const rls::ast::Expr* expr) const {
	return std::visit([&](const auto& node) {
        if constexpr (std::is_same_v<std::decay_t<decltype(node)>, rls::ast::MatchExpr>) {
            return GenerateExpression(node, *expr);
        } else {
            return GenerateExpression(node);
        }
	}, expr->node);
}

std::string SohTranspiler::GenerateExpression(const rls::ast::ExprPtr& expr) const {
	return GenerateExpression(expr.get());
}

std::string SohTranspiler::GenerateBoolExpression(const rls::ast::ExprPtr& expr) const {
    return GenerateBoolExpression(expr.get());
}

std::string SohTranspiler::GenerateBoolExpression(const rls::ast::Expr* expr) const {
    auto result = GenerateExpression(expr);
    if (project.getType(expr) == rls::ast::Type::Int) {
        if (GetCppPrecedence(expr) > 10) {
            result = "(" + result + ")";
        }
        return result + " != 0";
    }
    return result;
}

std::string SohTranspiler::GenerateBoolChildExpression(
    const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild) const
{
    auto result = GenerateBoolExpression(expr);
    const int childPrec = project.getType(expr.get()) == rls::ast::Type::Int
        ? 10 : GetCppPrecedence(expr);
    if (childPrec > parentPrec || (isRightChild && childPrec == parentPrec)) {
        return "(" + result + ")";
    }
    return result;
}

}