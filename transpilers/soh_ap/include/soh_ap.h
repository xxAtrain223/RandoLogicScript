#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh_ap {

class SohApTranspiler {
public:
	explicit SohApTranspiler(const rls::ast::Project& project);

	void Transpile(rls::OutputWriter& out) const;

	void GenerateFunctionDefinitionsSource(rls::OutputWriter& out) const;
	void GenerateRegionsSource(rls::OutputWriter& out) const;
	void GenerateEnumsSource(rls::OutputWriter& out) const;
	std::string GenerateExpression(const rls::ast::ExprPtr& expr) const;
    void SetCurrentLocation(std::optional<std::string> location) const;

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
	std::string GenerateExpression(const rls::ast::Expr::Variant& node) const;
	
	// True if this binary expression is a `setting(KEY) == VALUE` / `!= VALUE` comparison,
	// which is emitted as an atomic OptionFilter rule rather than a Python comparison.
	bool IsSettingComparison(const rls::ast::BinaryExpr& node) const;

	// Helper to convert setting(KEY) == VALUE expressions to OptionFilter(...) form for RuleBuilder.
	// Returns empty string if not a setting comparison; caller should use fallback.
	std::string TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const;

	// Wraps an OptionFilter argument list as a standalone RuleBuilder rule:
	// `True_(options=[OptionFilter(<args>)])`. A bare OptionFilter is not a Rule and cannot
	// combine with another OptionFilter via & / |, so every setting comparison is wrapped in
	// its own rule. This keeps each comparison a valid standalone rule that composes normally.
	std::string WrapOptionFilter(const std::string& optionFilterArgs) const;

	const rls::ast::Project& project;
	mutable std::optional<std::string> currentLocationName;
};

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh_ap
