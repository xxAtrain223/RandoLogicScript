#pragma once

#include <optional>
#include <string>

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
	virtual std::string renderAnyAgeBlock(const rls::ast::AnyAgeBlock& node) const;

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

private:
	int GetPythonPrecedence(const rls::ast::ExprPtr& expr) const;
	std::string GenerateChildExpression(const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false) const;
	std::string GenerateExpression(const rls::ast::BoolLiteral& node) const;
	std::string GenerateExpression(const rls::ast::IntLiteral& node) const;
	std::string GenerateExpression(const rls::ast::Identifier& node) const;
	std::string GenerateExpression(const rls::ast::UnaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::BinaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::TernaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::CallExpr& node) const;
	std::string GenerateExpression(const rls::ast::SharedBlock& node) const;
	std::string GenerateExpression(const rls::ast::AnyAgeBlock& node) const;
	std::string GenerateExpression(const rls::ast::MatchExpr& node) const;

	// True if this binary expression is a `setting(KEY) == VALUE` / `!= VALUE` comparison,
	// which is emitted as an atomic OptionFilter rule rather than a Python comparison.
	bool IsSettingComparison(const rls::ast::BinaryExpr& node) const;

	// Convert setting(KEY) == VALUE expressions to OptionFilter(...) form for RuleBuilder.
	// Returns empty string if not a setting comparison; caller uses the fallback.
	std::string TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const;

	// Wraps an OptionFilter argument list as a standalone RuleBuilder rule:
	// `True_(options=[OptionFilter(<args>)])`. A bare OptionFilter is not a Rule and cannot
	// combine with another OptionFilter via & / |, so every setting comparison is wrapped in
	// its own rule. This keeps each comparison a valid standalone rule that composes normally.
	std::string WrapOptionFilter(const std::string& optionFilterArgs) const;
};

} // namespace rls::transpilers::ap
