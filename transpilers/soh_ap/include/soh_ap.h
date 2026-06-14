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
	
	// Helper to convert setting(KEY) == VALUE expressions to OptionFilter(...) form for RuleBuilder.
	// Returns empty string if not a setting comparison; caller should use fallback.
	std::string TryGenerateOptionFilter(const rls::ast::BinaryExpr& node) const;

	const rls::ast::Project& project;
};

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh_ap
