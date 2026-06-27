#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ast.h"
#include "output.h"

namespace rls::transpilers::ap {

// Generic RLS -> Archipelago RuleBuilder transpiler.
//
// This base class owns everything that is true of *any* AP world: the expression
// walk, Python operator precedence/parentheses, OptionFilter wrapping of setting
// comparisons, True_/False_ rule literals, lambda composition, and the region/enum/
// function file structure.
//
// Every game-specific decision is a protected virtual hook. Hooks with a sensible
// generic default are implemented here; hooks that are pure scaffolding (region
// helper names, enum classes, Python type names) are pure virtual, which makes this
// class abstract. A concrete game transpiler derives from it and overrides the hooks
// it needs -- see SohApTranspiler.
class ApTranspiler {
public:
	virtual ~ApTranspiler() = default;

	// Emit the source files for this transpiler. Derived classes choose which of
	// the Generate*Source building blocks to call (e.g. SoH emits regions + enums).
	virtual void Transpile(rls::OutputWriter& out) const = 0;

	std::string GenerateExpression(const rls::ast::ExprPtr& expr) const;
	std::string GenerateExpression(const rls::ast::Expr::Variant& node) const;
	void SetCurrentLocation(std::optional<std::string> location) const;

	// True if the expression lowers to a runtime Rule object (see ValueClass::Rule).
	// Thin wrapper over ClassifyExpression for callers that only care about rule-ness.
	bool ExpressionIsRule(const rls::ast::ExprPtr& expr) const;

	// Diagnostics raised while generating expressions: constructs that type-check in RLS
	// but cannot be expressed in the RuleBuilder target (negating a rule, a rule-valued
	// ternary condition, combining a runtime non-rule value). Accumulated across a
	// Transpile() so the driver can report them and abort rather than emit code that
	// raises at world-load. See docs/AP-Function-Generation.md §6.4.
	const std::vector<rls::ast::Diagnostic>& Diagnostics() const;

protected:
	explicit ApTranspiler(const rls::ast::Project& project);

	// Building blocks a derived Transpile() can choose from.
	void GenerateFunctionDefinitionsSource(rls::OutputWriter& out) const;
	void GenerateRegionsSource(rls::OutputWriter& out) const;
	void GenerateEnumsSource(rls::OutputWriter& out) const;

	// == Game-specific hooks ==================================================
	// Hooks with a generic default are defined in the base; override to change.

	// Name of the implicit first parameter threaded through generated rule
	// lambdas, calls and function signatures (SoH uses "bundle"). Default: empty,
	// meaning calls/signatures take no implicit receiver.
	virtual std::string ruleContextParam() const;

	// Render an enum-value identifier (e.g. RG_HOOKSHOT) to its Python form.
	// Default: the bare value name. Override to add world enum-class prefixes
	// and value overrides (e.g. RO_GENERIC_YES -> "True").
	virtual std::string renderEnumValue(rls::ast::Type type, const std::string& name) const;

	// Rewrite a host/builtin call (has, flag, trick, ...) to Python. Default:
	// std::nullopt, so the core emits the default call form `callee(args...)`
	// (with the ruleContextParam() receiver prepended when one is set).
	virtual std::optional<std::string> renderHostCall(const rls::ast::CallExpr& node) const;

	// Rewrite a world-specific binary special case. Default: std::nullopt
	// (normal binary-operator handling).
	virtual std::optional<std::string> renderBinarySpecialCase(const rls::ast::BinaryExpr& node) const;

	// World-specific block-node renderings. Default: empty string.
	virtual std::string renderSharedBlock(const rls::ast::SharedBlock& node) const;

	// True if a user `define` of this name is supplied natively by the host world and so
	// must NOT be emitted as a generated function. The canonical cases are defines whose AP
	// lowering is a hand-written host rule (SoH's `has_bottle`) or that exist only to be
	// folded away at their call sites (SoH's `wallet_capacity`, collapsed into
	// `can_afford_slot` by renderBinarySpecialCase) -- emitting them would shadow the host
	// helper or produce an unrepresentable body. Default: false (every define is emitted).
	virtual bool isHostProvidedDefine(const std::string& name) const;

	// == Game-specific scaffolding (pure virtual: no generic AP default) ======

	// Preamble for the regions file, ending with the rule-setup def line.
	virtual std::string regionsPreamble() const = 0;
	// Leading args of a region's helper call: e.g. `Regions.<key>, world, [\n`.
	virtual std::string regionCreationArgs(const std::string& regionKey) const = 0;
	// Names of the per-region helper calls.
	virtual std::string addEventsFn() const = 0;
	virtual std::string addLocationsFn() const = 0;
	virtual std::string connectRegionsFn() const = 0;
	// Per-entry tuple lines emitted inside each helper call.
	virtual std::string eventEntryLine(
		const std::string& regionKey, const std::string& entryName, const std::string& rule) const = 0;
	virtual std::string locationEntryLine(const std::string& entryName, const std::string& rule) const = 0;
	virtual std::string exitEntryLine(const std::string& entryName, const std::string& rule) const = 0;
	// Emit the entire enums file (world enum-class scaffolding).
	virtual void writeEnums(rls::OutputWriter& out) const = 0;
	// Preamble for the functions file (header comment + imports).
	virtual std::string functionsPreamble() const = 0;
	// Python type name for an RLS type, used in generated function signatures.
	virtual std::string pythonTypeName(rls::ast::Type type) const = 0;

	const rls::ast::Project& project;
	mutable std::optional<std::string> currentLocationName;

	// Record an error diagnostic for an unrepresentable construct at `span`.
	void Diagnose(const rls::ast::Span& span, std::string message) const;

private:
	mutable std::vector<rls::ast::Diagnostic> diagnostics;

	// How a Bool/Int expression lowers to the RuleBuilder target. The keystone of
	// function generation (see docs/AP-Function-Generation.md). The rule lambda
	// `lambda bundle: <expr>` runs ONCE to build a Rule tree; only the resulting
	// Rule re-evaluates against collection state. So an expression's class is about
	// *when* its value is known:
	//  - Rule:      lowers to a Rule object whose truth is re-evaluated at solve time
	//               (has(X), can_use(X), setting comparisons, a define that is a Rule).
	//  - BuildTime: a plain Python value fixed when the lambda runs -- int/enum
	//               literals, parameters (bound to literals/config at the call that
	//               builds the rule), value comparisons over build-time operands, a
	//               value-define like distance_to_int. Safe to use as a ternary
	//               condition because it is frozen at build time.
	//  - Runtime:   a non-rule value that depends on collection state, so it is NOT
	//               fixed at build time -- e.g. bottle_count() or a comparison over it.
	//               It cannot be a Rule *or* a build-time condition; it must be lowered
	//               to a host rule (like has_bottle_count) or rejected with a
	//               diagnostic. Folding it as a build-time condition would freeze it to
	//               its value in the initial (empty) collection state -- a miscompile.
	// The RLS type alone does not decide this: has(X) and bottle_count() >= 1 are both
	// Bool, but the first is Rule and the second is Runtime.
	enum class ValueClass { Rule, BuildTime, Runtime };
	ValueClass ClassifyExpression(const rls::ast::Expr* expr) const;
	ValueClass ClassifyExpression(const rls::ast::ExprPtr& expr) const;

	// Combine two operand classes for an operator that folds its operands: Runtime
	// dominates Rule dominates BuildTime.
	static ValueClass JoinClass(ValueClass a, ValueClass b);

	// A user define's class, computed from its body (with parameters treated as
	// BuildTime). Memoized because the same define is queried repeatedly; the
	// in-progress set breaks recursion cycles conservatively (a cycle is a Rule).
	ValueClass DefineClass(const rls::ast::DefineDecl* decl) const;
	mutable std::map<const rls::ast::DefineDecl*, ValueClass> defineClassCache;
	mutable std::set<const rls::ast::DefineDecl*> defineClassInProgress;

	// How a Bool `and`/`or` lowers, given its operands' classes (§4.1 of
	// docs/AP-Function-Generation.md):
	//  - RuleOp:         both operands are rules     -> `L & R` / `L | R`
	//  - PythonOp:       both operands are build-time -> `L and R` / `L or R`
	//  - MixedTernary:   one rule, one build-time     -> `R if V else False_()` (and) /
	//                                                    `True_() if V else R` (or)
	//  - Unrepresentable: a Runtime operand is involved -- cannot be expressed without a
	//                     host rule; a Phase 2 diagnostic will reject it.
	// GenerateExpression and GetPythonPrecedence both dispatch on this so the emitted
	// form and its parenthesization stay in sync.
	enum class AndOrLowering { RuleOp, PythonOp, MixedTernary, Unrepresentable };
	AndOrLowering ClassifyAndOr(const rls::ast::BinaryExpr& node) const;

	// True if `node` is a rule-conditioned ternary with rule branches, which lowers to the
	// `(C & a) | b` rule idiom rather than a Python `if`. Shared by GenerateExpression and
	// GetPythonPrecedence so the emitted form and its precedence stay in sync.
	bool isRuleConditionedRuleTernary(const rls::ast::TernaryExpr& node) const;

	int GetPythonPrecedence(const rls::ast::ExprPtr& expr) const;
	std::string GenerateChildExpression(const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false) const;
	std::string GenerateExpression(const rls::ast::BoolLiteral& node) const;
	std::string GenerateExpression(const rls::ast::IntLiteral& node) const;
	std::string GenerateExpression(const rls::ast::Identifier& node) const;
	std::string GenerateExpression(const rls::ast::UnaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::BinaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::TernaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::CallExpr& node) const;
	std::string GenerateExpression(const rls::ast::InvokeExpr& node) const;

	// The declared type of parameter `index` of the function `node` calls, looked up from
	// the extern/define decl. std::nullopt if the callee or parameter cannot be resolved.
	std::optional<rls::ast::Type> ResolveCallParamType(const rls::ast::CallExpr& node, size_t index) const;

	// Render one call argument, accounting for Condition parameters: a non-Condition argument
	// bound to a Condition parameter is wrapped in a `(lambda <ctx>: <expr>)` thunk so it is
	// evaluated lazily; an argument already of Condition type is passed through unchanged.
	std::string GenerateCallArgument(const rls::ast::Expr* argExpr, std::optional<rls::ast::Type> paramType) const;

	std::string GenerateExpression(const rls::ast::SharedBlock& node) const;
	std::string GenerateExpression(const rls::ast::MatchExpr& node) const;

	// True if this binary expression is a `setting(KEY) == VALUE` / `!= VALUE` comparison,
	// which is emitted as an atomic OptionFilter rule rather than a Python comparison.
	bool IsSettingComparison(const rls::ast::BinaryExpr& node) const;

	// Convert setting(KEY) == VALUE expressions to OptionFilter(...) form for RuleBuilder.
	// Returns empty string if not a setting comparison; caller uses the fallback.
	std::string TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const;

	// Render a setting comparison as an OptionFilter rule (precondition: IsSettingComparison).
	// When `negate`, renders its negation by flipping eq <-> "ne". Shared by the positive
	// path (TryGenerateOptionFilter) and the De Morgan negation.
	std::string renderSettingOptionFilter(const rls::ast::BinaryExpr& node, bool negate) const;

	// Wraps an OptionFilter argument list as a standalone RuleBuilder rule:
	// `True_(options=[OptionFilter(<args>)])`. A bare OptionFilter is not a Rule and cannot
	// combine with another OptionFilter via & / |, so every setting comparison is wrapped in
	// its own rule. This keeps each comparison a valid standalone rule that composes normally.
	std::string WrapOptionFilter(const std::string& optionFilterArgs) const;

	// == Negation of pure option-filter rules =================================
	// The RuleBuilder has no rule negation in general, but a rule built entirely from
	// `setting(...)` comparisons resolves at *build time* against `world.options`, so it can
	// be negated soundly by pushing `not` down via De Morgan and flipping each leaf
	// (eq <-> "ne"). This is what lets `not is_fire_loop_locked()` lower without the source
	// having to reverse the logic by hand. Collection rules (has/can_use/...) are never pure,
	// so `not` over them stays a diagnostic.

	// True iff `expr` resolves entirely from build-time settings: a setting comparison, a
	// bool literal, `and`/`or`/`not` of such, or a call to a define whose body is such.
	// Memoized over defines with a cycle guard. Anything touching a collection/host rule is
	// impure (returns false).
	bool IsPureOptionFilterRule(const rls::ast::Expr* expr) const;
	bool IsPureOptionFilterRule(const rls::ast::ExprPtr& expr) const;
	mutable std::map<const rls::ast::DefineDecl*, bool> pureOptionFilterCache;
	mutable std::set<const rls::ast::DefineDecl*> pureOptionFilterInProgress;

	// Renders the negation of a pure option-filter rule (precondition:
	// IsPureOptionFilterRule(expr)). De Morgan dual: `and`->`|` of negations, `or`->`&` of
	// negations, a setting leaf flips eq<->"ne", `not x` returns x's positive rendering, and
	// a define call inlines its negated body.
	std::string GenerateNegatedOptionFilterRule(const rls::ast::ExprPtr& expr) const;
	// Precedence of the form GenerateNegatedOptionFilterRule emits (the De Morgan dual swaps
	// and/or), so parenthesization of a negated rule stays in sync with its rendering.
	int NegatedPrecedence(const rls::ast::ExprPtr& expr) const;
	// As GenerateChildExpression, but for a negated child: wraps using NegatedPrecedence.
	std::string GenerateNegatedChild(const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false) const;
};

} // namespace rls::transpilers::ap
