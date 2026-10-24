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
	if (scope && std::any_of(scope->params.begin(), scope->params.end(),
		[&](const ast::Param& parameter) { return parameter.name.text == call.callee.text; })) {
		// Lexical callable parameters shadow global declarations, just as they do in sema.
		return Fail(expression, "Tracker callable-parameter invocation lowering is not implemented yet.");
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
	const auto existing = std::find_if(graph.adapters.begin(), graph.adapters.end(),
		[&](const HostAdapter& adapter) { return adapter.declaration == external; });
	const AdapterHandle adapter{static_cast<size_t>(existing - graph.adapters.begin())};
	if (existing == graph.adapters.end()) {
		// The call's analyzed result supplies the signature; no annotation parsing or host-width guess.
		graph.adapters.push_back({external, type, std::move(parameterTypes)});
	}
	return Add(ExternCall{adapter, std::move(parameters)}, type,
		{expression.span, {&expression}});
}

} // namespace rls::transpilers::soh_tracker::ir
