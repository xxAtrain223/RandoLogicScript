#include "ap_transpiler.h"

#include <sstream>

namespace rls::transpilers::ap {

std::string ApTranspiler::WrapOptionFilter(const std::string& optionFilterArgs) const {
	return "True_(options=[OptionFilter(" + optionFilterArgs + ")])";
}

// True if this binary expression is a `setting(KEY) == VALUE` / `!= VALUE` comparison.
bool ApTranspiler::IsSettingComparison(const rls::ast::BinaryExpr& node) const {
	// Only == and != comparisons
	if (node.op != rls::ast::BinaryOp::Eq && node.op != rls::ast::BinaryOp::NotEq) {
		return false;
	}

	// Left side must be a setting() call and right must be an identifier (the enum value)
	auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
	auto* rightId = std::get_if<rls::ast::Identifier>(&node.right->node);
	if (!leftCall || !rightId || leftCall->callee.text != "setting") {
		return false;
	}

	// The setting key argument (RSK_*) must resolve to an identifier
	auto resolvedPtr = project.getResolvedCallArgs(leftCall);
	if (!resolvedPtr || resolvedPtr->empty()) {
		return false;
	}
	return std::get_if<rls::ast::Identifier>(&resolvedPtr->front()->node) != nullptr;
}

// Try to generate an OptionFilter expression for setting comparisons.
// Returns empty string if not a setting comparison; caller should use standard binary expression.
std::string ApTranspiler::TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const {
	if (!IsSettingComparison(node)) {
		return "";
	}

	auto* rightId = std::get_if<rls::ast::Identifier>(&node.right->node);
	auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
	auto* settingKeyId = std::get_if<rls::ast::Identifier>(&project.getResolvedCallArgs(leftCall)->front()->node);

	std::ostringstream args;
	args << settingKeyId->name.text << ", " << GenerateExpression(*rightId);
	if (node.op == rls::ast::BinaryOp::NotEq) {
		args << ", \"ne\"";
	}

	return WrapOptionFilter(args.str());
}

std::string ApTranspiler::GenerateExpression(const rls::ast::BoolLiteral& node) const {
	return node.value ? "True_()" : "False_()";
}

std::string ApTranspiler::GenerateExpression(const rls::ast::IntLiteral& node) const {
	return std::to_string(node.value);
}

std::string ApTranspiler::GenerateExpression(const rls::ast::Identifier& node) const {
	if (node.kind == rls::ast::IdentifierKind::EnumValue) {
		auto type = project.getType(&node);
		if (!type.has_value()) {
			return node.name.text;
		}
		return renderEnumValue(type.value(), node.name.text);
	} else if (node.kind == rls::ast::IdentifierKind::Parameter) {
		return node.name.text;
	} else if (node.kind == rls::ast::IdentifierKind::FunctionRef) {
		// Bare reference to a function used as a callable value: emit the name.
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
int ApTranspiler::GetPythonPrecedence(const rls::ast::ExprPtr& expr) const {
	if (auto* bin = std::get_if<rls::ast::BinaryExpr>(&expr->node)) {
		// Setting comparisons are emitted as an atomic OptionFilter rule call, not a
		// Python comparison, so they bind as tightly as a call (no parentheses needed).
		if (IsSettingComparison(*bin)) {
			return 0;
		}
		// A game-specific rewrite (e.g. wallet capacity, triforce hunt) collapses the
		// comparison to an atomic call, so it also binds as tightly as a call.
		if (renderBinarySpecialCase(*bin)) {
			return 0;
		}
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
std::string ApTranspiler::GenerateChildExpression(
	const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild) const
{
	auto result = GenerateExpression(expr);
	int childPrec = GetPythonPrecedence(expr);
	if (childPrec > parentPrec || (isRightChild && childPrec == parentPrec)) {
		return "(" + result + ")";
	}
	return result;
}

std::string ApTranspiler::GenerateExpression(const rls::ast::UnaryExpr& node) const {
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
					return WrapOptionFilter(settingKeyId->name.text + ", False");
				}
			}
		}
		return GenerateExpression(node.operand);
	}
	default:
		return "";
	}
}

std::string ApTranspiler::GenerateExpression(const rls::ast::BinaryExpr& node) const {
	// Setting comparisons become atomic OptionFilter rules.
	std::string optionFilter = TryGenerateOptionFilter(node);
	if (!optionFilter.empty()) {
		return optionFilter;
	}

	// Game-specific binary rewrites (e.g. price <= wallet capacity, triforce hunt).
	if (auto special = renderBinarySpecialCase(node)) {
		return *special;
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
std::string ApTranspiler::GenerateExpression(const rls::ast::TernaryExpr& node) const {
	return GenerateExpression(node.thenBranch) + " if " +
		   GenerateChildExpression(node.condition, 15) + " else " +
		   GenerateExpression(node.elseBranch);
}

std::string ApTranspiler::GenerateExpression(const rls::ast::CallExpr& node) const {
	auto resolvedPtr = project.getResolvedCallArgs(&node);
	if (resolvedPtr == nullptr) {
		// Unknown calls or calls with semantic errors are blocked earlier in sema;
		// emit empty as a defensive fallback so generation does not invent call forms.
		return "";
	}
	const auto& resolved = *resolvedPtr;

	// setting(KEY) is a truthiness check, emitted as an OptionFilter rule (AP-generic).
	if (node.callee.text == "setting") {
		if (auto* id = std::get_if<rls::ast::Identifier>(&resolved[0]->node)) {
			return WrapOptionFilter(id->name.text + std::string(", True"));
		}
		return "";
	}

	// Game-specific host-call rewrites (has, flag, trick, ...).
	if (auto hostCall = renderHostCall(node)) {
		return *hostCall;
	}

	// Default: a regular function call, optionally threading the rule-context
	// receiver (e.g. SoH's `bundle`) as the implicit first argument.
	std::ostringstream oss;
	oss << node.callee.text << "(";
	const std::string receiver = ruleContextParam();
	bool needComma = false;
	if (!receiver.empty()) {
		oss << receiver;
		needComma = true;
	}
	for (size_t i = 0; i < resolved.size(); ++i) {
		if (needComma) {
			oss << ", ";
		}
		needComma = true;

		oss << GenerateCallArgument(resolved[i], ResolveCallParamType(node, i));
	}
	oss << ")";
	return oss.str();
}

std::optional<rls::ast::Type> ApTranspiler::ResolveCallParamType(
	const rls::ast::CallExpr& node, size_t index) const {
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

std::string ApTranspiler::GenerateCallArgument(
	const rls::ast::Expr* argExpr, std::optional<rls::ast::Type> paramType) const {
	const bool paramIsCondition = paramType == rls::ast::Type::Condition;
	const bool argIsCondition = project.getType(argExpr) == rls::ast::Type::Condition;

	// An argument already of Condition type is a callable value; pass it through unchanged.
	if (paramIsCondition && argIsCondition) {
		return GenerateExpression(argExpr->node);
	}

	// A non-Condition expression bound to a Condition parameter is wrapped in a thunk so the
	// RuleBuilder evaluates it lazily: `(lambda <ctx>: <expr>)`.
	if (paramIsCondition) {
		return "(lambda " + ruleContextParam() + ": " + GenerateExpression(argExpr->node) + ")";
	}

	// Function parameters use Python's True/False, not the True_()/False_() rule literals.
	if (auto* lit = std::get_if<rls::ast::BoolLiteral>(&argExpr->node)) {
		return lit->value ? "True" : "False";
	}

	return GenerateExpression(argExpr->node);
}

std::string ApTranspiler::GenerateExpression(const rls::ast::InvokeExpr& node) const {
	// Invoke a callable-valued result. A Condition is a rule callback typed
	// `Callable[[bundle], Rule]`, so it is invoked with the rule-context receiver
	// (e.g. SoH's `bundle`): `<callee>(bundle)`. This mirrors the thunk form produced
	// for Condition arguments, so the two agree on arity.
	return GenerateExpression(node.callee) + "(" + ruleContextParam() + ")";
}

std::string ApTranspiler::GenerateExpression(const rls::ast::SharedBlock& node) const {
	return renderSharedBlock(node);
}

std::string ApTranspiler::GenerateExpression(const rls::ast::MatchExpr& node) const {
	std::ostringstream oss;
	oss << "rls_match(";

	for (size_t i = 0; i < node.arms.size(); i++) {
		const auto& arm = node.arms[i];

		if (i > 0) oss << ", ";

		// Condition - lambda discriminant: discriminant == P1 or discriminant == P2
		if (arm.isDefault) {
			oss << "(lambda: True), ";
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

std::string ApTranspiler::GenerateExpression(const rls::ast::Expr::Variant& node) const {
	return std::visit([&](const auto& node) {
		return ApTranspiler::GenerateExpression(node);
	}, node);
}

std::string ApTranspiler::GenerateExpression(const rls::ast::ExprPtr& expr) const {
	return GenerateExpression(expr->node);
}

} // namespace rls::transpilers::ap
