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
	return renderSettingOptionFilter(node, /*negate=*/false);
}

std::string ApTranspiler::renderSettingOptionFilter(const rls::ast::BinaryExpr& node, bool negate) const {
	auto* rightId = std::get_if<rls::ast::Identifier>(&node.right->node);
	auto* leftCall = std::get_if<rls::ast::CallExpr>(&node.left->node);
	auto* settingKeyId = std::get_if<rls::ast::Identifier>(&project.getResolvedCallArgs(leftCall)->front()->node);

	std::ostringstream args;
	args << settingKeyId->name.text << ", " << GenerateExpression(*rightId);
	// `!=` / `is not` is the "ne" operator; negation flips eq <-> ne.
	bool ne = (node.op == rls::ast::BinaryOp::NotEq);
	if (negate) {
		ne = !ne;
	}
	if (ne) {
		args << ", \"ne\"";
	}

	return WrapOptionFilter(args.str());
}

// Render the negation of a pure option-filter rule. Precondition: IsPureOptionFilterRule(expr).
std::string ApTranspiler::GenerateNegatedOptionFilterRule(const rls::ast::ExprPtr& expr) const {
	const auto& node = expr->node;

	// Negate the rule literal: not True_() -> False_(), not False_() -> True_().
	if (auto* lit = std::get_if<rls::ast::BoolLiteral>(&node)) {
		return lit->value ? "False_()" : "True_()";
	}

	// not (not x) == x: emit x's positive rendering (x is itself a pure option-filter rule).
	if (auto* unary = std::get_if<rls::ast::UnaryExpr>(&node)) {
		return GenerateExpression(unary->operand);
	}

	if (auto* binary = std::get_if<rls::ast::BinaryExpr>(&node)) {
		// Setting comparison leaf: flip eq <-> "ne".
		if (IsSettingComparison(*binary)) {
			return renderSettingOptionFilter(*binary, /*negate=*/true);
		}
		// De Morgan: not(a and b) = not a | not b; not(a or b) = not a & not b.
		if (binary->op == rls::ast::BinaryOp::And) {
			return GenerateNegatedChild(binary->left, 11) + " | " +
				   GenerateNegatedChild(binary->right, 11, /*isRightChild=*/true);
		}
		if (binary->op == rls::ast::BinaryOp::Or) {
			return GenerateNegatedChild(binary->left, 9) + " & " +
				   GenerateNegatedChild(binary->right, 9, /*isRightChild=*/true);
		}
	}

	if (auto* call = std::get_if<rls::ast::CallExpr>(&node)) {
		// Bare setting(K) truthiness: its negation is "the setting is off".
		if (call->callee.text == "setting") {
			auto resolvedPtr = project.getResolvedCallArgs(call);
			if (resolvedPtr && !resolvedPtr->empty()) {
				if (auto* keyId = std::get_if<rls::ast::Identifier>(&resolvedPtr->front()->node)) {
					return WrapOptionFilter(keyId->name.text + ", False");
				}
			}
		}
		// A call into a pure (no-arg) define: inline the negation of its body.
		if (auto it = project.DefineDecls.find(call->callee.text); it != project.DefineDecls.end()) {
			return GenerateNegatedOptionFilterRule(it->second->body);
		}
	}

	// Precondition violated (IsPureOptionFilterRule should have gated this) -- fall back to the
	// positive rendering rather than emit malformed output.
	return GenerateExpression(expr);
}

// Precedence of the form GenerateNegatedOptionFilterRule emits (the De Morgan dual swaps the
// and/or operator at each level), keeping negated-rule parenthesization in sync.
int ApTranspiler::NegatedPrecedence(const rls::ast::ExprPtr& expr) const {
	const auto& node = expr->node;
	if (auto* unary = std::get_if<rls::ast::UnaryExpr>(&node)) {
		return GetPythonPrecedence(unary->operand);  // negation cancels to the positive form
	}
	if (auto* binary = std::get_if<rls::ast::BinaryExpr>(&node)) {
		if (!IsSettingComparison(*binary)) {
			if (binary->op == rls::ast::BinaryOp::And) {
				return 11;  // negates to `|`
			}
			if (binary->op == rls::ast::BinaryOp::Or) {
				return 9;   // negates to `&`
			}
		}
	}
	if (auto* call = std::get_if<rls::ast::CallExpr>(&node)) {
		if (call->callee.text != "setting") {
			if (auto it = project.DefineDecls.find(call->callee.text); it != project.DefineDecls.end()) {
				return NegatedPrecedence(it->second->body);
			}
		}
	}
	// Setting comparison/bare setting and bool literals lower to an atomic call (binds tightly).
	return 0;
}

std::string ApTranspiler::GenerateNegatedChild(
	const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild) const {
	std::string result = GenerateNegatedOptionFilterRule(expr);
	int childPrec = NegatedPrecedence(expr);
	if (childPrec > parentPrec || (isRightChild && childPrec == parentPrec)) {
		return "(" + result + ")";
	}
	return result;
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
			switch (ClassifyAndOr(*bin)) {
			case AndOrLowering::RuleOp:          return 9;   // Bitwise AND (&)
			case AndOrLowering::PythonOp:        return 13;  // Python `and`
			case AndOrLowering::MixedTernary:    return 16;  // emitted as a conditional
			case AndOrLowering::Unrepresentable: return 9;   // rule-op fallback
			}
			return 9;
		case rls::ast::BinaryOp::Lt:
		case rls::ast::BinaryOp::LtEq:
		case rls::ast::BinaryOp::Gt:
		case rls::ast::BinaryOp::GtEq:
		case rls::ast::BinaryOp::Eq:
		case rls::ast::BinaryOp::NotEq:
			return 12;
		case rls::ast::BinaryOp::Or:
			switch (ClassifyAndOr(*bin)) {
			case AndOrLowering::RuleOp:          return 11;  // Bitwise OR (|)
			case AndOrLowering::PythonOp:        return 14;  // Python `or`
			case AndOrLowering::MixedTernary:    return 16;  // emitted as a conditional
			case AndOrLowering::Unrepresentable: return 11;  // rule-op fallback
			}
			return 11;
		default: return 0;
		}
	}
	if (auto* tern = std::get_if<rls::ast::TernaryExpr>(&expr->node)) {
		// A rule-conditioned ternary is emitted as an or-expression `(C & a) | b` (precedence
		// of `|`); a normal Python ternary binds loosest.
		return isRuleConditionedRuleTernary(*tern) ? 11 : 16;
	}
	if (auto* unary = std::get_if<rls::ast::UnaryExpr>(&expr->node);
		unary && unary->op == rls::ast::UnaryOp::Not) {
		// `not <build-time value>` lowers to a Python `not` (precedence between comparison
		// and `and`).
		if (ClassifyExpression(unary->operand) == ValueClass::BuildTime) {
			return 12;
		}
		// `not <pure option-filter rule>` is emitted as its De Morgan dual, so its precedence
		// is that of the negated form.
		if (IsPureOptionFilterRule(unary->operand)) {
			return NegatedPrecedence(unary->operand);
		}
	}
	// A `not setting(...)` leaf and the diagnosed cases lower to an atomic call/operand,
	// which binds tightly (0).
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
		// `not <build-time value>` is an ordinary Python negation.
		if (ClassifyExpression(node.operand) == ValueClass::BuildTime) {
			return "not " + GenerateChildExpression(node.operand, 12);
		}
		// `not <pure option-filter rule>` is representable: settings resolve at build time
		// against world.options, so the negation is sound. Push `not` down via De Morgan and
		// flip each setting leaf (eq <-> "ne"). This covers `not setting(...)` and negated
		// membership such as `not is_fire_loop_locked()`.
		if (IsPureOptionFilterRule(node.operand)) {
			return GenerateNegatedOptionFilterRule(node.operand);
		}
		// `not <collection rule>` / `not <runtime value>` cannot be expressed: the RuleBuilder
		// has no negation for a collection-state rule. Diagnose rather than silently drop the
		// `not` and emit a rule with inverted meaning.
		Diagnose(node.operand->span,
			"cannot negate a rule: the Archipelago RuleBuilder has no rule negation; only "
			"settings (setting(...) comparisons) can be negated");
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
		switch (ClassifyAndOr(node)) {
		case AndOrLowering::RuleOp:
			return GenerateChildExpression(node.left, 9) + " & " + GenerateChildExpression(node.right, 9, true);
		case AndOrLowering::PythonOp:
			return GenerateChildExpression(node.left, 13) + " and " + GenerateChildExpression(node.right, 13, true);
		case AndOrLowering::MixedTernary: {
			// `V and R` short-circuits at build time: `R if V else False_()`.
			const bool leftIsRule = ExpressionIsRule(node.left);
			const auto& ruleExpr = leftIsRule ? node.left : node.right;
			const auto& valueExpr = leftIsRule ? node.right : node.left;
			return GenerateChildExpression(ruleExpr, 15) + " if " +
				   GenerateChildExpression(valueExpr, 15) + " else False_()";
		}
		case AndOrLowering::Unrepresentable:
			// A runtime non-rule operand (e.g. bottle_count() >= 1) cannot be combined here
			// without a host rule. Diagnose; the rule-op form is a best-effort fallback.
			Diagnose(node.left->span,
				"cannot combine a runtime value (e.g. a count comparison like "
				"bottle_count() >= 1) with a rule; it must be lowered to a host rule");
			return GenerateChildExpression(node.left, 9) + " & " + GenerateChildExpression(node.right, 9, true);
		}
		return "";
	case rls::ast::BinaryOp::Or:
		switch (ClassifyAndOr(node)) {
		case AndOrLowering::RuleOp:
			return GenerateChildExpression(node.left, 11) + " | " + GenerateChildExpression(node.right, 11, true);
		case AndOrLowering::PythonOp:
			return GenerateChildExpression(node.left, 14) + " or " + GenerateChildExpression(node.right, 14, true);
		case AndOrLowering::MixedTernary: {
			// `V or R` short-circuits at build time: `True_() if V else R`.
			const bool leftIsRule = ExpressionIsRule(node.left);
			const auto& ruleExpr = leftIsRule ? node.left : node.right;
			const auto& valueExpr = leftIsRule ? node.right : node.left;
			return "True_() if " + GenerateChildExpression(valueExpr, 15) + " else " +
				   GenerateExpression(ruleExpr);
		}
		case AndOrLowering::Unrepresentable:
			// See the `and` case.
			Diagnose(node.left->span,
				"cannot combine a runtime value (e.g. a count comparison like "
				"bottle_count() >= 1) with a rule; it must be lowered to a host rule");
			return GenerateChildExpression(node.left, 11) + " | " + GenerateChildExpression(node.right, 11, true);
		}
		return "";
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

bool ApTranspiler::isRuleConditionedRuleTernary(const rls::ast::TernaryExpr& node) const {
	// A ternary lowers to the rule idiom only when its condition is a rule (not a build-time
	// value, which stays an ordinary Python `if`) and both branches are rules (a value branch
	// could not be `&`-combined with the rule condition).
	if (ClassifyExpression(node.condition) == ValueClass::BuildTime) {
		return false;
	}
	return ExpressionIsRule(node.thenBranch) && ExpressionIsRule(node.elseBranch);
}

// Python ternary syntax is "a if test else b"
std::string ApTranspiler::GenerateExpression(const rls::ast::TernaryExpr& node) const {
	// A rule-conditioned ternary cannot be a Python `if` (`bool(rule)` raises), and the
	// RuleBuilder has no rule negation to express the complement of the condition. We lower
	// `C ? a : b` to `(C & a) | b`: the then-branch stays gated by the condition, while the
	// else-branch becomes unconditional. This is always representable (no negation needed) and
	// monotonic -- gaining the condition never *removes* the else-branch's access, which is
	// what access logic wants. It deliberately does NOT synthesize a complement rule (e.g.
	// `is_adult()` for `is_child()`): the source never wrote one, and assuming the condition's
	// negation is some specific other rule would bake in an invariant the game may not hold.
	if (isRuleConditionedRuleTernary(node)) {
		return "(" + GenerateChildExpression(node.condition, 9) + " & " +
			   GenerateChildExpression(node.thenBranch, 9, true) + ") | " +
			   GenerateChildExpression(node.elseBranch, 11, true);
	}
	// Otherwise the condition becomes a Python `if`, so it must be a build-time value. A
	// runtime non-rule value (or a rule paired with a value branch) cannot be one -- diagnose
	// rather than emit code that raises at world-load.
	if (ClassifyExpression(node.condition) != ValueClass::BuildTime) {
		Diagnose(node.condition->span,
			"ternary condition must be a build-time value; a rule cannot be used as a "
			"Python condition (the RuleBuilder raises on bool(rule))");
	}
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
	// Classify the arms. A rule body anywhere makes this a rule match (matched arms are
	// |-combined, accumulating down `or`-fallthrough chains); otherwise the arms produce
	// build-time values and the match returns the selected one. A runtime non-rule body
	// (e.g. a bottle_count comparison) is unrepresentable.
	bool anyRule = false;
	bool anyRuntime = false;
	for (const auto& arm : node.arms) {
		switch (ClassifyExpression(arm.body)) {
		case ValueClass::Rule:    anyRule = true; break;
		case ValueClass::Runtime: anyRuntime = true; break;
		case ValueClass::BuildTime: break;
		}
	}

	// Each arm renders to a flat `condition, body, fallthrough` triple. The condition is a
	// zero-arg predicate that closes over the (build-time) discriminant; the body is a
	// zero-arg thunk so the helper can pick/combine arms lazily.
	const std::string disc = GenerateExpression(node.discriminant);
	std::ostringstream arms;
	for (size_t i = 0; i < node.arms.size(); i++) {
		const auto& arm = node.arms[i];
		if (i > 0) arms << ", ";

		if (arm.isDefault) {
			arms << "(lambda: True)";
		} else {
			arms << "(lambda " << disc << "=" << disc << ": ";
			for (size_t j = 0; j < arm.patterns.size(); j++) {
				if (j > 0) arms << " or ";
				arms << disc << " == " << GenerateExpression(arm.patterns[j]);
			}
			arms << ")";
		}
		arms << ", (lambda: " << GenerateExpression(arm.body) << ")";
		arms << ", " << (arm.fallthrough ? "True" : "False");
	}

	if (anyRule) {
		return "rls_match_rule(" + arms.str() + ")";
	}
	if (anyRuntime) {
		// A value match whose result depends on collection state cannot be represented;
		// diagnose and fall back to a value match so generation still produces something.
		Diagnose(node.discriminant->span,
			"match arms produce a runtime value that is neither a rule nor build-time; "
			"it cannot be represented (lower it to a host rule)");
		return "rls_match_value(0, " + arms.str() + ")";
	}
	// A build-time value match: default to the additive identity for the result type
	// (0 for ints, False for bools) when no arm matches.
	const std::string defaultValue =
		project.getType(node.arms.empty() ? nullptr : node.arms.front().body.get()) == rls::ast::Type::Bool
			? "False" : "0";
	return "rls_match_value(" + defaultValue + ", " + arms.str() + ")";
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
