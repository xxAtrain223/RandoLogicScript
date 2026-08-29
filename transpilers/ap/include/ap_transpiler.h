#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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

	// Python expression for the world's options dataclass, reached from a rule lambda's
	// receiver -- passed to OptionFilter.check() to evaluate a setting comparison as a
	// build-time bool. How a lambda reaches world.options is world-specific (it depends on the
	// receiver's shape), so the default is empty, which disables the build-time-setting lowering
	// (a setting-conditioned ternary then falls back to a diagnostic). A world overrides this to
	// enable it -- e.g. SoH's bundle is `(region, world)`, so it returns "bundle[1].options".
	virtual std::string ruleContextOptions() const;

	// Render an enum-value identifier (e.g. RG_HOOKSHOT) to its Python form, given the
	// name of the enum it belongs to (e.g. "Item") and the value name. Enums are keyed by
	// name rather than by ast::Type: every enum shares the single Type::Enum, and the
	// identity lives beside it in project.getEnumType(node).
	// Default: the bare value name. Override to add world enum-class prefixes
	// and value overrides (e.g. RO_GENERIC_YES -> "True").
	virtual std::string renderEnumValue(std::string_view enumName, const std::string& value) const;

	// Rewrite a host/builtin call (has, flag, trick, ...) to Python. Default:
	// std::nullopt, so the core emits the default call form `callee(args...)`
	// (with the ruleContextParam() receiver prepended when one is set).
	// `overrideIdx`/`overrideExpr` let the ternary distribution re-render the call with one
	// argument replaced (see renderCall); when overrideIdx is std::string::npos there is no
	// override and every argument comes from the resolved call args as usual.
	virtual std::optional<std::string> renderHostCall(const rls::ast::CallExpr& node,
		size_t overrideIdx = std::string::npos, const rls::ast::Expr* overrideExpr = nullptr) const;

	// Rewrite a world-specific binary special case. Default: std::nullopt
	// (normal binary-operator handling).
	virtual std::optional<std::string> renderBinarySpecialCase(const rls::ast::BinaryExpr& node) const;

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
	// Python type name for an RLS type, used in generated function signatures. For
	// Type::Enum, `enumName` carries which enum it is (from project.getEnumType); it is
	// std::nullopt for every other type.
	virtual std::string pythonTypeName(
		rls::ast::Type type, std::optional<std::string_view> enumName) const = 0;

	const rls::ast::Project& project;
	mutable std::optional<std::string> currentLocationName;

	// Record an error diagnostic for an unrepresentable construct at `span`.
	void Diagnose(const rls::ast::Span& span, std::string message) const;

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

private:
	mutable std::vector<rls::ast::Diagnostic> diagnostics;

	// The value class of a call, by its callee's return semantics (setting/define/extern).
	// Shared by ClassifyExpression and the ternary distribution guard so both agree on which
	// calls produce a Rule.
	ValueClass ClassifyCall(const rls::ast::CallExpr& call) const;

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

	// True if `node` is a rule-conditioned ternary with rule branches, which lowers to a
	// renderConditionalRule(...) host call rather than a Python `if`. Shared by
	// GenerateExpression and GetPythonPrecedence so the emitted form and its precedence stay
	// in sync.
	bool isRuleConditionedRuleTernary(const rls::ast::TernaryExpr& node) const;

	int GetPythonPrecedence(const rls::ast::ExprPtr& expr) const;
	std::string GenerateChildExpression(const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false) const;
	std::string GenerateExpression(const rls::ast::BoolLiteral& node) const;
	std::string GenerateExpression(const rls::ast::IntLiteral& node) const;
	std::string GenerateExpression(const rls::ast::StringLiteral& node) const;
	std::string GenerateExpression(const rls::ast::ListExpr& node) const;
	std::string GenerateExpression(const rls::ast::Identifier& node) const;
	// `EnumName.ValueName` -- the dotted form disambiguating a value shared by two enums.
	std::string GenerateExpression(const rls::ast::MemberExpr& node) const;
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

	// Dispatch a call to its emitted form: the setting truthiness check, a world host-call
	// rewrite (renderHostCall), or the default `<callee>(<ctx>, <args>...)` form. `overrideIdx`/
	// `overrideExpr` replace one resolved argument (used when distributing a call over a ternary's
	// branches -- each branch re-renders through this same dispatch so host rewrites still apply).
	// Precondition: node has resolved call args.
	std::string renderCall(const rls::ast::CallExpr& node, size_t overrideIdx,
		const rls::ast::Expr* overrideExpr) const;

	// Render a plain function call `<callee>(<ctx>, <args>...)`, threading the rule-context
	// receiver. If `overrideIdx` is a valid argument index, `overrideExpr` is generated in place
	// of the resolved argument there -- used to distribute a call over a ternary's branches.
	// Precondition: node has resolved call args.
	std::string renderDefaultCall(const rls::ast::CallExpr& node, size_t overrideIdx,
		const rls::ast::Expr* overrideExpr) const;

	// If a call argument is a rule-conditioned ternary whose branches are (non-rule) build-time
	// values -- e.g. `can_use(is_adult() ? RG_HOOKSHOT : RG_LONGSHOT)` -- the branches cannot be
	// &/|-combined with the rule condition, so the ternary is not directly representable. Instead
	// we lift the call over the ternary: `f(.., C ? A : B, ..)` becomes
	// `rls_conditional(<ctx>, C, f(..,A,..), f(..,B,..))`, a solve-time pick between the two rules
	// that mirrors the source ternary exactly (see renderConditionalRule). Returns the distributed
	// call when such an argument exists, else std::nullopt (the caller renders the call normally).
	// A build-time or pure-setting condition is left alone (it stays an ordinary Python `if`), as
	// is a runtime non-rule condition (which remains a diagnosed, unrepresentable value).
	std::optional<std::string> tryDistributeTernaryArg(const rls::ast::CallExpr& node) const;

	// Render a conditional (if-then-else) rule `rls_conditional(<ctx>, <cond>, <then>, <else>)`: a
	// host rule that evaluates <cond> at solve time and picks <then> or <else> accordingly. This
	// is the faithful lowering of every rule-conditioned ternary -- rule branches directly, value
	// branches after distributing the enclosing call. Unlike the `(C & a) | b` idiom it replaced
	// it does not ungate the else-branch, and it needs no rule negation.
	std::string renderConditionalRule(const std::string& cond, const std::string& thenExpr,
		const std::string& elseExpr) const;

	std::string GenerateExpression(const rls::ast::HereRef& node) const;
	std::string GenerateExpression(const rls::ast::MatchExpr& node) const;

	// The `setting(KEY) == VALUE` / `!= VALUE` shape: the resolved RSK_* key identifier and the
	// value it is compared against. Matched in EITHER operand order, and the value may be any
	// build-time form -- a bare enum value, the dotted `Setting.RO_X` form, an int literal, a
	// parameter -- so every way the source can spell the comparison reaches the OptionFilter
	// lowering. std::nullopt when the node is not a setting comparison.
	struct SettingComparison {
		const rls::ast::Identifier* key;
		const rls::ast::Expr* value;
	};
	std::optional<SettingComparison> MatchSettingComparison(const rls::ast::BinaryExpr& node) const;

	// True if this binary expression is a `setting(KEY) == VALUE` / `!= VALUE` comparison,
	// which is emitted as an atomic OptionFilter rule rather than a Python comparison.
	bool IsSettingComparison(const rls::ast::BinaryExpr& node) const;

	// True if either operand is a direct `setting(...)` call. An OptionFilter tests a setting for
	// equality only, so any *other* operation over one (an ordered comparison, arithmetic, a
	// comparison against a non-build-time value) has no lowering: it would emit a raw Python
	// operation against a Rule object -- silently False for ==/!=, a TypeError for </>. Used by
	// GenerateExpression(BinaryExpr) to diagnose those instead of emitting them.
	bool HasSettingOperand(const rls::ast::BinaryExpr& node) const;

	// Diagnose an expression that cannot stand alone at the top of a generated rule. A Runtime
	// value (bottle_count() >= 1, a state-dependent Int) is neither a Rule nor frozen at build
	// time, so emitting it yields a lambda returning a plain Python value computed once against
	// the empty initial collection state -- a silent miscompile. The operator paths (and/or,
	// ternary conditions) diagnose their own operands; this covers the TOP of each region entry
	// condition and generated function body, where nothing else looks.
	void DiagnoseTopLevelValue(const rls::ast::ExprPtr& expr) const;

	// Convert setting(KEY) == VALUE expressions to OptionFilter(...) form for RuleBuilder.
	// Returns empty string if not a setting comparison; caller uses the fallback.
	std::string TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const;

	// Render a setting comparison as an OptionFilter rule (precondition: IsSettingComparison).
	// When `negate`, renders its negation by flipping eq <-> "ne". Shared by the positive
	// path (TryGenerateOptionFilter) and the De Morgan negation.
	std::string renderSettingOptionFilter(const rls::ast::BinaryExpr& node, bool negate) const;

	// The `<key>, <value>[, "ne"]` argument list for an OptionFilter (precondition:
	// IsSettingComparison). `negate` flips eq <-> "ne". Shared by the rule-wrapping form
	// (renderSettingOptionFilter) and the build-time .check() form (renderSettingCheck).
	std::string optionFilterArgs(const rls::ast::BinaryExpr& node, bool negate) const;

	// Render a setting comparison as a build-time bool: `OptionFilter(<args>).check(<options>)`.
	// Precondition: IsSettingComparison(node) and a non-empty ruleContextOptions(). Unlike the
	// rule form, this yields a plain Python bool usable as a ternary condition.
	std::string renderSettingCheck(const rls::ast::BinaryExpr& node) const;

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

	// == Build-time evaluation of settings ====================================
	// A pure option-filter expression resolves entirely from `world.options`, which is frozen
	// at generation. So in a *build-time boolean* position (a ternary condition, where a Rule
	// cannot go because `bool(rule)` raises) it lowers to plain Python over
	// `OptionFilter(...).check(<options>)` calls, rather than the OptionFilter-attached rule
	// form used when a setting comparison combines *with* rules via & / |.

	// True iff `cond` can be evaluated as a build-time bool here: it is a pure option-filter
	// expression and the world exposes an options accessor (ruleContextOptions() non-empty).
	bool isBuildTimeSettingCondition(const rls::ast::ExprPtr& cond) const;

	// Render a pure option-filter expression as a build-time Python bool (precondition:
	// isBuildTimeSettingCondition(expr)): setting leaves become OptionFilter(...).check(...),
	// and `and`/`or`/`not`/pure-define nodes become the matching Python boolean operators.
	std::string GenerateBuildTimeSettingCondition(const rls::ast::ExprPtr& expr) const;
	// As GenerateChildExpression, but for a negated child: wraps using NegatedPrecedence.
	std::string GenerateNegatedChild(const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false) const;
};

} // namespace rls::transpilers::ap
