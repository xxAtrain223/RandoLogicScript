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

} // namespace rls::transpilers::soh_tracker::ir
