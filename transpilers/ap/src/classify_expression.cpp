// Value classification: decide whether an RLS expression lowers to a runtime Rule
// object, a build-time Python value, or a runtime non-rule value in the Archipelago
// RuleBuilder target. This is pure analysis over the resolved AST -- it produces no
// output -- and is the keystone the function-generation bridging (and/or/not/ternary)
// builds on. See docs/AP-Function-Generation.md for the model and bridging tables.
//
// The distinction is about *when* a value is known. The rule lambda runs once to build
// a Rule tree; a build-time value is frozen then, a Rule re-evaluates at solve time, and
// a runtime non-rule value (e.g. bottle_count()) is collection-state dependent yet not a
// Rule -- so it can only be represented via a dedicated host rule.
#include "ap_transpiler.h"

namespace rls::transpilers::ap {

// Worse-of, where Runtime dominates Rule dominates BuildTime. Used to combine operand
// classes for operators that fold their operands (and/or, comparisons, calls): a Runtime
// operand makes the whole thing Runtime; otherwise a Rule operand makes it a Rule.
ApTranspiler::ValueClass ApTranspiler::JoinClass(ValueClass a, ValueClass b) {
	if (a == ValueClass::Runtime || b == ValueClass::Runtime) {
		return ValueClass::Runtime;
	}
	if (a == ValueClass::Rule || b == ValueClass::Rule) {
		return ValueClass::Rule;
	}
	return ValueClass::BuildTime;
}

ApTranspiler::ValueClass ApTranspiler::ClassifyExpression(const rls::ast::ExprPtr& expr) const {
	return ClassifyExpression(expr.get());
}

ApTranspiler::ValueClass ApTranspiler::ClassifyExpression(const rls::ast::Expr* expr) const {
	if (!expr) {
		return ValueClass::BuildTime;
	}
	const auto& node = expr->node;

	// true/false/always/never lower to the rule literals True_()/False_().
	if (std::holds_alternative<rls::ast::BoolLiteral>(node)) {
		return ValueClass::Rule;
	}
	// Integer literals are build-time.
	if (std::holds_alternative<rls::ast::IntLiteral>(node)) {
		return ValueClass::BuildTime;
	}
	// Identifiers are build-time: an enum value is a Python constant, a parameter carries
	// a build-time value (a deferred rule is passed as a Condition, not a bare Bool), and
	// a FunctionRef is a callable object.
	if (std::holds_alternative<rls::ast::Identifier>(node)) {
		return ValueClass::BuildTime;
	}
	// Invoking a Condition yields a Rule.
	if (std::holds_alternative<rls::ast::InvokeExpr>(node)) {
		return ValueClass::Rule;
	}
	// `here` resolves to a region name -- a build-time enum constant, like any other
	// Region-typed identifier.
	if (std::holds_alternative<rls::ast::HereRef>(node)) {
		return ValueClass::BuildTime;
	}

	// `not` preserves class: `not setting(...)` is still an OptionFilter rule, while
	// `not <build-time value>` stays a build-time bool.
	if (auto* unary = std::get_if<rls::ast::UnaryExpr>(&node)) {
		return ClassifyExpression(unary->operand);
	}

	// A ternary is a Rule if either branch is a Rule (the build-time condition selects
	// between them). If both branches are build-time but the condition is not, the result
	// is a runtime value (e.g. wallet_capacity: has(X) ? 999 : ...).
	if (auto* ternary = std::get_if<rls::ast::TernaryExpr>(&node)) {
		ValueClass thenClass = ClassifyExpression(ternary->thenBranch);
		ValueClass elseClass = ClassifyExpression(ternary->elseBranch);
		if (thenClass == ValueClass::Rule || elseClass == ValueClass::Rule) {
			return ValueClass::Rule;
		}
		ValueClass condClass = ClassifyExpression(ternary->condition);
		return condClass == ValueClass::BuildTime ? ValueClass::BuildTime : ValueClass::Runtime;
	}

	// A match mirrors the ternary: a Rule if any arm body is a Rule; build-time only if
	// the discriminant and every body are build-time; otherwise a runtime value.
	if (auto* match = std::get_if<rls::ast::MatchExpr>(&node)) {
		ValueClass combined = ClassifyExpression(match->discriminant);
		bool anyRule = false;
		for (const auto& arm : match->arms) {
			ValueClass bodyClass = ClassifyExpression(arm.body);
			anyRule = anyRule || bodyClass == ValueClass::Rule;
			combined = JoinClass(combined, bodyClass);
		}
		if (anyRule) {
			return ValueClass::Rule;
		}
		return combined == ValueClass::BuildTime ? ValueClass::BuildTime : ValueClass::Runtime;
	}

	if (auto* binary = std::get_if<rls::ast::BinaryExpr>(&node)) {
		// setting(KEY) == / != VALUE lowers to an OptionFilter rule.
		if (IsSettingComparison(*binary)) {
			return ValueClass::Rule;
		}
		// A game-specific rewrite (e.g. wallet capacity, triforce hunt) collapses the
		// comparison to an atomic rule call.
		if (renderBinarySpecialCase(*binary)) {
			return ValueClass::Rule;
		}
		// Conjunction/disjunction and comparisons/arithmetic all fold their operands: a
		// Runtime operand wins, then a Rule operand. (For `and`/`or` this yields the Rule
		// vs build-time mix; for comparisons over a runtime quantity it yields Runtime --
		// e.g. bottle_count() >= 1.)
		return JoinClass(ClassifyExpression(binary->left), ClassifyExpression(binary->right));
	}

	if (auto* call = std::get_if<rls::ast::CallExpr>(&node)) {
		// setting(KEY) used as a truthiness guard lowers to an OptionFilter rule.
		if (call->callee.text == "setting") {
			return ValueClass::Rule;
		}
		// A call into a user define takes the define's own class. A Rule define is a Rule
		// regardless of its arguments; a value-define is build-time unless an argument is
		// itself runtime (or a rule spliced into a value parameter), which it then is.
		if (auto it = project.DefineDecls.find(call->callee.text); it != project.DefineDecls.end()) {
			ValueClass defineClass = DefineClass(it->second);
			if (defineClass == ValueClass::Rule) {
				return ValueClass::Rule;
			}
			ValueClass combined = defineClass;
			if (auto* args = project.getResolvedCallArgs(call)) {
				for (const rls::ast::Expr* arg : *args) {
					combined = JoinClass(combined, ClassifyExpression(arg));
				}
			}
			return combined;
		}
		// A host/extern function returning Bool is a runtime rule (has, can_use, trick,
		// is_child, ...). Any other host return -- notably Int (bottle_count,
		// check_price, the triforce counts) -- is a runtime non-rule value: it depends on
		// collection state but is not itself a Rule.
		if (auto it = project.ExternDefineDecls.find(call->callee.text); it != project.ExternDefineDecls.end()) {
			const bool returnsBool = it->second->returnType && it->second->returnType->name.text == "Bool";
			return returnsBool ? ValueClass::Rule : ValueClass::Runtime;
		}
		// Unknown callee (blocked earlier in sema): assume a host rule, conservatively.
		return ValueClass::Rule;
	}

	return ValueClass::BuildTime;
}

bool ApTranspiler::ExpressionIsRule(const rls::ast::ExprPtr& expr) const {
	return ClassifyExpression(expr) == ValueClass::Rule;
}

ApTranspiler::ValueClass ApTranspiler::DefineClass(const rls::ast::DefineDecl* decl) const {
	if (auto it = defineClassCache.find(decl); it != defineClassCache.end()) {
		return it->second;
	}
	// A define that (transitively) calls itself is treated as a Rule; logic helpers do
	// not actually recurse, so this only guards against pathological input.
	if (!defineClassInProgress.insert(decl).second) {
		return ValueClass::Rule;
	}
	ValueClass result = ClassifyExpression(decl->body);
	defineClassInProgress.erase(decl);
	defineClassCache[decl] = result;
	return result;
}

bool ApTranspiler::IsPureOptionFilterRule(const rls::ast::ExprPtr& expr) const {
	return IsPureOptionFilterRule(expr.get());
}

bool ApTranspiler::IsPureOptionFilterRule(const rls::ast::Expr* expr) const {
	if (!expr) {
		return false;
	}
	const auto& node = expr->node;

	// true/false/always/never -> True_()/False_(), trivially negatable.
	if (std::holds_alternative<rls::ast::BoolLiteral>(node)) {
		return true;
	}
	// not <pure> is still a pure option-filter rule (negation of one stays one).
	if (auto* unary = std::get_if<rls::ast::UnaryExpr>(&node)) {
		return IsPureOptionFilterRule(unary->operand);
	}
	if (auto* binary = std::get_if<rls::ast::BinaryExpr>(&node)) {
		// A setting comparison is the pure leaf.
		if (IsSettingComparison(*binary)) {
			return true;
		}
		// and/or of pure option-filter rules stays pure. Other binary ops (comparisons,
		// arithmetic) are not option-filter rules.
		if (binary->op == rls::ast::BinaryOp::And || binary->op == rls::ast::BinaryOp::Or) {
			return IsPureOptionFilterRule(binary->left) && IsPureOptionFilterRule(binary->right);
		}
		return false;
	}
	if (auto* call = std::get_if<rls::ast::CallExpr>(&node)) {
		// Bare setting(K) truthiness guard.
		if (call->callee.text == "setting") {
			return true;
		}
		// A call into a user define is pure iff its body is. Only no-argument defines qualify:
		// inlining the negated body has no parameter substitution, so a parameterized body
		// (even a pure-setting one) is treated as impure rather than miscompiled. Memoized with
		// a cycle guard; a (pathological) cycle is conservatively impure.
		if (auto it = project.DefineDecls.find(call->callee.text); it != project.DefineDecls.end()) {
			const rls::ast::DefineDecl* decl = it->second;
			if (!decl->params.empty()) {
				return false;
			}
			if (auto c = pureOptionFilterCache.find(decl); c != pureOptionFilterCache.end()) {
				return c->second;
			}
			if (!pureOptionFilterInProgress.insert(decl).second) {
				return false;
			}
			bool result = IsPureOptionFilterRule(decl->body);
			pureOptionFilterInProgress.erase(decl);
			pureOptionFilterCache[decl] = result;
			return result;
		}
		// Host/extern calls (has, can_use, trick, flag, ...) are collection rules -- impure.
		return false;
	}
	// Identifiers, ints, ternaries, matches, invokes, here refs: not option-filter rules.
	return false;
}

ApTranspiler::AndOrLowering ApTranspiler::ClassifyAndOr(const rls::ast::BinaryExpr& node) const {
	const ValueClass left = ClassifyExpression(node.left);
	const ValueClass right = ClassifyExpression(node.right);
	if (left == ValueClass::Runtime || right == ValueClass::Runtime) {
		return AndOrLowering::Unrepresentable;
	}
	if (left == ValueClass::Rule && right == ValueClass::Rule) {
		return AndOrLowering::RuleOp;
	}
	if (left == ValueClass::BuildTime && right == ValueClass::BuildTime) {
		return AndOrLowering::PythonOp;
	}
	return AndOrLowering::MixedTernary;
}

} // namespace rls::transpilers::ap
