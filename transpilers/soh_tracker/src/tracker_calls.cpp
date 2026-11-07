#include "tracker_lowering.h"

#include <algorithm>

namespace rls::transpilers::soh_tracker::ir {

std::optional<NodeHandle> ExpressionLowerer::Coerce(NodeHandle operand,
	const ValueType& target, const ast::Expr& expression)
{
	if (const auto* integral = std::get_if<IntegralTarget>(&target.representation)) {
		return Convert(operand, *integral, expression);
	}
	const auto& source = graph.nodes.at(operand.slot).valueType;
	if (!source) {
		return Fail(expression, "Tracker condition conversion requires a value expression.");
	}
	if (std::holds_alternative<ConditionTarget>(source->representation)) {
		return operand;
	}
	if (source->sourceType != ast::Type::Bool && source->sourceType != ast::Type::Int) {
		return Fail(expression, "Tracker lazy condition requires a Bool-compatible expression.");
	}
	const auto body = Convert(operand, IntegralTarget::Bool, expression);
	if (!body) return std::nullopt;
	// This node captures lexical scope at runtime; lowering never samples live game state.
	return Add(Condition{*body}, target, {expression.span, {&expression}});
}

std::optional<NodeHandle> ExpressionLowerer::CallHelper(const ast::Expr& expression,
	const ast::DefineDecl& declaration, const ValueType& type,
	std::vector<NodeHandle> parameters)
{
	const auto previousCount = graph.helpers.size();
	const auto definition = graph.RegisterHelper(declaration);
	if (definition.slot == previousCount) {
		// Publish the definition handle first, so recursive references do not re-lower its body.
		if (!declaration.body) {
			return Fail(expression, "Tracker helper lowering requires a body for '" +
				declaration.name.text + "'.");
		}
		const auto bodyType = GetValueType(project, *declaration.body);
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&bodyType)) {
			diagnostics.push_back(*diagnostic);
			return std::nullopt;
		}
		auto body = Lower(*declaration.body, &declaration);
		if (!body) return std::nullopt;
		body = Coerce(*body, std::get<ValueType>(bodyType), *declaration.body);
		if (!body) return std::nullopt;
		// Recursive lowering may have grown helpers; do not retain references into the vector.
		graph.helpers[definition.slot].body = *body;
	}
	return Add(HelperCall{definition, std::move(parameters)}, type,
		{expression.span, {&expression}});
}

std::optional<NodeHandle> ExpressionLowerer::Call(const ast::Expr& expression,
	const ast::CallExpr& call, const ValueType& type, const ast::DefineDecl* scope)
{
	if (scope) {
		const auto parameter = std::find_if(scope->params.begin(), scope->params.end(),
			[&](const ast::Param& candidate) { return candidate.name.text == call.callee.text; });
		if (parameter != scope->params.end()) {
			// Lexical callable parameters shadow global declarations, just as they do in sema.
			const auto* resolved = project.getResolvedCallArgs(&call);
			if (!resolved || !resolved->empty() || !call.args.empty()) {
				return Fail(expression, "Tracker callable-parameter invocation requires zero sema-resolved arguments.");
			}
			const auto parameterType = GetValueType(project, *parameter);
			if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&parameterType)) {
				diagnostics.push_back(*diagnostic);
				return std::nullopt;
			}
			const auto valueType = std::get<ValueType>(parameterType);
			if (!std::holds_alternative<ConditionTarget>(valueType.representation)) {
				return Fail(expression, "Tracker callable-parameter invocation requires a Condition value.");
			}
			const auto callable = Add(Value{ParameterRead{&*parameter}}, valueType,
				{expression.span, {&expression}});
			return InvokeCallable(callable, expression, type);
		}
	}

	const ast::ExternDefineDecl* external = nullptr;
	const ast::DefineDecl* helper = nullptr;
	if (const auto declaration = project.ExternDefineDecls.find(call.callee.text);
		declaration != project.ExternDefineDecls.end()) {
		external = declaration->second;
	} else if (const auto declaration = project.DefineDecls.find(call.callee.text);
		declaration != project.DefineDecls.end()) {
		helper = declaration->second;
	}
	if (!external && !helper) {
		return Fail(expression, "Tracker call requires an analyzed declaration for '" +
			call.callee.text + "'.");
	}
	const auto& declarations = external ? external->params : helper->params;
	const auto* resolved = project.getResolvedCallArgs(&call);
	if (!resolved || resolved->size() != declarations.size()) {
		return Fail(expression, "Tracker call requires complete sema-resolved arguments for '" +
			call.callee.text + "'.");
	}
	if (!declarations.empty()) {
		const auto position = CheckParameterPosition(declarations.size() - 1, expression.span);
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&position)) {
			diagnostics.push_back(*diagnostic);
			return std::nullopt;
		}
	}

	std::vector<ValueType> parameterTypes;
	std::vector<NodeHandle> parameters;
	for (size_t i = 0; i < declarations.size(); ++i) {
		const auto* argument = (*resolved)[i];
		if (!argument) {
			return Fail(expression, "Tracker call has a missing sema-resolved argument for '" +
				declarations[i].name.text + "'.");
		}
		const auto parameterType = GetValueType(project, declarations[i]);
		if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&parameterType)) {
			diagnostics.push_back(*diagnostic);
			return std::nullopt;
		}
		const auto target = std::get<ValueType>(parameterType);
		// Sema resolves defaults globally; caller parameters must not shadow calls within them.
		const auto* argumentScope = argument == declarations[i].defaultValue.get() ? nullptr : scope;
		auto value = Lower(*argument, argumentScope);
		if (!value) return std::nullopt;
		value = Coerce(*value, target, *argument);
		if (!value) return std::nullopt;
		parameters.push_back(Add(Parameter{static_cast<uint8_t>(i), &declarations[i], *value},
			target, {argument->span, {argument}}));
		parameterTypes.push_back(target);
	}

	if (helper) {
		return CallHelper(expression, *helper, type, std::move(parameters));
	}
	return CallExtern(expression, *external, type, std::move(parameters), std::move(parameterTypes));
}

NodeHandle ExpressionLowerer::CallExtern(const ast::Expr& expression,
	const ast::ExternDefineDecl& declaration, const ValueType& type,
	std::vector<NodeHandle> parameters, std::vector<ValueType> parameterTypes)
{
	const auto existing = std::find_if(graph.adapters.begin(), graph.adapters.end(),
		[&](const HostAdapter& adapter) { return adapter.declaration == &declaration; });
	const AdapterHandle adapter{static_cast<size_t>(existing - graph.adapters.begin())};
	if (existing == graph.adapters.end()) {
		// Analyzed calls/references supply RLS representations, never guessed host widths.
		graph.adapters.push_back({&declaration, type, std::move(parameterTypes)});
	}
	return Add(ExternCall{adapter, std::move(parameters)}, type,
		{expression.span, {&expression}});
}

std::optional<NodeHandle> ExpressionLowerer::InvokeCallable(NodeHandle callable,
	const ast::Expr& expression, const ValueType& type)
{
	const auto& callableType = graph.nodes.at(callable.slot).valueType;
	if (!callableType || !std::holds_alternative<ConditionTarget>(callableType->representation)) {
		return Fail(expression, "Tracker invocation requires a Condition value.");
	}
	if (type.sourceType != ast::Type::Bool) {
		return Fail(expression, "Tracker invocation requires an analyzed Bool result.");
	}
	return Add(Invoke{callable}, type, {expression.span, {&expression}});
}

std::optional<NodeHandle> ExpressionLowerer::FunctionReference(const ast::Expr& expression,
	const ast::Identifier& identifier, const ValueType& type)
{
	if (type.sourceType != ast::Type::Condition) {
		return Fail(expression, "Tracker function reference requires an analyzed Condition type.");
	}
	// FunctionRef is sema's certification of a zero-parameter Bool function. Do not
	// synthesize a CallExpr: graph provenance must reference the original, persistent AST.
	const ValueType boolType{ast::Type::Bool, nullptr, IntegralTarget::Bool};
	std::optional<NodeHandle> body;
	if (const auto declaration = project.DefineDecls.find(identifier.name.text);
		declaration != project.DefineDecls.end() && declaration->second) {
		const auto& helper = *declaration->second;
		if (!helper.params.empty() || !helper.body ||
			project.getType(helper.body.get()) != ast::Type::Bool) {
			return Fail(expression, "Tracker function reference requires a zero-parameter Bool helper.");
		}
		body = CallHelper(expression, helper, boolType, {});
	} else if (const auto declaration = project.ExternDefineDecls.find(identifier.name.text);
		declaration != project.ExternDefineDecls.end() && declaration->second) {
		const auto& external = *declaration->second;
		if (!external.params.empty() || !external.returnType || external.returnType->name != "Bool") {
			return Fail(expression, "Tracker function reference requires a zero-parameter Bool extern.");
		}
		body = CallExtern(expression, external, boolType, {}, {});
	} else {
		return Fail(expression, "Tracker function reference requires an analyzed declaration for '" +
			identifier.name.text + "'.");
	}
	if (!body) return std::nullopt;
	return Add(Condition{*body}, type, {expression.span, {&expression}});
}

} // namespace rls::transpilers::soh_tracker::ir
