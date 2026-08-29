#include "ap_transpiler.h"

#include <sstream>

namespace rls::transpilers::ap {

namespace {

// The resolved RSK_* key of a `setting(KEY)` call, or nullptr when `expr` is not one.
const rls::ast::Identifier* settingKeyOf(const rls::ast::Project& project, const rls::ast::Expr* expr) {
	auto* call = std::get_if<rls::ast::CallExpr>(&expr->node);
	if (call == nullptr || call->callee.text != "setting") {
		return nullptr;
	}
	const auto* resolved = project.getResolvedCallArgs(call);
	if (resolved == nullptr || resolved->empty()) {
		return nullptr;
	}
	return std::get_if<rls::ast::Identifier>(&resolved->front()->node);
}

} // namespace

std::string ApTranspiler::WrapOptionFilter(const std::string& optionFilterArgs) const {
	return "True_(options=[OptionFilter(" + optionFilterArgs + ")])";
}

std::optional<ApTranspiler::SettingComparison> ApTranspiler::MatchSettingComparison(
	const rls::ast::BinaryExpr& node) const {
	// An OptionFilter tests equality only, so only == / != can be one.
	if (node.op != rls::ast::BinaryOp::Eq && node.op != rls::ast::BinaryOp::NotEq) {
		return std::nullopt;
	}
	// Either operand may be the setting() call: `setting(K) is RO_X` and `RO_X is setting(K)`
	// mean the same thing and must lower the same way.
	auto tryOrder = [&](const rls::ast::Expr* keySide,
		const rls::ast::Expr* valueSide) -> std::optional<SettingComparison> {
		const auto* key = settingKeyOf(project, keySide);
		// The filter compares the option against a value fixed at generation, so the other side
		// must be build-time -- a bare or dotted enum value, a literal, a parameter. A rule there
		// (another setting() call, has(...)) has no OptionFilter form.
		if (key == nullptr || ClassifyExpression(valueSide) != ValueClass::BuildTime) {
			return std::nullopt;
		}
		return SettingComparison{key, valueSide};
	};
	if (auto match = tryOrder(node.left.get(), node.right.get())) {
		return match;
	}
	return tryOrder(node.right.get(), node.left.get());
}

// True if this binary expression is a `setting(KEY) == VALUE` / `!= VALUE` comparison.
bool ApTranspiler::IsSettingComparison(const rls::ast::BinaryExpr& node) const {
	return MatchSettingComparison(node).has_value();
}

bool ApTranspiler::HasSettingOperand(const rls::ast::BinaryExpr& node) const {
	return settingKeyOf(project, node.left.get()) != nullptr ||
		   settingKeyOf(project, node.right.get()) != nullptr;
}

void ApTranspiler::DiagnoseTopLevelValue(const rls::ast::ExprPtr& expr) const {
	if (ClassifyExpression(expr) != ValueClass::Runtime) {
		return;
	}
	Diagnose(expr->span,
		"this rule is a runtime value that is neither a rule nor build-time (e.g. a count "
		"comparison like bottle_count() >= 1); it would be frozen against the empty initial "
		"collection state -- lower it to a host rule");
}

// Try to generate an OptionFilter expression for setting comparisons.
// Returns empty string if not a setting comparison; caller should use standard binary expression.
std::string ApTranspiler::TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const {
	if (!IsSettingComparison(node)) {
		return "";
	}
	return renderSettingOptionFilter(node, /*negate=*/false);
}

std::string ApTranspiler::optionFilterArgs(const rls::ast::BinaryExpr& node, bool negate) const {
	const SettingComparison comparison = *MatchSettingComparison(node);

	std::ostringstream args;
	// The compared value sits in a plain Python value position, exactly like a call argument, so
	// it renders through the same path (a Bool there is `True`/`False`, not `True_()`/`False_()`).
	args << comparison.key->name.text << ", " << GenerateCallArgument(comparison.value, std::nullopt);
	// `!=` / `is not` is the "ne" operator; negation flips eq <-> ne.
	bool ne = (node.op == rls::ast::BinaryOp::NotEq);
	if (negate) {
		ne = !ne;
	}
	if (ne) {
		args << ", \"ne\"";
	}
	return args.str();
}

std::string ApTranspiler::renderSettingOptionFilter(const rls::ast::BinaryExpr& node, bool negate) const {
	return WrapOptionFilter(optionFilterArgs(node, negate));
}

std::string ApTranspiler::renderSettingCheck(const rls::ast::BinaryExpr& node) const {
	return "OptionFilter(" + optionFilterArgs(node, /*negate=*/false) + ").check(" + ruleContextOptions() + ")";
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

std::string ApTranspiler::GenerateExpression(const rls::ast::StringLiteral& node) const {
	return "\"" + node.value + "\"";
}

std::string ApTranspiler::GenerateExpression(const rls::ast::ListExpr& node) const {
	// Lists only ever appear in region data (e.g. `areas: [RA_X, RA_Y]`), which this
	// transpiler reads directly rather than through expression generation -- so a list
	// reaching here is a list in a rule expression, which RuleBuilder has no form for.
	// ListExpr carries no span of its own; the first element locates it well enough.
	Diagnose(node.elements.empty() ? rls::ast::Span{} : node.elements.front()->span,
		"a list is not representable in an Archipelago rule expression");
	return "";
}

std::string ApTranspiler::GenerateExpression(const rls::ast::MemberExpr& node) const {
	// `EnumName.ValueName`: the enum is named explicitly, so no type lookup is needed.
	return renderEnumValue(node.object.text, node.member.text);
}

std::string ApTranspiler::GenerateExpression(const rls::ast::Identifier& node) const {
	if (node.kind == rls::ast::IdentifierKind::EnumValue) {
		auto enumName = project.getEnumType(&node);
		if (!enumName.has_value()) {
			return node.name.text;
		}
		return renderEnumValue(*enumName, node.name.text);
	} else if (node.kind == rls::ast::IdentifierKind::Parameter) {
		return node.name.text;
	} else if (node.kind == rls::ast::IdentifierKind::DeclaredValue) {
		// A region/event/location declared in RLS itself: sema resolved the kind, but the
		// enum it belongs to is implied by which decl table holds it.
		if (project.RegionDecls.contains(node.name.text)) {
			return renderEnumValue("Region", node.name.text);
		}
		if (project.EventDecls.contains(node.name.text)) {
			return renderEnumValue("Event", node.name.text);
		}
		if (project.LocationDecls.contains(node.name.text)) {
			return renderEnumValue("Location", node.name.text);
		}
		return "";
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
		// A rule-conditioned ternary is emitted as an rls_conditional(...) call, which binds as
		// tightly as any call; a normal Python ternary binds loosest.
		return isRuleConditionedRuleTernary(*tern) ? 0 : 16;
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

	// A `setting(...)` operand that survived both of the above has no lowering: an OptionFilter
	// tests a setting for equality against a build-time value and nothing else. Emitting the
	// operation anyway would apply it to a Rule object -- `==`/`!=` would silently compare by
	// identity (always False / always True), an ordered comparison or arithmetic would raise at
	// world-load. `and`/`or` are excluded: a bare setting(...) truthiness guard is a proper rule
	// and combines normally. Diagnose; the raw form below is a best-effort fallback.
	if (node.op != rls::ast::BinaryOp::And && node.op != rls::ast::BinaryOp::Or &&
		HasSettingOperand(node)) {
		Diagnose(node.left->span,
			"a setting can only be compared for equality (`is` / `is not`) against a build-time "
			"value; the Archipelago OptionFilter has no other comparison");
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
		// RLS Div is Int / Int -> Int, and the C++ target emits truncating integer division.
		// Python's `/` is float division, so `//` is what matches (both operands are
		// non-negative counts, where `//` and truncation agree).
		return GenerateChildExpression(node.left, 6) + " // " + GenerateChildExpression(node.right, 6, true);
	default:
		return "";
	}
}

bool ApTranspiler::isBuildTimeSettingCondition(const rls::ast::ExprPtr& cond) const {
	// A pure option-filter expression is build-time only if we have somewhere to read the
	// options from; without an accessor, .check() cannot be emitted.
	return !ruleContextOptions().empty() && IsPureOptionFilterRule(cond);
}

std::string ApTranspiler::GenerateBuildTimeSettingCondition(const rls::ast::ExprPtr& expr) const {
	const auto& node = expr->node;

	// true/false/always/never -> plain Python bools.
	if (auto* lit = std::get_if<rls::ast::BoolLiteral>(&node)) {
		return lit->value ? "True" : "False";
	}
	// The only unary over a pure rule is `not`. Parenthesize to stay above `and`/`or`.
	if (auto* unary = std::get_if<rls::ast::UnaryExpr>(&node)) {
		return "not (" + GenerateBuildTimeSettingCondition(unary->operand) + ")";
	}
	if (auto* binary = std::get_if<rls::ast::BinaryExpr>(&node)) {
		if (IsSettingComparison(*binary)) {
			return renderSettingCheck(*binary);
		}
		// and/or of pure settings -> Python and/or. Parenthesize compound operands (safe, and
		// keeps mixed and/or nests unambiguous).
		if (binary->op == rls::ast::BinaryOp::And) {
			return "(" + GenerateBuildTimeSettingCondition(binary->left) + ") and (" +
				   GenerateBuildTimeSettingCondition(binary->right) + ")";
		}
		if (binary->op == rls::ast::BinaryOp::Or) {
			return "(" + GenerateBuildTimeSettingCondition(binary->left) + ") or (" +
				   GenerateBuildTimeSettingCondition(binary->right) + ")";
		}
	}
	if (auto* call = std::get_if<rls::ast::CallExpr>(&node)) {
		// Bare setting(K) truthiness guard: OptionFilter(K, True).check(...).
		if (call->callee.text == "setting") {
			auto resolvedPtr = project.getResolvedCallArgs(call);
			if (resolvedPtr && !resolvedPtr->empty()) {
				if (auto* keyId = std::get_if<rls::ast::Identifier>(&resolvedPtr->front()->node)) {
					return "OptionFilter(" + keyId->name.text + ", True).check(" + ruleContextOptions() + ")";
				}
			}
		}
		// A call into a pure (no-arg) define: inline its body's build-time form.
		if (auto it = project.DefineDecls.find(call->callee.text); it != project.DefineDecls.end()) {
			return GenerateBuildTimeSettingCondition(it->second->body);
		}
	}

	// Precondition violated (isBuildTimeSettingCondition should have gated this) -- fall back to
	// the positive rendering rather than emit malformed output.
	return GenerateExpression(expr);
}

bool ApTranspiler::isRuleConditionedRuleTernary(const rls::ast::TernaryExpr& node) const {
	// A pure setting condition is evaluated at build time via .check(), so it is not the
	// rule-conditioned case; the ternary stays an ordinary Python conditional (precedence 16).
	if (isBuildTimeSettingCondition(node.condition)) {
		return false;
	}
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
	// A pure setting condition resolves at build time against world.options, so it can be a
	// real Python condition via OptionFilter.check(). The ternary then lowers to an ordinary
	// `a if <check> else b` for ANY branch types -- int (e.g. small_keys count), enum, or rule.
	if (isBuildTimeSettingCondition(node.condition)) {
		return GenerateExpression(node.thenBranch) + " if " +
			   GenerateBuildTimeSettingCondition(node.condition) + " else " +
			   GenerateExpression(node.elseBranch);
	}
	// A rule-conditioned ternary cannot be a Python `if` (`bool(rule)` raises), and the
	// RuleBuilder has no rule negation to express the complement of the condition. Both branches
	// are handed to the host conditional rule, which evaluates the condition at solve time and
	// returns the branch it selects -- an exact mirror of the source ternary, needing no
	// negation and no synthesized complement for the condition. This is the same lowering the
	// value-branch case gets by distributing the call (see tryDistributeTernaryArg); the older
	// `(C & a) | b` idiom is not used because it left the else-branch ungated, granting access
	// the source never wrote.
	if (isRuleConditionedRuleTernary(node)) {
		return renderConditionalRule(GenerateExpression(node.condition),
			GenerateExpression(node.thenBranch), GenerateExpression(node.elseBranch));
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
	if (project.getResolvedCallArgs(&node) == nullptr) {
		// Unknown calls or calls with semantic errors are blocked earlier in sema;
		// emit empty as a defensive fallback so generation does not invent call forms.
		return "";
	}

	// A rule-conditioned ternary passed as a value argument is distributed over the call, turning
	// it into a conditional rule (finding D). This runs before the setting/host/default dispatch
	// so it also covers host-rewrite calls (e.g. small_keys(SCENE, rule ? 2 : 3)): each branch is
	// re-rendered through the full dispatch (renderCall), so host rewrites still apply per branch.
	if (auto distributed = tryDistributeTernaryArg(node)) {
		return *distributed;
	}

	return renderCall(node, std::string::npos, nullptr);
}

std::string ApTranspiler::renderCall(const rls::ast::CallExpr& node, size_t overrideIdx,
	const rls::ast::Expr* overrideExpr) const {
	const auto& resolved = *project.getResolvedCallArgs(&node);

	// setting(KEY) is a truthiness check, emitted as an OptionFilter rule (AP-generic). Its sole
	// argument is a Setting key, never a distributed ternary branch, so the override never applies.
	if (node.callee.text == "setting") {
		if (auto* id = std::get_if<rls::ast::Identifier>(&resolved[0]->node)) {
			return WrapOptionFilter(id->name.text + std::string(", True"));
		}
		return "";
	}

	// Game-specific host-call rewrites (has, flag, trick, small_keys, ...).
	if (auto hostCall = renderHostCall(node, overrideIdx, overrideExpr)) {
		return *hostCall;
	}

	// Default: a regular function call, optionally threading the rule-context
	// receiver (e.g. SoH's `bundle`) as the implicit first argument.
	return renderDefaultCall(node, overrideIdx, overrideExpr);
}

std::string ApTranspiler::renderDefaultCall(const rls::ast::CallExpr& node, size_t overrideIdx,
	const rls::ast::Expr* overrideExpr) const {
	const auto& resolved = *project.getResolvedCallArgs(&node);
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

		const rls::ast::Expr* arg = (i == overrideIdx) ? overrideExpr : resolved[i];
		oss << GenerateCallArgument(arg, ResolveCallParamType(node, i));
	}
	oss << ")";
	return oss.str();
}

std::optional<std::string> ApTranspiler::tryDistributeTernaryArg(const rls::ast::CallExpr& node) const {
	const auto* resolvedPtr = project.getResolvedCallArgs(&node);
	if (resolvedPtr == nullptr) {
		return std::nullopt;
	}
	// Only distribute when the call itself yields a Rule, so each branch call (f(A), f(B)) is a
	// rule the conditional can hold. A value-returning callee (e.g. check_price/price_of) would
	// otherwise wrap non-rule values in a conditional rule; leave it to the normal path, which
	// diagnoses the rule-conditioned value ternary as unrepresentable.
	if (ClassifyCall(node) != ValueClass::Rule) {
		return std::nullopt;
	}
	const auto& resolved = *resolvedPtr;
	for (size_t i = 0; i < resolved.size(); ++i) {
		auto* tern = std::get_if<rls::ast::TernaryExpr>(&resolved[i]->node);
		if (tern == nullptr) {
			continue;
		}
		// A build-time or pure-setting condition already lowers to an ordinary Python `if`
		// ternary in the argument -- it needs no rule to pick the branch.
		if (isBuildTimeSettingCondition(tern->condition) ||
			ClassifyExpression(tern->condition) != ValueClass::Rule) {
			continue;
		}
		// Only value branches are distributed. A rule-branch ternary is representable directly
		// (and never appears as a value argument, whose parameter is not a rule).
		if (ExpressionIsRule(tern->thenBranch) || ExpressionIsRule(tern->elseBranch)) {
			continue;
		}
		const std::string cond = GenerateExpression(tern->condition->node);
		const std::string thenCall = renderCall(node, i, tern->thenBranch.get());
		const std::string elseCall = renderCall(node, i, tern->elseBranch.get());
		return renderConditionalRule(cond, thenCall, elseCall);
	}
	return std::nullopt;
}

std::string ApTranspiler::renderConditionalRule(const std::string& cond,
	const std::string& thenExpr, const std::string& elseExpr) const {
	const std::string receiver = ruleContextParam();
	const std::string prefix = receiver.empty() ? "" : receiver + ", ";
	return "rls_conditional(" + prefix + cond + ", " + thenExpr + ", " + elseExpr + ")";
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

std::string ApTranspiler::GenerateExpression(const rls::ast::HereRef& node) const {
	// `here` lowers to a reference to the enclosing region, resolved by sema. It renders
	// like any other value of the host's region enum (e.g. SoH's `Regions.<name>`). The
	// enum name is fixed here because `here` has no identifier node to look up.
	return renderEnumValue("Region", node.resolvedRegion.text);
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
