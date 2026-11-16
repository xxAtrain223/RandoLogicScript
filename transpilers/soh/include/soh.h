#pragma once

#include "ast.h"
#include "output.h"

namespace rls::transpilers::soh {

class SohTranspiler {
public:
	explicit SohTranspiler(const rls::ast::Project& project);

	std::vector<rls::ast::Diagnostic> Validate() const;
	std::vector<rls::ast::Diagnostic> Transpile(rls::OutputWriter& out) const;

	void GenerateRuntimeHeaders(rls::OutputWriter& out) const;
	void GenerateFunctionDefinitionsHeader(rls::OutputWriter& out) const;
	void GenerateFunctionDefinitionsSource(rls::OutputWriter& out) const;
	void GenerateRegionsHeader(rls::OutputWriter& out) const;
	std::vector<rls::ast::Diagnostic> GenerateRegionsSource(rls::OutputWriter& out) const;
	std::string GenerateExpression(const rls::ast::ExprPtr& expr) const;
	std::string GenerateBoolExpression(const rls::ast::ExprPtr& expr) const;

private:
	int GetCppPrecedence(const rls::ast::ExprPtr& expr) const;
	int GetCppPrecedence(const rls::ast::Expr* expr) const;
	std::string GenerateChildExpression(const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false) const;
	std::string GenerateBoolExpression(const rls::ast::Expr* expr) const;
	std::string GenerateBoolChildExpression(const rls::ast::ExprPtr& expr, int parentPrec, bool isRightChild = false) const;
	std::string GenerateExpression(const rls::ast::BoolLiteral& node) const;
	std::string GenerateExpression(const rls::ast::IntLiteral& node) const;
	std::string GenerateExpression(const rls::ast::StringLiteral& node) const;
	std::string GenerateExpression(const rls::ast::Identifier& node) const;
	std::string GenerateExpression(const rls::ast::UnaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::BinaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::TernaryExpr& node) const;
	std::string GenerateExpression(const rls::ast::CallExpr& node) const;
	std::string GenerateExpression(const rls::ast::InvokeExpr& node) const;
	std::optional<rls::ast::Type> ResolveCallParamType(const rls::ast::CallExpr& node, size_t index) const;
	std::optional<std::string> ResolveCallParamEnumCppType(const rls::ast::CallExpr& node, size_t index) const;
	std::string GenerateCallArgument(const rls::ast::Expr* argExpr, std::optional<rls::ast::Type> paramType, std::optional<std::string> paramEnumCppType) const;
	std::string GenerateExpression(const rls::ast::MemberExpr& node) const;
	std::string GenerateExpression(const rls::ast::HereRef& node) const;
	std::string GenerateExpression(const rls::ast::MatchExpr& node, const rls::ast::Expr& expr) const;
	std::string GenerateExpression(const rls::ast::ListExpr& node) const;
	std::string GenerateExpression(const rls::ast::Expr* expr) const;
	void WriteRegionsSource(rls::OutputWriter& out) const;

	const rls::ast::Project& project;
};

std::vector<rls::ast::Diagnostic> Transpile(
	const rls::ast::Project& project, rls::OutputWriter& out);

} // namespace rls::transpilers::soh
