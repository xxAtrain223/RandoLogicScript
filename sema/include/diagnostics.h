#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "ast.h"

namespace rls::sema::diagnostics {

inline ast::Diagnostic FunctionRequiresZeroArguments(ast::Span span, std::string_view function, size_t count) {
	return {"RLS-T001", std::move(span), ast::DiagnosticLevel::Error, std::format("function '{}' requires {} argument(s); only zero-argument functions can be used as callable values", function, count)};
}
inline ast::Diagnostic FunctionReferenceTypeUnavailable(ast::Span span, std::string_view function) {
	return {"RLS-T002", std::move(span), ast::DiagnosticLevel::Error, std::format("function '{}' callable reference type is not available yet", function)};
}
inline ast::Diagnostic FunctionCannotBeCondition(ast::Span span, std::string_view function, std::string_view type) {
	return {"RLS-T003", std::move(span), ast::DiagnosticLevel::Error, std::format("function '{}' cannot be used as a Condition callable because it returns {}", function, type)};
}
inline ast::Diagnostic FunctionCallableMissingReturnType(ast::Span span, std::string_view function) {
	return {"RLS-T004", std::move(span), ast::DiagnosticLevel::Error, std::format("function '{}' cannot be used as callable value without a return type", function)};
}
inline ast::Diagnostic AmbiguousIdentifier(ast::Span span, std::string_view name, std::string_view enums) {
	return {"RLS-T005", std::move(span), ast::DiagnosticLevel::Error, std::format("ambiguous identifier '{}' found in multiple enums ({}); use EnumName.{} to disambiguate", name, enums, name)};
}
inline ast::Diagnostic UnknownIdentifier(ast::Span span, std::string_view name) {
	return {"RLS-T006", std::move(span), ast::DiagnosticLevel::Error,
		std::format("unknown identifier '{}'", name),
		ast::DiagnosticActionData{1, "rls.declareSymbol", {std::string(name)}}};
}
inline ast::Diagnostic UnaryRequiresBool(ast::Span span, std::string_view type) {
	return {"RLS-T007", std::move(span), ast::DiagnosticLevel::Error, std::format("'not' requires a Bool operand, got {}", type)};
}
inline ast::Diagnostic LogicalRequiresBool(ast::Span span, std::string_view op, std::string_view side, std::string_view type) {
	return {"RLS-T008", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' requires Bool operands, {} is {}", op, side, type)};
}
inline ast::Diagnostic IncompatibleComparison(ast::Span span, std::string_view left, std::string_view right) {
	return {"RLS-T009", std::move(span), ast::DiagnosticLevel::Error, std::format("comparison between incompatible types {} and {}", left, right)};
}
inline ast::Diagnostic EnumComparisonMismatch(ast::Span span, std::string_view left, std::string_view right) {
	return {"RLS-T010", std::move(span), ast::DiagnosticLevel::Error, std::format("comparison between enum '{}' and enum '{}'", left, right)};
}
inline ast::Diagnostic ComparisonRequiresInt(ast::Span span, std::string_view side, std::string_view type) {
	return {"RLS-T011", std::move(span), ast::DiagnosticLevel::Error, std::format("comparison requires Int operands, {} is {}", side, type)};
}
inline ast::Diagnostic ArithmeticRequiresInt(ast::Span span, std::string_view side, std::string_view type) {
	return {"RLS-T012", std::move(span), ast::DiagnosticLevel::Error, std::format("arithmetic requires Int operands, {} is {}", side, type)};
}
inline ast::Diagnostic TernaryConditionType(ast::Span span, std::string_view type) {
	return {"RLS-T013", std::move(span), ast::DiagnosticLevel::Error, std::format("ternary condition must be Bool, got {}", type)};
}
inline ast::Diagnostic TernaryEnumMismatch(ast::Span span, std::string_view thenEnum, std::string_view elseEnum) {
	return {"RLS-T014", std::move(span), ast::DiagnosticLevel::Error, std::format("ternary branches have different enum types: '{}' and '{}'", thenEnum, elseEnum)};
}
inline ast::Diagnostic TernaryImplicitBool(ast::Span span, std::string_view thenType, std::string_view elseType) {
	return {"RLS-T015", std::move(span), ast::DiagnosticLevel::Warning, std::format("ternary branches have types {} and {}, implicitly converted to Bool", thenType, elseType)};
}
inline ast::Diagnostic TernaryBranchMismatch(ast::Span span, std::string_view thenType, std::string_view elseType) {
	return {"RLS-T016", std::move(span), ast::DiagnosticLevel::Error, std::format("ternary branches have different types: {} and {}", thenType, elseType)};
}
inline ast::Diagnostic UnknownNamedArgument(ast::Span span, std::string_view function, std::string_view name) {
	return {"RLS-T017", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' unknown named argument '{}'", function, name)};
}
inline ast::Diagnostic DuplicateArgument(ast::Span span, std::string_view function, std::string_view name) {
	return {"RLS-T018", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' duplicate argument for parameter '{}'", function, name)};
}
inline ast::Diagnostic ArgumentCountMismatch(ast::Span span, std::string_view function, std::string_view expected, size_t actual) {
	return {"RLS-T019", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' expects {} argument(s), got {}", function, expected, actual)};
}
inline ast::Diagnostic MissingRequiredArguments(ast::Span span, std::string_view function, std::string_view names) {
	return {"RLS-T020", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' missing required argument(s): {}", function, names)};
}
inline ast::Diagnostic AmbiguousEnumInteger(ast::Span span, std::string_view function, size_t argument, int value, std::string_view enums) {
	return {"RLS-T021", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' argument {} uses ambiguous integer value {}; matching enums: {}; provide explicit enum context or EnumName.ValueName", function, argument, value, enums)};
}
inline ast::Diagnostic EnumArgumentMismatch(ast::Span span, std::string_view function, size_t argument, std::string_view expected, std::string_view actual) {
	return {"RLS-T022", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' argument {} expected enum '{}', got enum '{}'", function, argument, expected, actual)};
}
inline ast::Diagnostic ArgumentTypeMismatch(ast::Span span, std::string_view function, size_t argument, std::string_view expected, std::string_view actual) {
	return {"RLS-T023", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' argument {} expected {}, got {}", function, argument, expected, actual)};
}
inline ast::Diagnostic ValueNotCallable(ast::Span span, std::string_view name, std::string_view type) {
	return {"RLS-T024", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' is not callable (type {})", name, type)};
}
inline ast::Diagnostic ZeroArgumentCallMismatch(ast::Span span, std::string_view name, size_t count) {
	return {"RLS-T025", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' expects 0 argument(s), got {}", name, count)};
}
inline ast::Diagnostic UnknownFunction(ast::Span span, std::string_view name) {
	return {"RLS-T026", std::move(span), ast::DiagnosticLevel::Error,
		std::format("unknown function '{}'", name),
		ast::DiagnosticActionData{1, "rls.declareFunction", {std::string(name)}}};
}
inline ast::Diagnostic ExpressionNotCallable(ast::Span span, std::string_view type) {
	return {"RLS-T027", std::move(span), ast::DiagnosticLevel::Error, std::format("expression is not callable (type {})", type)};
}
inline ast::Diagnostic UnknownEnumMember(ast::Span span, std::string_view member, std::string_view enumName) {
	return {"RLS-T028", std::move(span), ast::DiagnosticLevel::Error, std::format("'{}' is not a member of enum '{}'", member, enumName)};
}
inline ast::Diagnostic UnknownEnum(ast::Span span, std::string_view name) {
	return {"RLS-T029", std::move(span), ast::DiagnosticLevel::Error, std::format("unknown enum '{}' in member access", name)};
}
inline ast::Diagnostic HereOutsideRegion(ast::Span span) {
	return {"RLS-T030", std::move(span), ast::DiagnosticLevel::Error, "'here' can only be used inside a region entry condition; it has type Region"};
}
inline ast::Diagnostic MatchWildcardNotStandalone(ast::Span span) {
	return {"RLS-T031", std::move(span), ast::DiagnosticLevel::Error, "match wildcard '_' must be a standalone pattern"};
}
inline ast::Diagnostic MatchWildcardNotLast(ast::Span span) {
	return {"RLS-T032", std::move(span), ast::DiagnosticLevel::Error, "match wildcard '_' arm must be last"};
}
inline ast::Diagnostic MatchPatternTypeMismatch(ast::Span span, std::string_view pattern, std::string_view actual, std::string_view expected) {
	return {"RLS-T033", std::move(span), ast::DiagnosticLevel::Error, std::format("match pattern '{}' is {} but expected {}", pattern, actual, expected)};
}
inline ast::Diagnostic MatchPatternEnumMismatch(ast::Span span, std::string_view pattern, std::string_view actual, std::string_view expected) {
	return {"RLS-T034", std::move(span), ast::DiagnosticLevel::Error, std::format("match pattern '{}' is enum '{}' but expected enum '{}'", pattern, actual, expected)};
}
inline ast::Diagnostic MatchDiscriminantTypeMismatch(ast::Span span, std::string_view name, std::string_view actual, std::string_view expected) {
	return {"RLS-T035", std::move(span), ast::DiagnosticLevel::Error, std::format("match discriminant '{}' is {} but patterns are {}", name, actual, expected)};
}
inline ast::Diagnostic MatchDiscriminantEnumMismatch(ast::Span span, std::string_view name, std::string_view actual, std::string_view expected) {
	return {"RLS-T036", std::move(span), ast::DiagnosticLevel::Error, std::format("match discriminant '{}' is enum '{}' but patterns are enum '{}'", name, actual, expected)};
}
inline ast::Diagnostic MatchArmImplicitBool(ast::Span span, std::string_view actual, std::string_view previous) {
	return {"RLS-T037", std::move(span), ast::DiagnosticLevel::Warning, std::format("match arm type {} implicitly converted to Bool (previous arms are {})", actual, previous)};
}
inline ast::Diagnostic MatchArmTypeMismatch(ast::Span span, std::string_view actual, std::string_view previous) {
	return {"RLS-T038", std::move(span), ast::DiagnosticLevel::Error, std::format("match arm type {} doesn't match previous arms ({})", actual, previous)};
}
inline ast::Diagnostic DefineCycle(ast::Span span, std::string_view names) {
	return {"RLS-T039", std::move(span), ast::DiagnosticLevel::Error, std::format("cycle in define call graph: {}", names)};
}
inline ast::Diagnostic UnknownParameterTypeAnnotation(ast::Span span, std::string_view type, std::string_view parameter) {
	return {"RLS-T040", std::move(span), ast::DiagnosticLevel::Error, std::format("unknown type annotation '{}' for parameter '{}'", type, parameter)};
}

inline ast::Diagnostic UnknownExtensionTarget(ast::Span span, std::string_view regionName) {
	return {"RLS-V001", std::move(span), ast::DiagnosticLevel::Error,
		std::format("extend region targets unknown region '{}'", regionName),
		ast::DiagnosticActionData{1, "rls.createRegion", {std::string(regionName)}}};
}
inline ast::Diagnostic DuplicateRegionData(ast::Span span, std::string_view key, std::string_view regionName) {
    return {"RLS-V002", std::move(span), ast::DiagnosticLevel::Error, std::format("duplicate data key '{}' in region '{}'", key, regionName)};
}
inline ast::Diagnostic DuplicateRegionEntry(ast::Span span, std::string_view kind, std::string_view name, std::string_view regionName) {
	return {"RLS-V003", std::move(span), ast::DiagnosticLevel::Error, std::format("duplicate {} '{}' in region '{}'", kind, name, regionName)};
}
inline ast::Diagnostic EntryConditionType(ast::Span span, std::string_view kind, std::string_view name, std::string_view regionName, std::string_view type) {
	return {"RLS-V004", std::move(span), ast::DiagnosticLevel::Error, std::format("{} condition for '{}' in region '{}' must be Bool, got {}", kind, name, regionName, type)};
}
inline ast::Diagnostic UnreachableRegion(ast::Span span, std::string_view regionName) {
	return {"RLS-V005", std::move(span), ast::DiagnosticLevel::Warning, std::format("region '{}' is not reachable from 'RR_ROOT'", regionName)};
}
inline ast::Diagnostic UnusedDefine(ast::Span span, std::string_view name) {
	return {"RLS-V006", std::move(span), ast::DiagnosticLevel::Info, std::format("'{}' is defined but never used", name)};
}
inline ast::Diagnostic DuplicateParameter(ast::Span span, std::string_view parameter, std::string_view kind, std::string_view name) {
	return {"RLS-V007", std::move(span), ast::DiagnosticLevel::Error, std::format("duplicate parameter '{}' in {} '{}'", parameter, kind, name)};
}
inline ast::Diagnostic RequiredAfterOptionalParameter(ast::Span span, std::string_view parameter, std::string_view kind, std::string_view name) {
	return {"RLS-V008", std::move(span), ast::DiagnosticLevel::Error, std::format("required parameter '{}' cannot follow optional parameters in {} '{}'", parameter, kind, name)};
}
inline ast::Diagnostic ExternParameterMissingType(ast::Span span, std::string_view function, std::string_view parameter) {
	return {"RLS-V009", std::move(span), ast::DiagnosticLevel::Error, std::format("extern define '{}' parameter '{}' must have a type annotation or a default value", function, parameter)};
}
inline ast::Diagnostic ExternParameterCannotInfer(ast::Span span, std::string_view function, std::string_view parameter) {
	return {"RLS-V010", std::move(span), ast::DiagnosticLevel::Error, std::format("extern define '{}' parameter '{}' needs an explicit type or an inferrable default", function, parameter)};
}
inline ast::Diagnostic DefaultValueTypeMismatch(ast::Span span, std::string_view parameter, std::string_view kind, std::string_view name, std::string_view actual, std::string_view expected) {
	return {"RLS-V011", std::move(span), ast::DiagnosticLevel::Error, std::format("default value for parameter '{}' in {} '{}' has type {}, expected {}", parameter, kind, name, actual, expected)};
}
inline ast::Diagnostic ExternMissingReturnType(ast::Span span, std::string_view function) {
	return {"RLS-V012", std::move(span), ast::DiagnosticLevel::Error, std::format("extern define '{}' must declare a return type", function)};
}
inline ast::Diagnostic ExternUnknownReturnType(ast::Span span, std::string_view type, std::string_view function) {
	return {"RLS-V013", std::move(span), ast::DiagnosticLevel::Error, std::format("unknown return type annotation '{}' for extern define '{}'", type, function)};
}
inline ast::Diagnostic EnumWildcardPattern(ast::Span span, std::string_view name, std::string_view pattern) {
	return {"RLS-V014", std::move(span), ast::DiagnosticLevel::Error, std::format("enum '{}' cannot contain wildcard pattern '{}'", name, pattern)};
}
inline ast::Diagnostic EnumDuplicateMember(ast::Span span, std::string_view member, std::string_view name) {
	return {"RLS-V015", std::move(span), ast::DiagnosticLevel::Error, std::format("duplicate enum member '{}' in enum '{}'", member, name)};
}
inline ast::Diagnostic ExternEnumDuplicateMember(ast::Span span, std::string_view member, std::string_view name) {
	return {"RLS-V015", std::move(span), ast::DiagnosticLevel::Error, std::format("duplicate enum member '{}' in extern enum '{}'", member, name)};
}
inline ast::Diagnostic EnumDuplicateValue(ast::Span span, int value, std::string_view name) {
	return {"RLS-V016", std::move(span), ast::DiagnosticLevel::Error, std::format("duplicate enum value {} in enum '{}'", value, name)};
}
inline ast::Diagnostic ExternEnumEmpty(ast::Span span, std::string_view name) {
	return {"RLS-V017", std::move(span), ast::DiagnosticLevel::Error, std::format("extern enum '{}' must declare at least one member or wildcard pattern", name)};
}
inline ast::Diagnostic ExternEnumWildcardOverlap(ast::Span span, std::string_view name, std::string_view pattern, std::string_view member) {
	return {"RLS-V018", std::move(span), ast::DiagnosticLevel::Warning, std::format("extern enum '{}' wildcard '{}' overlaps explicit member '{}'", name, pattern, member)};
}
inline ast::Diagnostic EnumValueNameCollision(ast::Span span, std::string_view value, std::string_view enumNames) {
	return {"RLS-V019", std::move(span), ast::DiagnosticLevel::Warning, std::format("enum value '{}' appears in multiple enums ({}) and may require dotted disambiguation", value, enumNames)};
}

inline bool IsDuplicateRegionData(const ast::Diagnostic& diagnostic) {
	return diagnostic.code == "RLS-V002";
}

} // namespace rls::sema::diagnostics
