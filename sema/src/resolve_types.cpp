#include "resolve_types.h"
#include "diagnostics.h"
#include "type_helpers.h"

#include <format>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace rls::sema {

// == Two-Stage Identifier Lookup (Stage A + B) ===============================

/// Result of two-stage identifier lookup.
struct IdentifierLookupResult {
	std::optional<ast::Type> type;
	std::optional<std::string> enumName; // Name of the enum if resolved
	std::vector<std::string> ambiguousEnums; // If multiple enums claim the identifier
};

/// Two-stage identifier lookup:
/// Check whether an identifier matches a declared enum member or glob pattern.
/// Returns error info if multiple enums claim the identifier (ambiguity).
static IdentifierLookupResult lookupIdentifierInEnums(
	std::string_view name, const ast::Project& project) {
	
	std::vector<std::string> matchingEnums;

	// Stage A: Check all user-defined and extern enums for exact member matches
	for (const auto& [enumName, enumInfo] : project.EnumInfos) {
		for (const auto& entry : enumInfo.entries) {
			if (std::holds_alternative<ast::EnumMemberInfo>(entry)) {
				const auto& member = std::get<ast::EnumMemberInfo>(entry);
				if (member.name.text == name) {
					matchingEnums.push_back(enumName);
					break; // Found a match in this enum, move to next enum
				}
			} else if (std::holds_alternative<ast::EnumPatternInfo>(entry)) {
				const auto& pattern = std::get<ast::EnumPatternInfo>(entry);
				if (globMatches(pattern.pattern, name)) {
					matchingEnums.push_back(enumName);
					break; // Found a match in this enum, move to next enum
				}
			}
		}
	}

	// If multiple enums claim the identifier, report ambiguity
	if (matchingEnums.size() > 1) {
		return {
			.type = std::nullopt,
			.enumName = std::nullopt,
			.ambiguousEnums = std::move(matchingEnums)
		};
	}

	// If exactly one enum claims it, return that enum type
	if (matchingEnums.size() == 1) {
		return {
			.type = ast::Type::Enum,
			.enumName = std::move(matchingEnums[0]),
			.ambiguousEnums = {}
		};
	}

	// No match found anywhere
	return {
		.type = std::nullopt,
		.enumName = std::nullopt,
		.ambiguousEnums = {}
	};
}

static bool isEnumLikeType(ast::Type type) {
	return type == ast::Type::Enum;
}

static bool isIntCompatibleType(ast::Type type) {
	return type == ast::Type::Int || isEnumLikeType(type);
}

static bool enumEntryMatchesName(const ast::EnumEntryInfo& entry, std::string_view valueName) {
	if (std::holds_alternative<ast::EnumMemberInfo>(entry)) {
		return std::get<ast::EnumMemberInfo>(entry).name.text == valueName;
	}

	const auto& pattern = std::get<ast::EnumPatternInfo>(entry);
	return globMatches(pattern.pattern, valueName);
}

static bool enumContainsValueName(const ast::EnumInfo& info, std::string_view valueName) {
	for (const auto& entry : info.entries) {
		if (enumEntryMatchesName(entry, valueName)) {
			return true;
		}
	}
	return false;
}

static std::vector<std::string> enumNamesWithExplicitValue(const ast::Project& project, int value) {
	std::vector<std::string> names;
	for (const auto& [enumName, info] : project.EnumInfos) {
		for (const auto& entry : info.entries) {
			if (!std::holds_alternative<ast::EnumMemberInfo>(entry)) {
				continue;
			}
			const auto& member = std::get<ast::EnumMemberInfo>(entry);
			if (member.value.has_value() && *member.value == value) {
				names.push_back(enumName);
				break;
			}
		}
	}
	return names;
}

// == Step 4: Scope for parameters ============================================

/// Maps parameter names to their types. nullopt = not yet inferred.
using Scope = std::unordered_map<std::string, std::optional<ast::Type>>;

/// Maps parameter names to enum identity when parameter type is Enum.
using EnumIdentityScope = std::unordered_map<std::string, std::optional<std::string>>;

// == Step 3: Bottom-up expression typing =====================================

/// Visitor that resolves and type-checks all expression nodes.
/// Holds shared context (project, scope, diagnostics) so individual
/// handlers only need the node and its wrapping Expr.
struct ExprResolver {
	ast::Project& project;
	Scope& scope;
	EnumIdentityScope& enumScope;
	std::vector<ast::Diagnostic>& diags;
	std::optional<ast::Name> currentRegion; // Set when resolving region/extend-region entries.

	using T = ast::Type;

	struct ArgBindingResult {
		std::vector<std::optional<size_t>> argToParam;
		std::vector<bool> paramBound;
		bool hasNamedArgs = false;
		bool hasError = false;
	};

	bool inferUntypedParamIdentifier(ast::Expr& expr, T expectedType) {
		auto* id = std::get_if<ast::Identifier>(&expr.node);
		if (id == nullptr) {
			return false;
		}

		auto it = scope.find(id->name.text);
		if (it == scope.end() || it->second.has_value()) {
			return false;
		}

		id->kind = ast::IdentifierKind::Parameter;
		it->second = expectedType;
		if (expectedType == T::Enum) {
			enumScope[id->name.text] = std::nullopt;
		}
		project.setType(&expr, expectedType);
		return true;
	}

	// -- Node handlers -------------------------------------------------------

	ast::Type resolve(const ast::BoolLiteral&, ast::Expr&) {
		return ast::Type::Bool;
	}

	ast::Type resolve(const ast::IntLiteral&, ast::Expr&) {
		return ast::Type::Int;
	}

	ast::Type resolve(const ast::StringLiteral&, ast::Expr&) {
		return ast::Type::String;
	}

	ast::Type resolve(const ast::ListExpr& node, ast::Expr&) {
		for (const auto& element : node.elements) {
			resolveExpr(*element);
		}
		return ast::Type::List;
	}

	ast::Type resolve(ast::Identifier& node, ast::Expr& expr) {
		// Check scope first (parameter names).
		if (auto it = scope.find(node.name.text); it != scope.end()) {
			node.kind = ast::IdentifierKind::Parameter;
			if (it->second) {
				if (*it->second == T::Enum) {
					if (auto enumIt = enumScope.find(node.name.text);
						enumIt != enumScope.end() && enumIt->second.has_value()) {
						project.setEnumType(&expr, *enumIt->second);
					}
				}
				return *it->second;
			}
			// Parameter exists but type not yet inferred (Step 5).
			return ast::Type::Error;
		}

		if (auto defIt = project.DefineDecls.find(node.name.text);
			defIt != project.DefineDecls.end()) {
			const auto* def = defIt->second;
			// TODO: Zero-Argument Constraint — functions with parameters cannot be callable.
			if (!def->params.empty()) {
				diags.push_back(diagnostics::FunctionRequiresZeroArguments(expr.span, node.name.text, def->params.size()));
				return ast::Type::Error;
			}

			auto bodyType = project.getType(def->body.get());
			if (!bodyType.has_value()) {
				diags.push_back(diagnostics::FunctionReferenceTypeUnavailable(expr.span, node.name.text));
				return ast::Type::Error;
			}

			// TODO: Bool-Return-Type Constraint — only () -> Bool functions become Condition.
			if (*bodyType != ast::Type::Bool) {
				diags.push_back(diagnostics::FunctionCannotBeCondition(expr.span, node.name.text, typeName(*bodyType)));
				return ast::Type::Error;
			}

			node.kind = ast::IdentifierKind::FunctionRef;
			return ast::Type::Condition;
		}

		if (auto extIt = project.ExternDefineDecls.find(node.name.text);
			extIt != project.ExternDefineDecls.end()) {
			const auto* ext = extIt->second;
			if (!ext->params.empty()) {
				diags.push_back(diagnostics::FunctionRequiresZeroArguments(expr.span, node.name.text, ext->params.size()));
				return ast::Type::Error;
			}

			if (!ext->returnType) {
				diags.push_back(diagnostics::FunctionCallableMissingReturnType(expr.span, node.name.text));
				return ast::Type::Error;
			}

			auto returnType = resolveTypeAnnotation(project, ext->returnType->name.text);
			if (!returnType.has_value() || returnType->type != ast::Type::Bool) {
				diags.push_back(diagnostics::FunctionCannotBeCondition(expr.span, node.name.text,
					returnType.has_value() ? typeName(returnType->type) : std::string_view{"<unknown>"}));
				return ast::Type::Error;
			}

			node.kind = ast::IdentifierKind::FunctionRef;
			return ast::Type::Condition;
		}

		std::vector<std::pair<std::string_view, T>> declaredTypes;
		if (project.RegionDecls.contains(node.name.text)) {
			declaredTypes.emplace_back("Region", T::Region);
		}
		if (project.EventDecls.contains(node.name.text)) {
			declaredTypes.emplace_back("Event", T::Event);
		}
		if (project.LocationDecls.contains(node.name.text)) {
			declaredTypes.emplace_back("Location", T::Location);
		}
		if (declaredTypes.size() > 1) {
			std::string categories(declaredTypes.front().first);
			for (size_t index = 1; index < declaredTypes.size(); ++index) {
				categories += ", ";
				categories += declaredTypes[index].first;
			}
			diags.push_back(diagnostics::AmbiguousIdentifier(
				expr.span, node.name.text, categories));
			return T::Error;
		}
		if (declaredTypes.size() == 1) {
			node.kind = ast::IdentifierKind::DeclaredValue;
			return declaredTypes.front().second;
		}

		// Resolve identifiers exclusively through declared enum metadata.
		auto lookup = lookupIdentifierInEnums(node.name.text, project);

		// Check for ambiguity (multiple enums claiming the same identifier)
		if (!lookup.ambiguousEnums.empty()) {
			std::string enumList;
			for (size_t i = 0; i < lookup.ambiguousEnums.size(); ++i) {
				if (i > 0) enumList += ", ";
				enumList += lookup.ambiguousEnums[i];
			}
			diags.push_back(diagnostics::AmbiguousIdentifier(expr.span, node.name.text, enumList));
			return ast::Type::Error;
		}

		// If found in an enum (Stage A match)
		if (lookup.type && lookup.enumName) {
			node.kind = ast::IdentifierKind::EnumValue;
			project.setEnumType(&expr, *lookup.enumName);
			return *lookup.type;
		}

		diags.push_back(diagnostics::UnknownIdentifier(expr.span, node.name.text));
		return ast::Type::Error;
	}

	ast::Type resolve(const ast::UnaryExpr& node, ast::Expr& expr) {
		inferUntypedParamIdentifier(*node.operand, ast::Type::Bool);
		auto opType = resolveExpr(*node.operand);
		if (opType != ast::Type::Error && !isBoolCompatible(opType)) {
			diags.push_back(diagnostics::UnaryRequiresBool(expr.span, typeName(opType)));
		}
		return ast::Type::Bool;
	}

	ast::Type resolve(const ast::BinaryExpr& node, ast::Expr& expr) {
		using T = ast::Type;

		switch (node.op) {
		case ast::BinaryOp::And:
		case ast::BinaryOp::Or:
			inferUntypedParamIdentifier(*node.left, T::Bool);
			inferUntypedParamIdentifier(*node.right, T::Bool);
			break;

		case ast::BinaryOp::Lt:
		case ast::BinaryOp::LtEq:
		case ast::BinaryOp::Gt:
		case ast::BinaryOp::GtEq:
		case ast::BinaryOp::Add:
		case ast::BinaryOp::Sub:
		case ast::BinaryOp::Mul:
		case ast::BinaryOp::Div:
			inferUntypedParamIdentifier(*node.left, T::Int);
			inferUntypedParamIdentifier(*node.right, T::Int);
			break;

		case ast::BinaryOp::Eq:
		case ast::BinaryOp::NotEq:
			break;
		}

		auto leftType = resolveExpr(*node.left);
		auto rightType = resolveExpr(*node.right);

		if (node.op == ast::BinaryOp::Eq || node.op == ast::BinaryOp::NotEq) {
			if (leftType == T::Error && rightType != T::Error
				&& inferUntypedParamIdentifier(*node.left, rightType)) {
				leftType = rightType;
			}
			if (rightType == T::Error && leftType != T::Error
				&& inferUntypedParamIdentifier(*node.right, leftType)) {
				rightType = leftType;
			}
		}

		switch (node.op) {
		// Logical: both sides must be bool-compatible.
		case ast::BinaryOp::And:
		case ast::BinaryOp::Or: {
			auto opName = node.op == ast::BinaryOp::And ? "and" : "or";
			if (leftType != T::Error && !isBoolCompatible(leftType)) {
				diags.push_back(diagnostics::LogicalRequiresBool(node.left->span, opName, "left", typeName(leftType)));
			}
			if (rightType != T::Error && !isBoolCompatible(rightType)) {
				diags.push_back(diagnostics::LogicalRequiresBool(node.right->span, opName, "right", typeName(rightType)));
			}
			return T::Bool;
		}

		// Equality: both sides must be the same type.
		case ast::BinaryOp::Eq:
		case ast::BinaryOp::NotEq:
		{
			const auto leftEnum = leftType == T::Enum
				? project.getEnumType(node.left.get()) : std::optional<std::string_view>{};
			const auto rightEnum = rightType == T::Enum
				? project.getEnumType(node.right.get()) : std::optional<std::string_view>{};
			if (leftType != T::Error && rightType != T::Error
				&& leftType != rightType
				&& !(leftType == T::Int && isEnumLikeType(rightType))
				&& !(rightType == T::Int && isEnumLikeType(leftType))
				&& !areDomainAndEnumCompatible(leftType, leftEnum, rightType, rightEnum)) {
				diags.push_back(diagnostics::IncompatibleComparison(expr.span, typeName(leftType), typeName(rightType)));
			}
			if (leftType == T::Enum && rightType == T::Enum) {
				if (leftEnum.has_value() && rightEnum.has_value() && *leftEnum != *rightEnum) {
					diags.push_back(diagnostics::EnumComparisonMismatch(expr.span, *leftEnum, *rightEnum));
				}
			}
			return T::Bool;
		}

		// Ordering: both sides must be Int.
		case ast::BinaryOp::Lt:
		case ast::BinaryOp::LtEq:
		case ast::BinaryOp::Gt:
		case ast::BinaryOp::GtEq:
			if (leftType != T::Error && !isIntCompatibleType(leftType)) {
				diags.push_back(diagnostics::ComparisonRequiresInt(node.left->span, "left", typeName(leftType)));
			}
			if (rightType != T::Error && !isIntCompatibleType(rightType)) {
				diags.push_back(diagnostics::ComparisonRequiresInt(node.right->span, "right", typeName(rightType)));
			}
			return T::Bool;

		// Arithmetic: both sides must be Int.
		case ast::BinaryOp::Add:
		case ast::BinaryOp::Sub:
		case ast::BinaryOp::Mul:
		case ast::BinaryOp::Div:
			if (leftType != T::Error && !isIntCompatibleType(leftType)) {
				diags.push_back(diagnostics::ArithmeticRequiresInt(node.left->span, "left", typeName(leftType)));
			}
			if (rightType != T::Error && !isIntCompatibleType(rightType)) {
				diags.push_back(diagnostics::ArithmeticRequiresInt(node.right->span, "right", typeName(rightType)));
			}
			return T::Int;
		}

		return T::Error; // unreachable — all BinaryOp cases covered
	}

	ast::Type resolve(const ast::TernaryExpr& node, ast::Expr& expr) {
		using T = ast::Type;
		inferUntypedParamIdentifier(*node.condition, T::Bool);
		auto condType = resolveExpr(*node.condition);
		auto thenType = resolveExpr(*node.thenBranch);
		auto elseType = resolveExpr(*node.elseBranch);

		if (condType != T::Error && !isBoolCompatible(condType)) {
			diags.push_back(diagnostics::TernaryConditionType(node.condition->span, typeName(condType)));
		}

		// Determine result type from branches.
		if (thenType == T::Error) return elseType == T::Error ? T::Error : elseType;
		if (elseType == T::Error) return thenType;

		if (thenType == elseType) {
			if (thenType == T::Enum) {
				auto thenEnum = project.getEnumType(node.thenBranch.get());
				auto elseEnum = project.getEnumType(node.elseBranch.get());
				if (thenEnum.has_value() && elseEnum.has_value() && *thenEnum != *elseEnum) {
					diags.push_back(diagnostics::TernaryEnumMismatch(expr.span, *thenEnum, *elseEnum));
					return T::Error;
				}
				if (thenEnum.has_value()) {
					project.setEnumType(&expr, std::string(*thenEnum));
				}
			}
			return thenType;
		}

		// Both bool-compatible but different (e.g. Int + Bool) → unify to Bool.
		if (isBoolCompatible(thenType) && isBoolCompatible(elseType)) {
			diags.push_back(diagnostics::TernaryImplicitBool(expr.span, typeName(thenType), typeName(elseType)));
			return T::Bool;
		}

		diags.push_back(diagnostics::TernaryBranchMismatch(expr.span, typeName(thenType), typeName(elseType)));
		return T::Error;
	}

	/// Recursively resolve the type of an expression.
	ast::Type resolveExpr(ast::Expr& expr) {
		if (auto cached = project.getType(&expr)) {
			return *cached;
		}

		auto result = std::visit(
			[&](auto& node) { return resolve(node, expr); },
			expr.node);

		project.setType(&expr, result);
		return result;
	}

	template <typename IsParamRequired, typename GetParamName>
	ArgBindingResult bindFunctionCallArgs(
		const std::string& function,
		size_t nParams,
		const ast::CallExpr& node,
		const ast::Expr& expr,
		IsParamRequired&& isParamRequired,
		GetParamName&& getParamName)
	{
		ArgBindingResult result;
		result.argToParam.resize(node.args.size());
		result.paramBound.assign(nParams, false);

		std::unordered_map<std::string, size_t> paramIndexByName;
		for (size_t i = 0; i < nParams; ++i) {
			paramIndexByName.emplace(getParamName(i), i);
		}

		size_t nextPositionalParam = 0;
		bool hadTooManyArgs = false;

		for (size_t argIndex = 0; argIndex < node.args.size(); ++argIndex) {
			const auto& arg = node.args[argIndex];
			if (arg.name) {
				result.hasNamedArgs = true;
				auto it = paramIndexByName.find(arg.name->text);
				if (it == paramIndexByName.end()) {
					result.hasError = true;
					diags.push_back(diagnostics::UnknownNamedArgument(arg.value->span, function, arg.name->text));
					continue;
				}

				size_t paramIndex = it->second;
				if (result.paramBound[paramIndex]) {
					result.hasError = true;
					diags.push_back(diagnostics::DuplicateArgument(arg.value->span, function, arg.name->text));
					continue;
				}

				result.paramBound[paramIndex] = true;
				result.argToParam[argIndex] = paramIndex;
				continue;
			}

			while (nextPositionalParam < nParams && result.paramBound[nextPositionalParam]) {
				++nextPositionalParam;
			}

			if (nextPositionalParam >= nParams) {
				hadTooManyArgs = true;
				result.hasError = true;
				continue;
			}

			result.paramBound[nextPositionalParam] = true;
			result.argToParam[argIndex] = nextPositionalParam;
			++nextPositionalParam;
		}

		size_t required = 0;
		for (size_t i = 0; i < nParams; ++i) {
			if (isParamRequired(i)) ++required;
		}

		size_t nArgs = node.args.size();
		bool argCountOk = nArgs >= required && nArgs <= nParams;

		if (!result.hasNamedArgs && !argCountOk) {
			auto count = required == nParams
				? std::format("{}", required)
				: std::format("{}-{}", required, nParams);
			diags.push_back(diagnostics::ArgumentCountMismatch(expr.span, function, count, nArgs));
			result.hasError = true;
		}

		if (result.hasNamedArgs && hadTooManyArgs) {
			auto count = required == nParams
				? std::format("{}", required)
				: std::format("{}-{}", required, nParams);
			diags.push_back(diagnostics::ArgumentCountMismatch(expr.span, function, count, nArgs));
			result.hasError = true;
		}

		if (result.hasNamedArgs && !result.hasError) {
			std::vector<std::string> missingRequired;
			for (size_t i = 0; i < nParams; ++i) {
				if (isParamRequired(i) && !result.paramBound[i]) {
					missingRequired.push_back(getParamName(i));
				}
			}

			if (!missingRequired.empty()) {
				std::string missing = missingRequired.front();
				for (size_t i = 1; i < missingRequired.size(); ++i) {
					missing += ", ";
					missing += missingRequired[i];
				}
				diags.push_back(diagnostics::MissingRequiredArguments(expr.span, function, missing));
				result.hasError = true;
			}
		}

		return result;
	}

	template <typename GetParamType, typename GetParamEnumType>
	void validateBoundArgTypes(
		const std::string& function,
		std::vector<T>& argTypes,
		const ArgBindingResult& binding,
		const ast::CallExpr& node,
		GetParamType&& getParamType,
		GetParamEnumType&& getParamEnumType)
	{
		auto isCallArgCompatible = [](T expected, T actual) {
			if (expected == T::Condition) {
				return actual == T::Condition || isBoolCompatible(actual);
			}
			if (expected == T::Callable) {
				return actual == T::Callable || actual == T::Condition || isBoolCompatible(actual);
			}
			if (expected == T::Int) {
				return isIntCompatibleType(actual);
			}
			return actual == expected;
		};

		for (size_t argIndex = 0; argIndex < argTypes.size(); ++argIndex) {
			if (!binding.argToParam[argIndex]) continue;

			size_t paramIndex = *binding.argToParam[argIndex];
			auto paramType = getParamType(paramIndex);
			if (!paramType) continue;
			auto expectedEnum = *paramType == T::Enum
				? getParamEnumType(paramIndex)
				: std::optional<std::string_view>{};

			if (argTypes[argIndex] == T::Error
				&& inferUntypedParamIdentifier(*node.args[argIndex].value, *paramType)) {
				argTypes[argIndex] = *paramType;
				if (*paramType == T::Enum && expectedEnum.has_value()) {
					auto& argExpr = *node.args[argIndex].value;
					auto& identifier = std::get<ast::Identifier>(argExpr.node);
					enumScope[identifier.name.text] = std::string(*expectedEnum);
					project.setEnumType(&argExpr, std::string(*expectedEnum));
				}
			}

			if (argTypes[argIndex] == T::Error) continue;
			auto actualEnum = argTypes[argIndex] == T::Enum
				? project.getEnumType(node.args[argIndex].value.get())
				: std::optional<std::string_view>{};
			if (areDomainAndEnumCompatible(
					*paramType, expectedEnum, argTypes[argIndex], actualEnum)) {
				continue;
			}

			// For Enum-typed parameters with known identity, require the same enum.
			if (*paramType == T::Enum) {
				// Explicit int-conversion path for enum parameters.
				if (argTypes[argIndex] == T::Int) {
					if (expectedEnum.has_value()) {
						// Enum context is explicit via parameter identity.
						continue;
					}

					if (const auto* intLiteral = std::get_if<ast::IntLiteral>(&node.args[argIndex].value->node)) {
						auto candidateEnums = enumNamesWithExplicitValue(project, intLiteral->value);
						if (candidateEnums.size() > 1) {
							std::string enumList = candidateEnums.front();
							for (size_t i = 1; i < candidateEnums.size(); ++i) {
								enumList += ", ";
								enumList += candidateEnums[i];
							}
							diags.push_back(diagnostics::AmbiguousEnumInteger(
									node.args[argIndex].value->span, function, argIndex + 1, intLiteral->value, enumList));
							continue;
						}
					}

					// Generic Enum target with Int source is allowed if not provably ambiguous.
					continue;
				}

				if (expectedEnum.has_value() && argTypes[argIndex] == T::Enum) {
					if (!actualEnum.has_value() || *actualEnum != *expectedEnum) {
						diags.push_back(diagnostics::EnumArgumentMismatch(
							node.args[argIndex].value->span, function, argIndex + 1, *expectedEnum,
							actualEnum.has_value() ? *actualEnum : std::string_view{"<unknown>"}));
						continue;
					}
				}
			}

			if (isCallArgCompatible(*paramType, argTypes[argIndex])) continue;
			auto expectedName = expectedEnum.has_value()
				? std::format("enum '{}'", *expectedEnum)
				: std::string(typeName(*paramType));

			diags.push_back(diagnostics::ArgumentTypeMismatch(
				node.args[argIndex].value->span, function, argIndex + 1, expectedName, typeName(argTypes[argIndex])));
		}
	}

	template <typename GetDefaultValue>
	std::vector<const ast::Expr*> normalizeBoundCallArgs(
		const ast::CallExpr& node,
		const ArgBindingResult& binding,
		size_t nParams,
		GetDefaultValue&& getDefaultValue)
	{
		std::vector<const ast::Expr*> resolved(nParams, nullptr);
		for (size_t argIndex = 0; argIndex < node.args.size(); ++argIndex) {
			if (!binding.argToParam[argIndex]) continue;
			resolved[*binding.argToParam[argIndex]] = node.args[argIndex].value.get();
		}
		for (size_t i = 0; i < nParams; ++i) {
			if (resolved[i] == nullptr) {
				resolved[i] = getDefaultValue(i);
			}
		}
		return resolved;
	}

	std::optional<T> resolveExternParamType(const ast::ExternDefineDecl& ext, size_t index) {
		std::optional<T> paramType = project.getType(&ext.params[index]);
		if (!paramType && ext.params[index].type) {
			if (auto annotation = resolveTypeAnnotation(project, ext.params[index].type->name.text)) {
				paramType = annotation->type;
				project.setType(&ext.params[index], annotation->type);
				if (annotation->enumName.has_value()) {
					project.setEnumType(&ext.params[index], std::string(*annotation->enumName));
				}
			}
		}
		if (!paramType && ext.params[index].defaultValue) {
			auto inferredType = resolveExpr(*ext.params[index].defaultValue);
			if (inferredType != T::Error) {
				paramType = inferredType;
				project.setType(&ext.params[index], inferredType);
			}
		}
		return paramType;
	}

	ast::Type resolve(const ast::CallExpr& node, const ast::Expr& expr) {
		// Resolve all argument types first.
		std::vector<T> argTypes;
		argTypes.reserve(node.args.size());
		for (auto& arg : node.args) {
			argTypes.push_back(resolveExpr(*arg.value));
		}

		auto isCallableType = [](T type) {
			return type == T::Callable || type == T::Condition;
		};

		if (auto scopeIt = scope.find(node.callee.text); scopeIt != scope.end()) {
			if (!scopeIt->second.has_value()) {
				scopeIt->second = T::Condition;
			}

			auto calleeType = *scopeIt->second;
			if (!isCallableType(calleeType)) {
				diags.push_back(diagnostics::ValueNotCallable(expr.span, node.callee.text, typeName(calleeType)));
				return T::Error;
			}

			if (!node.args.empty()) {
				diags.push_back(diagnostics::ZeroArgumentCallMismatch(expr.span, node.callee.text, node.args.size()));
				return T::Error;
			}

			project.setResolvedCallArgs(&node, {});
			return T::Bool;
		}

		// Extern-defined host functions.
		if (auto it = project.ExternDefineDecls.find(node.callee.text);
			it != project.ExternDefineDecls.end()) {
			const auto& ext = *it->second;

			auto binding = bindFunctionCallArgs(
				node.callee.text,
				ext.params.size(),
				node,
				expr,
				[&](size_t i) { return !ext.params[i].defaultValue; },
				[&](size_t i) -> const std::string& { return ext.params[i].name.text; });

			validateBoundArgTypes(
				node.callee.text,
				argTypes,
				binding,
				node,
				[&](size_t i) { return resolveExternParamType(ext, i); },
				[&](size_t i) { return project.getEnumType(&ext.params[i]); });

			if (!binding.hasError) {
				project.setResolvedCallArgs(
					&node,
					normalizeBoundCallArgs(
						node,
						binding,
						ext.params.size(),
						[&](size_t i) { return ext.params[i].defaultValue.get(); }));
			}

			if (!ext.returnType) {
				return T::Error;
			}
			if (auto returnType = resolveTypeAnnotation(project, ext.returnType->name.text)) {
				if (returnType->enumName.has_value()) {
					project.setEnumType(&expr, std::string(*returnType->enumName));
				}
				return returnType->type;
			}
			return T::Error;
		}

		// User-defined functions (define declarations).
		if (auto it = project.DefineDecls.find(node.callee.text);
			it != project.DefineDecls.end()) {
			const auto& def = *it->second;

			auto binding = bindFunctionCallArgs(
				node.callee.text,
				def.params.size(),
				node,
				expr,
				[&](size_t i) { return !def.params[i].defaultValue; },
				[&](size_t i) -> const std::string& { return def.params[i].name.text; });

			validateBoundArgTypes(
				node.callee.text,
				argTypes,
				binding,
				node,
				[&](size_t i) { return project.getType(&def.params[i]); },
				[&](size_t i) { return project.getEnumType(&def.params[i]); });

			if (!binding.hasError) {
				project.setResolvedCallArgs(
					&node,
					normalizeBoundCallArgs(
						node,
						binding,
						def.params.size(),
						[&](size_t i) { return def.params[i].defaultValue.get(); }));
			}

			// Return the define's body type if available.
			if (auto bodyType = project.getType(def.body.get())) {
				if (*bodyType == T::Enum) {
					if (auto enumName = project.getEnumType(def.body.get()); enumName.has_value()) {
						project.setEnumType(&expr, std::string(*enumName));
					}
				}
				return *bodyType;
			}
			// Body not yet resolved — proper ordering in Step 7.
			return T::Error;
		}

		// Unknown function.
		diags.push_back(diagnostics::UnknownFunction(expr.span, node.callee.text));
		return T::Error;
	}

	ast::Type resolve(const ast::InvokeExpr& node, const ast::Expr& expr) {
		auto calleeType = resolveExpr(*node.callee);
		if (calleeType == T::Error) {
			return T::Error;
		}

		if (calleeType != T::Callable && calleeType != T::Condition) {
			diags.push_back(diagnostics::ExpressionNotCallable(expr.span, typeName(calleeType)));
			return T::Error;
		}

		// TODO: Nested Function Invocation Not Supported — InvokeExpr always returns Bool.
		// To support foo()()(), InvokeExpr would need to return Callable/Condition types
		// for chaining. Requires: (1) parameterized callable types, (2) higher-order logic,
		// or (3) explicit curry syntax.

		return T::Bool;
	}

	ast::Type resolve(const ast::MemberExpr& node, const ast::Expr& expr) {
		if (const auto* enumInfo = project.getEnumInfo(node.object.text); enumInfo != nullptr) {
			if (!enumContainsValueName(*enumInfo, node.member.text)) {
				diags.push_back(diagnostics::UnknownEnumMember(expr.span, node.member.text, node.object.text));
				return T::Error;
			}

			project.setEnumType(&expr, node.object.text);
			return T::Enum;
		}

		diags.push_back(diagnostics::UnknownEnum(expr.span, node.object.text));
		return T::Error;
	}

	ast::Type resolve(ast::HereRef& node, ast::Expr& expr) {
		if (!currentRegion.has_value()) {
			diags.push_back(diagnostics::HereOutsideRegion(expr.span));
			return T::Error;
		}
		node.resolvedRegion = *currentRegion;
		return T::Region;
	}

	ast::Type resolve(const ast::MatchExpr& node, const ast::Expr& expr) {
		using T = ast::Type;

		auto identifierText = [](const ast::Expr& matchExpr) -> std::optional<std::string_view> {
			if (auto* id = std::get_if<ast::Identifier>(&matchExpr.node)) {
				return id->name.text;
			}
			return std::nullopt;
		};

		auto patternDisplayName = [](const ast::Expr& matchExpr) -> std::string {
			if (auto* id = std::get_if<ast::Identifier>(&matchExpr.node)) {
				return id->name.text;
			}
			if (auto* member = std::get_if<ast::MemberExpr>(&matchExpr.node)) {
				return std::format("{}.{}", member->object.text, member->member.text);
			}
			return "<expr>";
		};

		auto enumIdentityOfExpr = [&](const ast::Expr& e) -> std::optional<std::string> {
			if (auto enumIdentity = project.getEnumType(&e); enumIdentity.has_value()) {
				return std::string(*enumIdentity);
			}

			if (auto* member = std::get_if<ast::MemberExpr>(&e.node)) {
				if (project.getEnumInfo(member->object.text) != nullptr) {
					return std::string(member->object.text);
				}
			}

			return std::nullopt;
		};

		// --- Discriminant --------------------------------------------------
		auto discrimType = resolveExpr(*node.discriminant);
		auto discrimName = identifierText(*node.discriminant);

		// --- Arm patterns: all must be the same enum type ---------------
		std::optional<T> patternType;
		std::optional<std::string> patternEnumIdentity;

		for (size_t armIndex = 0; armIndex < node.arms.size(); ++armIndex) {
			const auto& arm = node.arms[armIndex];

			if (arm.isDefault) {
				if (!arm.patterns.empty()) {
					diags.push_back(diagnostics::MatchWildcardNotStandalone(expr.span));
				}

				if (armIndex + 1 != node.arms.size()) {
					diags.push_back(diagnostics::MatchWildcardNotLast(expr.span));
				}

				continue;
			}

			for (const auto& pattern : arm.patterns) {
				auto currentPatternType = resolveExpr(*pattern);
				if (currentPatternType == T::Error) {
					continue;
				}

				auto currentPatternEnumIdentity = enumIdentityOfExpr(*pattern);

				if (!patternType) {
					patternType = currentPatternType;
					if (currentPatternType == T::Enum) {
						patternEnumIdentity = currentPatternEnumIdentity;
					}
				} else if (currentPatternType != *patternType) {
					auto patternName = patternDisplayName(*pattern);
					diags.push_back(diagnostics::MatchPatternTypeMismatch(
						expr.span, patternName, typeName(currentPatternType), typeName(*patternType)));
				} else if (currentPatternType == T::Enum
					&& patternEnumIdentity.has_value()
					&& currentPatternEnumIdentity.has_value()
					&& *currentPatternEnumIdentity != *patternEnumIdentity) {
					auto patternName = patternDisplayName(*pattern);
					diags.push_back(diagnostics::MatchPatternEnumMismatch(
						expr.span, patternName, *currentPatternEnumIdentity, *patternEnumIdentity));
				}
			}
		}

		// --- Unify discriminant type with pattern type ------------------
		if (patternType) {
			if (discrimType == T::Error) {
				if (!inferUntypedParamIdentifier(*node.discriminant, *patternType) && discrimName) {
					if (auto it = scope.find(std::string(*discrimName)); it != scope.end() && !it->second) {
						it->second = *patternType;
						if (*patternType == T::Enum && patternEnumIdentity.has_value()) {
							enumScope[std::string(*discrimName)] = *patternEnumIdentity;
						}
					}
				}

				if (*patternType == T::Enum && patternEnumIdentity.has_value()) {
					project.setEnumType(node.discriminant.get(), *patternEnumIdentity);
					if (discrimName.has_value()) {
						enumScope[std::string(*discrimName)] = *patternEnumIdentity;
					}
				}
			} else if (discrimType != *patternType) {
				auto name = discrimName.value_or("<expr>");
				diags.push_back(diagnostics::MatchDiscriminantTypeMismatch(
					expr.span, name, typeName(discrimType), typeName(*patternType)));
			} else if (discrimType == T::Enum && patternEnumIdentity.has_value()) {
				auto discrimEnumIdentity = enumIdentityOfExpr(*node.discriminant);
				if (discrimEnumIdentity.has_value() && *discrimEnumIdentity != *patternEnumIdentity) {
					auto name = discrimName.value_or("<expr>");
					diags.push_back(diagnostics::MatchDiscriminantEnumMismatch(
						expr.span, name, *discrimEnumIdentity, *patternEnumIdentity));
				}
			}
		}

		// --- Arm bodies: resolve and unify types -----------------------
		T bodyType = T::Error;
		for (auto& arm : node.arms) {
			auto armType = resolveExpr(*arm.body);
			if (armType == T::Error) continue;

			if (bodyType == T::Error) {
				bodyType = armType;
			} else if (armType != bodyType) {
				if (isBoolCompatible(armType) && isBoolCompatible(bodyType)) {
					diags.push_back(diagnostics::MatchArmImplicitBool(
						arm.body->span, typeName(armType), typeName(bodyType)));
					bodyType = T::Bool;
				} else {
					diags.push_back(diagnostics::MatchArmTypeMismatch(
						arm.body->span, typeName(armType), typeName(bodyType)));
				}
			}
		}

		return bodyType;
	}
};

// == Step 7: Topological ordering of defines =================================

/// Collect all call names from an expression tree, then filter to only
/// those that are user-defined functions (defines).
static void collectDefineCalls(
	const ast::Expr& expr,
	const std::map<std::string, const ast::DefineDecl*>& defines,
	std::unordered_set<std::string>& out)
{
	std::visit([&](const auto& node) {
		using N = std::decay_t<decltype(node)>;
		if constexpr (std::is_same_v<N, ast::Identifier>) {
			if (defines.contains(node.name.text)) {
				out.insert(node.name.text);
			}
		} else if constexpr (std::is_same_v<N, ast::UnaryExpr>) {
			collectDefineCalls(*node.operand, defines, out);
		} else if constexpr (std::is_same_v<N, ast::BinaryExpr>) {
			collectDefineCalls(*node.left, defines, out);
			collectDefineCalls(*node.right, defines, out);
		} else if constexpr (std::is_same_v<N, ast::TernaryExpr>) {
			collectDefineCalls(*node.condition, defines, out);
			collectDefineCalls(*node.thenBranch, defines, out);
			collectDefineCalls(*node.elseBranch, defines, out);
		} else if constexpr (std::is_same_v<N, ast::CallExpr>) {
			if (defines.contains(node.callee.text)) {
				out.insert(node.callee.text);
			}
			for (const auto& arg : node.args) {
				collectDefineCalls(*arg.value, defines, out);
			}
		} else if constexpr (std::is_same_v<N, ast::InvokeExpr>) {
			collectDefineCalls(*node.callee, defines, out);
		} else if constexpr (std::is_same_v<N, ast::MatchExpr>) {
			collectDefineCalls(*node.discriminant, defines, out);
			for (const auto& arm : node.arms) {
				for (const auto& pattern : arm.patterns) {
					collectDefineCalls(*pattern, defines, out);
				}
				collectDefineCalls(*arm.body, defines, out);
			}
		} else if constexpr (std::is_same_v<N, ast::ListExpr>) {
			for (const auto& element : node.elements) {
				collectDefineCalls(*element, defines, out);
			}
		}
	}, expr.node);
}

/// DFS helper for topological sort. Post-order traversal ensures callees
/// appear before callers in the output.
/// When a back edge is found, extracts the cycle from the path stack.
static void topoSortDfs(
	const std::string& name,
	const std::unordered_map<std::string, std::unordered_set<std::string>>& callees,
	std::unordered_map<std::string, int>& marks,
	std::vector<std::string>& order,
	std::vector<std::string>& path,
	std::vector<std::vector<std::string>>& cycles)
{
	if (marks[name] == 2) return;       // Perm — already processed
	if (marks[name] == 1) {             // Temp — back edge = cycle
		// Extract just the cycle portion from the path.
		std::vector<std::string> cycle;
		for (auto it = path.rbegin(); it != path.rend(); ++it) {
			cycle.push_back(*it);
			if (*it == name) break;
		}
		std::reverse(cycle.begin(), cycle.end());
		cycles.push_back(std::move(cycle));
		return;
	}
	marks[name] = 1;
	path.push_back(name);
	if (auto it = callees.find(name); it != callees.end()) {
		for (const auto& callee : it->second) {
			topoSortDfs(callee, callees, marks, order, path, cycles);
		}
	}
	path.pop_back();
	marks[name] = 2;
	order.push_back(name);
}

/// Topologically sort defines by their call graph.
/// Returns define names in dependency order: callees before callers.
/// Emits an error diagnostic if a cycle is detected.
static std::vector<std::string> topoSortDefines(
	const std::map<std::string, const ast::DefineDecl*>& defines,
	std::vector<ast::Diagnostic>& diags)
{
	// Build call graph: name → set of defines it calls.
	std::unordered_map<std::string, std::unordered_set<std::string>> callees;
	for (const auto& [name, decl] : defines) {
		auto& calls = callees[name];
		collectDefineCalls(*decl->body, defines, calls);
		for (const auto& param : decl->params) {
			if (param.defaultValue) {
				collectDefineCalls(*param.defaultValue, defines, calls);
			}
		}
	}

	// DFS-based topological sort with cycle detection.
	std::unordered_map<std::string, int> marks;
	for (const auto& [name, _] : defines) {
		marks[name] = 0;
	}

	std::vector<std::string> order;
	std::vector<std::string> path;
	std::vector<std::vector<std::string>> cycles;
	for (const auto& [name, _] : defines) {
		if (marks[name] == 0) {
			topoSortDfs(name, callees, marks, order, path, cycles);
		}
	}

	for (const auto& cycle : cycles) {
		std::string names = cycle.front();
		for (size_t i = 1; i < cycle.size(); ++i) {
			names += " -> ";
			names += cycle[i];
		}
		names += " -> ";
		names += cycle.front();
		diags.push_back(diagnostics::DefineCycle({}, names));
	}

	return order;
}

// == Top-level walk ===========================================================

std::vector<ast::Diagnostic> resolveTypes(ast::Project& project) {
	std::vector<ast::Diagnostic> diags;

	// Resolve define bodies in dependency order (callees first).
	auto defineOrder = topoSortDefines(project.DefineDecls, diags);

	for (const auto& name : defineOrder) {
		const auto* decl = project.DefineDecls.at(name);
		Scope scope;
		EnumIdentityScope enumScope;
		for (const auto& param : decl->params) {
			std::optional<ast::Type> annotatedType;
			if (param.type) {
				if (auto annotation = resolveTypeAnnotation(project, param.type->name.text)) {
					annotatedType = annotation->type;
					if (annotation->enumName.has_value()) {
						project.setEnumType(&param, std::string(*annotation->enumName));
						enumScope[param.name.text] = std::string(*annotation->enumName);
					}
				} else {
					diags.push_back(diagnostics::UnknownParameterTypeAnnotation(
						decl->span, param.type->name.text, param.name.text));
				}
			}

			std::optional<ast::Type> defaultType;
			if (param.defaultValue) {
				Scope noScope;
				EnumIdentityScope noEnumScope;
				ExprResolver defaultResolver{project, noScope, noEnumScope, diags};
				auto resolvedDefaultType =
					defaultResolver.resolveExpr(*param.defaultValue);
				if (resolvedDefaultType != ast::Type::Error) {
					defaultType = resolvedDefaultType;
				}
			}

			if (annotatedType) {
				scope[param.name.text] = *annotatedType;
				project.setType(&param, *annotatedType);
				if (*annotatedType == ast::Type::Enum && param.defaultValue
					&& !project.getEnumType(&param).has_value()) {
					auto defaultEnum = project.getEnumType(param.defaultValue.get());
					if (defaultEnum.has_value()) {
						project.setEnumType(&param, std::string(*defaultEnum));
						enumScope[param.name.text] = std::string(*defaultEnum);
					} else {
						enumScope[param.name.text] = std::nullopt;
					}
				} else if (*annotatedType == ast::Type::Enum
					&& !enumScope.contains(param.name.text)) {
					enumScope[param.name.text] = std::nullopt;
				}
			} else if (defaultType) {
				scope[param.name.text] = *defaultType;
				project.setType(&param, *defaultType);
				if (*defaultType == ast::Type::Enum && param.defaultValue) {
					auto defaultEnum = project.getEnumType(param.defaultValue.get());
					if (defaultEnum.has_value()) {
						project.setEnumType(&param, std::string(*defaultEnum));
						enumScope[param.name.text] = std::string(*defaultEnum);
					} else {
						enumScope[param.name.text] = std::nullopt;
					}
				} else if (*defaultType == ast::Type::Enum) {
					enumScope[param.name.text] = std::nullopt;
				}
			} else {
				// No annotation, no default — type unknown
				// until body-usage inference.
				scope[param.name.text] = std::nullopt;
				enumScope[param.name.text] = std::nullopt;
			}
		}
		ExprResolver resolver{project, scope, enumScope, diags};
		resolver.resolveExpr(*decl->body);

		for (const auto& param : decl->params) {
			if (project.getType(&param).has_value()) {
				continue;
			}

			auto scopeIt = scope.find(param.name.text);
			if (scopeIt != scope.end() && scopeIt->second.has_value()) {
				project.setType(&param, *scopeIt->second);
				if (*scopeIt->second == ast::Type::Enum) {
					if (auto enumIt = enumScope.find(param.name.text);
						enumIt != enumScope.end() && enumIt->second.has_value()) {
						project.setEnumType(&param, *enumIt->second);
					}
				}
			}
		}
	}

	// Resolve extern define parameter annotations/defaults.
	for (const auto& [name, decl] : project.ExternDefineDecls) {
		for (const auto& param : decl->params) {
			std::optional<ast::Type> annotatedType;
			if (param.type) {
				if (auto annotation = resolveTypeAnnotation(project, param.type->name.text)) {
					annotatedType = annotation->type;
					if (annotation->enumName.has_value()) {
						project.setEnumType(&param, std::string(*annotation->enumName));
					}
				} else {
					diags.push_back(diagnostics::UnknownParameterTypeAnnotation(
						decl->span, param.type->name.text, param.name.text));
				}
			}

			std::optional<ast::Type> defaultType;
			if (param.defaultValue) {
				Scope noScope;
				EnumIdentityScope noEnumScope;
				ExprResolver defaultResolver{project, noScope, noEnumScope, diags};
				auto resolvedDefaultType =
					defaultResolver.resolveExpr(*param.defaultValue);
				if (resolvedDefaultType != ast::Type::Error) {
					defaultType = resolvedDefaultType;
				}
			}

			if (annotatedType) {
				project.setType(&param, *annotatedType);
				if (*annotatedType == ast::Type::Enum && param.defaultValue
					&& !project.getEnumType(&param).has_value()) {
					auto defaultEnum = project.getEnumType(param.defaultValue.get());
					if (defaultEnum.has_value()) {
						project.setEnumType(&param, std::string(*defaultEnum));
					}
				}
			} else if (defaultType) {
				project.setType(&param, *defaultType);
				if (*defaultType == ast::Type::Enum && param.defaultValue) {
					auto defaultEnum = project.getEnumType(param.defaultValue.get());
					if (defaultEnum.has_value()) {
						project.setEnumType(&param, std::string(*defaultEnum));
					}
				}
			}
		}
	}

	// Resolve region entry conditions.
	{
		for (auto& [name, decl] : project.RegionDecls) {
			Scope regionScope;
			EnumIdentityScope regionEnumScope;
			ExprResolver resolver{project, regionScope, regionEnumScope, diags};
			resolver.currentRegion = decl->key;
			for (auto& data : decl->body.data) {
				resolver.resolveExpr(*data.value);
			}
			for (const auto& section : decl->body.sections) {
				for (const auto& entry : section.entries) {
					resolver.resolveExpr(*entry.condition);
				}
			}
		}
	}

	// Resolve extend-region entry conditions.
	{
		for (const auto& [name, decls] : project.ExtendRegionDecls) {
			for (const auto* decl : decls) {
				Scope extendScope;
				EnumIdentityScope extendEnumScope;
				ExprResolver resolver{project, extendScope, extendEnumScope, diags};
				resolver.currentRegion = ast::Name(name);
				for (const auto& section : decl->sections) {
					for (const auto& entry : section.entries) {
						resolver.resolveExpr(*entry.condition);
					}
				}
			}
		}
	}

	return diags;
}

} // namespace rls::sema
