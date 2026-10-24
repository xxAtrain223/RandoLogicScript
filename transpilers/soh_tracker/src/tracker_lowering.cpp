#include "tracker_lowering.h"

#include <type_traits>

namespace rls::transpilers::soh_tracker::ir {
namespace {

bool isLogical(ast::BinaryOp op) {
	return op == ast::BinaryOp::And || op == ast::BinaryOp::Or;
}

std::optional<ArithmeticOperator> arithmeticOperator(ast::BinaryOp op) {
	switch (op) {
	case ast::BinaryOp::Add: return ArithmeticOperator::Add;
	case ast::BinaryOp::Sub: return ArithmeticOperator::Subtract;
	case ast::BinaryOp::Mul: return ArithmeticOperator::Multiply;
	case ast::BinaryOp::Div: return ArithmeticOperator::Divide;
	default: return std::nullopt;
	}
}

std::optional<ComparisonOperator> comparisonOperator(ast::BinaryOp op) {
	switch (op) {
	case ast::BinaryOp::Eq: return ComparisonOperator::Equal;
	case ast::BinaryOp::NotEq: return ComparisonOperator::NotEqual;
	case ast::BinaryOp::Lt: return ComparisonOperator::Less;
	case ast::BinaryOp::LtEq: return ComparisonOperator::LessEqual;
	case ast::BinaryOp::Gt: return ComparisonOperator::Greater;
	case ast::BinaryOp::GtEq: return ComparisonOperator::GreaterEqual;
	default: return std::nullopt;
	}
}

} // namespace

ExpressionLowerer::ExpressionLowerer(const ast::Project& project, Graph& graph,
	std::vector<ast::Diagnostic>& diagnostics)
	: project(project), graph(graph), diagnostics(diagnostics) {}

NodeHandle ExpressionLowerer::Add(Payload payload, std::optional<ValueType> type,
	Provenance provenance)
{
	// Slot handles survive vector growth; final consumer IDs are assigned separately.
	const NodeHandle handle{graph.nodes.size()};
	graph.nodes.push_back({std::move(payload), type, std::move(provenance)});
	return handle;
}

std::optional<NodeHandle> ExpressionLowerer::Fail(const ast::Expr& expression,
	std::string message)
{
	diagnostics.push_back({"", expression.span, ast::DiagnosticLevel::Error, std::move(message)});
	return std::nullopt;
}

std::optional<NodeHandle> ExpressionLowerer::Child(const ast::ExprPtr& expression,
	const ast::Expr& parent, const ast::DefineDecl* scope)
{
	if (!expression) {
		return Fail(parent, "Tracker lowering requires a complete expression.");
	}
	return Lower(*expression, scope);
}

std::optional<NodeHandle> ExpressionLowerer::Convert(NodeHandle operand,
	IntegralTarget target, const ast::Expr& expression)
{
	const auto& type = graph.nodes.at(operand.slot).valueType;
	if (!type || !std::holds_alternative<IntegralTarget>(type->representation)) {
		return Fail(expression, "Tracker integral conversion cannot convert a Condition or structural node.");
	}
	if (std::get<IntegralTarget>(type->representation) == target) {
		return operand;
	}
	// Keep normalization explicit in the graph without changing the borrowed AST.
	const ValueType converted{target == IntegralTarget::Bool ? ast::Type::Bool : ast::Type::Int,
		nullptr, target};
	return Add(Conversion{target, operand}, converted, {expression.span, {&expression}});
}

std::optional<NodeHandle> ExpressionLowerer::Lower(const ast::Expr& expression,
	const ast::DefineDecl* scope)
{
	// Sema owns source typing and enum identity; lowering only selects runtime nodes.
	const auto result = GetValueType(project, expression);
	if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&result)) {
		diagnostics.push_back(*diagnostic);
		return std::nullopt;
	}
	const auto type = std::get<ValueType>(result);
	const Provenance provenance{expression.span, {&expression}};
	return std::visit([&](const auto& node) -> std::optional<NodeHandle> {
		using T = std::decay_t<decltype(node)>;
		if constexpr (std::is_same_v<T, ast::BoolLiteral>) {
			return Add(Value{node.value}, type, provenance);
		} else if constexpr (std::is_same_v<T, ast::IntLiteral>) {
			return Add(Value{int32_t{node.value}}, type, provenance);
		} else if constexpr (std::is_same_v<T, ast::Identifier>) {
			switch (node.kind) {
			case ast::IdentifierKind::EnumValue:
				if (!type.enumInfo) {
					return Fail(expression, "Tracker enum value requires analyzed enum identity.");
				}
				return Add(Value{EnumConstant{type.enumInfo, &node.name}}, type, provenance);
			case ast::IdentifierKind::DeclaredValue:
				return Add(Value{KeyConstant{&node.name}}, type, provenance);
			case ast::IdentifierKind::Parameter:
				// Sema classified the read; recover its declaration in the supplied lexical scope.
				if (scope) {
					for (const auto& parameter : scope->params) {
						if (parameter.name.text == node.name.text) {
							const auto parameterType = GetValueType(project, parameter);
							if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&parameterType)) {
								diagnostics.push_back(*diagnostic);
								return std::nullopt;
							}
							return Add(Value{ParameterRead{&parameter}}, type, provenance);
						}
					}
				}
				return Fail(expression, "Tracker parameter read requires its lexical declaration.");
			case ast::IdentifierKind::FunctionRef:
				return Fail(expression, "Tracker function-reference lowering is not implemented yet.");
			case ast::IdentifierKind::Unresolved:
				return Fail(expression, "Tracker lowering requires a resolved identifier.");
			}
			return Fail(expression, "Tracker lowering encountered an invalid identifier kind.");
		} else if constexpr (std::is_same_v<T, ast::MemberExpr>) {
			if (!type.enumInfo) {
				return Fail(expression, "Tracker enum member requires analyzed enum identity.");
			}
			return Add(Value{EnumConstant{type.enumInfo, &node.member}}, type, provenance);
		} else if constexpr (std::is_same_v<T, ast::HereRef>) {
			const auto region = project.RegionDecls.find(node.resolvedRegion.text);
			if (region == project.RegionDecls.end() || !region->second) {
				return Fail(expression, "Tracker here value requires its sema-resolved region.");
			}
			return Add(Value{HereValue{region->second}}, type, provenance);
		} else if constexpr (std::is_same_v<T, ast::UnaryExpr>) {
			auto child = Child(node.operand, expression, scope);
			if (!child) return std::nullopt;
			child = Convert(*child, IntegralTarget::Bool, *node.operand);
			if (!child) return std::nullopt;
			return Add(Not{*child}, type, provenance);
		} else if constexpr (std::is_same_v<T, ast::BinaryExpr>) {
			return Binary(expression, node, type, scope);
		} else if constexpr (std::is_same_v<T, ast::TernaryExpr>) {
			return Conditional(expression, node, type, scope);
		} else if constexpr (std::is_same_v<T, ast::MatchExpr>) {
			return MatchExpression(expression, node, type, scope);
		} else if constexpr (std::is_same_v<T, ast::CallExpr> || std::is_same_v<T, ast::InvokeExpr>) {
			return Fail(expression, "Tracker call/invocation lowering is not implemented yet.");
		} else {
			return Fail(expression, "Tracker lowering encountered an unsupported executable value.");
		}
	}, expression.node);
}

std::optional<NodeHandle> ExpressionLowerer::Binary(const ast::Expr& expression,
	const ast::BinaryExpr& binary, const ValueType& type, const ast::DefineDecl* scope)
{
	if (const auto operation = comparisonOperator(binary.op)) {
		const auto left = Child(binary.left, expression, scope);
		const auto right = Child(binary.right, expression, scope);
		if (!left || !right) return std::nullopt;
		return Add(Comparison{*operation, *left, *right}, type, {expression.span, {&expression}});
	}

	const auto arithmetic = arithmeticOperator(binary.op);
	const bool logical = isLogical(binary.op);
	if (!logical && !arithmetic) {
		return Fail(expression, "Tracker lowering encountered an unsupported binary operator.");
	}
	const auto target = logical ? IntegralTarget::Bool : IntegralTarget::Int32;
	const auto* resultTarget = std::get_if<IntegralTarget>(&type.representation);
	if (!resultTarget || *resultTarget != target) {
		return Fail(expression, "Tracker binary result has an incompatible analyzed representation.");
	}
	Provenance provenance{expression.span, {}};
	std::vector<const ast::Expr*> operands;
	// Collect before emitting children so flattened operators leave no orphan descriptors.
	std::vector<std::pair<const ast::Expr*, bool>> pending{{&expression, true}};
	while (!pending.empty()) {
		const auto [current, mayFlatten] = pending.back();
		pending.pop_back();
		const auto* nested = std::get_if<ast::BinaryExpr>(&current->node);
		if (mayFlatten && nested && nested->op == binary.op) {
			const auto nestedType = GetValueType(project, *current);
			if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&nestedType)) {
				diagnostics.push_back(*diagnostic);
				return std::nullopt;
			}
			const auto& representation = std::get<ValueType>(nestedType).representation;
			const auto* integral = std::get_if<IntegralTarget>(&representation);
			// A different intermediate representation is a normalization barrier.
			if (integral && *integral == target) {
				if (!nested->left || !nested->right) {
					return Fail(*current, "Tracker lowering requires a complete binary expression.");
				}
				provenance.expressions.push_back(current);
				// Right-first stack pushes preserve operand order. Logical chains flatten both
				// sides, but arithmetic retains right-nested groups and interleaved left-fold timing.
				pending.emplace_back(nested->right.get(), logical);
				pending.emplace_back(nested->left.get(), true);
				continue;
			}
		}
		operands.push_back(current);
	}
	std::vector<NodeHandle> children;
	for (const auto* operand : operands) {
		auto child = Lower(*operand, scope);
		if (!child) return std::nullopt;
		child = Convert(*child, target, *operand);
		if (!child) return std::nullopt;
		children.push_back(*child);
	}
	if (logical) {
		return Add(Logical{binary.op == ast::BinaryOp::And ? LogicalOperator::And :
			LogicalOperator::Or, std::move(children)}, type, std::move(provenance));
	}
	return Add(Arithmetic{*arithmetic, std::move(children)}, type, std::move(provenance));
}

std::optional<NodeHandle> ExpressionLowerer::Conditional(const ast::Expr& expression,
	const ast::TernaryExpr& ternary, const ValueType& type, const ast::DefineDecl* scope)
{
	// Lower both branches for diagnostics and display; runtime evaluation still selects one.
	auto condition = Child(ternary.condition, expression, scope);
	auto trueBranch = Child(ternary.thenBranch, expression, scope);
	auto falseBranch = Child(ternary.elseBranch, expression, scope);
	if (!condition || !trueBranch || !falseBranch) return std::nullopt;
	condition = Convert(*condition, IntegralTarget::Bool, *ternary.condition);
	// Mixed Bool/Int branches must yield the Bool selected by sema, not the raw integer.
	if (const auto* target = std::get_if<IntegralTarget>(&type.representation)) {
		trueBranch = Convert(*trueBranch, *target, *ternary.thenBranch);
		falseBranch = Convert(*falseBranch, *target, *ternary.elseBranch);
	}
	if (!condition || !trueBranch || !falseBranch) return std::nullopt;
	return Add(Ternary{*condition, *trueBranch, *falseBranch}, type,
		{expression.span, {&expression}});
}

std::optional<NodeHandle> ExpressionLowerer::MatchExpression(const ast::Expr& expression,
	const ast::MatchExpr& match, const ValueType& type, const ast::DefineDecl* scope)
{
	const auto* target = std::get_if<IntegralTarget>(&type.representation);
	if (!target) {
		return Fail(expression, "Tracker typed matches cannot return Condition values.");
	}
	const auto discriminant = Child(match.discriminant, expression, scope);
	if (!discriminant) return std::nullopt;
	if (!std::holds_alternative<IntegralTarget>(
		graph.nodes[discriminant->slot].valueType->representation)) {
		return Fail(*match.discriminant, "Tracker match discriminants cannot be Condition values.");
	}
	std::vector<NodeHandle> arms;
	for (const auto& arm : match.arms) {
		std::vector<NodeHandle> patterns;
		for (const auto& pattern : arm.patterns) {
			const auto child = Child(pattern, expression, scope);
			if (!child) return std::nullopt;
			if (!std::holds_alternative<IntegralTarget>(
				graph.nodes[child->slot].valueType->representation)) {
				return Fail(*pattern, "Tracker match patterns cannot be Condition values.");
			}
			patterns.push_back(*child);
		}
		if (arm.isDefault != patterns.empty()) {
			return Fail(expression, "Tracker match requires explicit nonempty patterns or a default arm.");
		}
		// An empty pattern list represents the source wildcard, not an extra fallback arm.
		const auto patternList = Add(MatchPatterns{std::move(patterns)}, std::nullopt,
			{expression.span, {&expression}});
		auto body = Child(arm.body, expression, scope);
		if (!body) return std::nullopt;
		body = Convert(*body, *target, *arm.body);
		if (!body) return std::nullopt;
		arms.push_back(Add(MatchArm{patternList, *body, arm.fallthrough}, std::nullopt,
			{arm.body->span, {arm.body.get()}}));
	}
	// The typed runtime match supplies zero/false when unmatched; no synthetic arm is needed.
	return Add(Match{*target, *discriminant, std::move(arms)}, type,
		{expression.span, {&expression}});
}

LoweringResult LowerRoots(const ast::Project& project, const std::filesystem::path& sourceRoot) {
	const auto sources = CollectRoots(project, sourceRoot);
	if (const auto* diagnostics = std::get_if<std::vector<ast::Diagnostic>>(&sources)) {
		return *diagnostics;
	}
	Graph graph;
	std::vector<ast::Diagnostic> diagnostics;
	ExpressionLowerer lowerer(project, graph, diagnostics);
	for (const auto& source : std::get<std::vector<RootSource>>(sources)) {
		if (!source.entry->condition) {
			diagnostics.push_back({"", source.entry->span, ast::DiagnosticLevel::Error,
				"Tracker root lowering requires an entry condition."});
			continue;
		}
		const auto& condition = *source.entry->condition;
		auto root = lowerer.Lower(condition);
		if (!root) continue;
		root = lowerer.Convert(*root, IntegralTarget::Bool, condition);
		if (!root) continue;
		graph.roots.push_back({source.kind, source.owner, source.entry, source.occurrence,
			*root, {source.entry->span, {&condition}}});
	}
	// Failed lowering may have appended nodes; never expose that partial graph as success.
	if (!diagnostics.empty()) return diagnostics;
	return graph;
}

} // namespace rls::transpilers::soh_tracker::ir
