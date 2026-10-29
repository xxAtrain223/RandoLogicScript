#include "tracker_ir.h"

#include <string>
#include <string_view>

namespace rls::transpilers::soh_tracker::ir {

namespace {

ast::Diagnostic error(const ast::Span& span, std::string message) {
	return {"", span, ast::DiagnosticLevel::Error, std::move(message)};
}

ValueTypeResult getValueType(const ast::Project& project, std::optional<ast::Type> type,
	std::optional<std::string_view> enumName, const ast::Span& span)
{
	if (!type) {
		return error(span, "Tracker lowering requires an analyzed value type.");
	}

	switch (*type) {
	case ast::Type::Bool:
		return ValueType{*type, nullptr, IntegralTarget::Bool};
	case ast::Type::Int:
	case ast::Type::Region:
	case ast::Type::Event:
	case ast::Type::Location:
		return ValueType{*type, nullptr, IntegralTarget::Int32};
	case ast::Type::Enum: {
		if (!enumName) {
			return error(span, "Tracker lowering requires analyzed enum identity.");
		}
		const auto* info = project.getEnumInfo(*enumName);
		if (!info) {
			return error(span, "Tracker lowering is missing enum metadata for '" +
				std::string(*enumName) + "'.");
		}
		return ValueType{*type, info, IntegralTarget::Int32};
	}
	case ast::Type::Condition:
		return ValueType{*type, nullptr, ConditionTarget{}};
	case ast::Type::String:
		return error(span, "Tracker runtime values cannot represent String.");
	case ast::Type::List:
		return error(span, "Tracker runtime values cannot represent List.");
	case ast::Type::Callable:
		return error(span, "Tracker runtime values cannot represent a general Callable.");
	case ast::Type::Void:
		return error(span, "Tracker runtime values cannot represent Void.");
	case ast::Type::Error:
		return error(span, "Tracker lowering cannot use an analyzed Error type.");
	}
	return error(span, "Tracker lowering encountered an unsupported analyzed type.");
}

} // namespace

ValueTypeResult GetValueType(const ast::Project& project, const ast::Expr& expression) {
	return getValueType(project, project.getType(&expression),
		project.getEnumType(&expression), expression.span);
}

ValueTypeResult GetValueType(const ast::Project& project, const ast::Param& parameter) {
	return getValueType(project, project.getType(&parameter),
		project.getEnumType(&parameter), parameter.span);
}

std::variant<ChainSources, ast::Diagnostic> CollectChainSources(
	const ast::Project& project, const ast::Expr& expression)
{
	const auto* binary = std::get_if<ast::BinaryExpr>(&expression.node);
	if (!binary) return error(expression.span, "Tracker chain requires a source binary expression.");

	const bool logical = binary->op == ast::BinaryOp::And || binary->op == ast::BinaryOp::Or;
	if (!logical && binary->op != ast::BinaryOp::Add && binary->op != ast::BinaryOp::Sub &&
		binary->op != ast::BinaryOp::Mul && binary->op != ast::BinaryOp::Div) {
		return error(expression.span, "Tracker chain requires a supported source operator.");
	}

	const auto target = logical ? IntegralTarget::Bool : IntegralTarget::Int32;
	ChainSources sources{{expression.span, {}}, {}};
	std::vector<std::pair<const ast::Expr*, bool>> pending{{&expression, true}};
	while (!pending.empty()) {
		const auto [current, mayFlatten] = pending.back();
		pending.pop_back();
		const auto* nested = std::get_if<ast::BinaryExpr>(&current->node);
		if (mayFlatten && nested && nested->op == binary->op) {
			const auto nestedType = GetValueType(project, *current);
			if (const auto* diagnostic = std::get_if<ast::Diagnostic>(&nestedType)) return *diagnostic;
			const auto& representation = std::get<ValueType>(nestedType).representation;
			const auto* integral = std::get_if<IntegralTarget>(&representation);

			// A different intermediate representation is a normalization barrier.
			if (integral && *integral == target) {
				if (!nested->left || !nested->right) {
					return error(current->span, "Tracker lowering requires a complete binary expression.");
				}
				sources.provenance.expressions.push_back(current);
				// Right-first pushes preserve operand order. Retaining right-nested arithmetic
				// also preserves the consumer's interleaved left-fold/exception timing.
				pending.emplace_back(nested->right.get(), logical);
				pending.emplace_back(nested->left.get(), true);
				continue;
			}
		}
		sources.operands.push_back(current);
	}
	return sources;
}

} // namespace rls::transpilers::soh_tracker::ir
