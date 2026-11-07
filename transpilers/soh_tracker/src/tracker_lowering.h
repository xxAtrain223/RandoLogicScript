#pragma once

#include "tracker_roots.h"

namespace rls::transpilers::soh_tracker::ir {

// Appends occurrence-specific descriptors using only established sema facts.
class ExpressionLowerer {
public:
	ExpressionLowerer(const ast::Project& project, Graph& graph,
		std::vector<ast::Diagnostic>& diagnostics);

	std::optional<NodeHandle> Lower(const ast::Expr& expression,
		const ast::DefineDecl* scope = nullptr);
	std::optional<NodeHandle> Convert(NodeHandle operand, IntegralTarget target,
		const ast::Expr& expression);

private:
	const ast::Project& project;
	Graph& graph;
	std::vector<ast::Diagnostic>& diagnostics;

	NodeHandle Add(Payload payload, std::optional<ValueType> type, Provenance provenance);
	std::optional<NodeHandle> Fail(const ast::Expr& expression, std::string message);
	std::optional<NodeHandle> Child(const ast::ExprPtr& expression,
		const ast::Expr& parent, const ast::DefineDecl* scope);
	std::optional<NodeHandle> Binary(const ast::Expr& expression,
		const ast::BinaryExpr& binary, const ValueType& type, const ast::DefineDecl* scope);
	std::optional<NodeHandle> Conditional(const ast::Expr& expression,
		const ast::TernaryExpr& ternary, const ValueType& type, const ast::DefineDecl* scope);
	std::optional<NodeHandle> MatchExpression(const ast::Expr& expression,
		const ast::MatchExpr& match, const ValueType& type, const ast::DefineDecl* scope);
	std::optional<NodeHandle> Coerce(NodeHandle operand, const ValueType& target,
		const ast::Expr& expression);
	std::optional<NodeHandle> Call(const ast::Expr& expression,
		const ast::CallExpr& call, const ValueType& type, const ast::DefineDecl* scope);
	std::optional<NodeHandle> CallHelper(const ast::Expr& expression,
		const ast::DefineDecl& declaration, const ValueType& type,
		std::vector<NodeHandle> parameters);
	NodeHandle CallExtern(const ast::Expr& expression,
		const ast::ExternDefineDecl& declaration, const ValueType& type,
		std::vector<NodeHandle> parameters, std::vector<ValueType> parameterTypes);
	std::optional<NodeHandle> FunctionReference(const ast::Expr& expression,
		const ast::Identifier& identifier, const ValueType& type);
	std::optional<NodeHandle> InvokeCallable(NodeHandle callable,
		const ast::Expr& expression, const ValueType& type);
};

using LoweringResult = std::variant<Graph, std::vector<ast::Diagnostic>>;
LoweringResult LowerRoots(const ast::Project& project, const std::filesystem::path& sourceRoot);

} // namespace rls::transpilers::soh_tracker::ir
