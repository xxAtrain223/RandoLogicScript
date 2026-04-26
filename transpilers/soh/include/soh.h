#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh {

class SohTranspiler {
public:
	explicit SohTranspiler(const rls::ast::Project& project);

	void Transpile(rls::OutputWriter& out) const;

	void GenerateRuntimeHeaders(rls::OutputWriter& out) const;
	void GenerateFunctionDefinitionsHeader(rls::OutputWriter& out) const;
	void GenerateFunctionDefinitionsSource(rls::OutputWriter& out) const;
	void GenerateRegionsHeader(rls::OutputWriter& out) const;
	void GenerateRegionsSource(rls::OutputWriter& out) const;
	std::string GenerateExpression(const rls::ast::ExprPtr& expr) const;

private:
	int GetCppPrecedence(const rls::ast::ExprPtr& expr) const;
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

	const rls::ast::Project& project;
};

void Transpile(const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh
