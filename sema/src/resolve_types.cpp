#include "resolve_types.h"
#include "type_helpers.h"

#include <format>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace rls::sema {

// == Step 1: Enum prefix resolution ==========================================

std::optional<ast::Type> typeFromIdentifier(std::string_view name) {
	struct PrefixEntry {
		std::string_view prefix;
		ast::Type type;
	};

	// Longer prefixes first where they share a starting substring.
	static constexpr PrefixEntry table[] = {
		{"LOGIC_",   ast::Type::Logic},
		{"SCENE_",   ast::Type::Scene},
		{"DUNGEON_", ast::Type::Dungeon},
		{"RG_",      ast::Type::Item},
		{"RE_",      ast::Type::Enemy},
		{"ED_",      ast::Type::Distance},
		{"RT_",      ast::Type::Trick},
		{"RSK_",     ast::Type::Setting},
		{"RO_",      ast::Type::Setting},
		{"RR_",      ast::Type::Region},
		{"RC_",      ast::Type::Check},
		{"RA_",      ast::Type::Area},
		{"TK_",      ast::Type::Trial},
		{"WL_",      ast::Type::WaterLevel},
	};

	for (const auto& [prefix, type] : table) {
		if (name.starts_with(prefix)) {
			return type;
		}
	}
	return std::nullopt;
}

// == Step 4: Scope for parameters ============================================

/// Maps parameter names to their types. nullopt = not yet inferred.
using Scope = std::unordered_map<std::string, std::optional<ast::Type>>;

// == Step 3: Bottom-up expression typing =====================================

/// Visitor that resolves and type-checks all expression nodes.
/// Holds shared context (project, scope, diagnostics) so individual
/// handlers only need the node and its wrapping Expr.
struct ExprResolver {
	ast::Project& project;
	Scope& scope;
	std::vector<ast::Diagnostic>& diags;

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

	ast::Type resolve(ast::Identifier& node, ast::Expr& expr) {
		// Check scope first (parameter names).
		if (auto it = scope.find(node.name.text); it != scope.end()) {
			node.kind = ast::IdentifierKind::Parameter;
			if (it->second) {
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
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("function '{}' requires {} argument(s); only zero-argument functions can be used as callable values",
						node.name.text, def->params.size()),
					expr.span
				});
				return ast::Type::Error;
			}

			auto bodyType = project.getType(def->body.get());
			if (!bodyType.has_value()) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("function '{}' callable reference type is not available yet", node.name.text),
					expr.span
				});
				return ast::Type::Error;
			}

			// TODO: Bool-Return-Type Constraint — only () -> Bool functions become Condition.
			if (*bodyType != ast::Type::Bool) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("function '{}' cannot be used as a Condition callable because it returns {}",
						node.name.text, typeName(*bodyType)),
					expr.span
				});
				return ast::Type::Error;
			}

			node.kind = ast::IdentifierKind::FunctionRef;
			return ast::Type::Condition;
		}

		if (auto extIt = project.ExternDefineDecls.find(node.name.text);
			extIt != project.ExternDefineDecls.end()) {
			const auto* ext = extIt->second;
			if (!ext->params.empty()) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("function '{}' requires {} argument(s); only zero-argument functions can be used as callable values",
						node.name.text, ext->params.size()),
					expr.span
				});
				return ast::Type::Error;
			}

			if (!ext->returnType) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("function '{}' cannot be used as callable value without a return type", node.name.text),
					expr.span
				});
				return ast::Type::Error;
			}

			auto returnType = typeFromAnnotation(ext->returnType->name.text);
			if (!returnType.has_value() || *returnType != ast::Type::Bool) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("function '{}' cannot be used as a Condition callable because it returns {}",
						node.name.text,
						returnType.has_value() ? typeName(*returnType) : std::string_view{"<unknown>"}),
					expr.span
				});
				return ast::Type::Error;
			}

			node.kind = ast::IdentifierKind::FunctionRef;
			return ast::Type::Condition;
		}

		if (auto t = typeFromIdentifier(node.name.text)) {
			node.kind = ast::IdentifierKind::EnumValue;
			return *t;
		}
		diags.push_back({
			ast::DiagnosticLevel::Error,
			std::format("unknown identifier '{}'", node.name.text),
			expr.span
		});
		return ast::Type::Error;
	}

	ast::Type resolve(const ast::UnaryExpr& node, ast::Expr& expr) {
		inferUntypedParamIdentifier(*node.operand, ast::Type::Bool);
		auto opType = resolveExpr(*node.operand);
		if (opType != ast::Type::Error && !isBoolCompatible(opType)) {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'not' requires a Bool operand, got {}",
					typeName(opType)),
				expr.span
			});
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
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("'{}' requires Bool operands, left is {}",
						opName, typeName(leftType)),
					node.left->span
				});
			}
			if (rightType != T::Error && !isBoolCompatible(rightType)) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("'{}' requires Bool operands, right is {}",
						opName, typeName(rightType)),
					node.right->span
				});
			}
			return T::Bool;
		}

		// Equality: both sides must be the same type.
		case ast::BinaryOp::Eq:
		case ast::BinaryOp::NotEq:
			if (leftType != T::Error && rightType != T::Error
				&& leftType != rightType) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("comparison between incompatible types {} and {}",
						typeName(leftType), typeName(rightType)),
					expr.span
				});
			}
			return T::Bool;

		// Ordering: both sides must be Int.
		case ast::BinaryOp::Lt:
		case ast::BinaryOp::LtEq:
		case ast::BinaryOp::Gt:
		case ast::BinaryOp::GtEq:
			if (leftType != T::Error && leftType != T::Int) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("comparison requires Int operands, left is {}",
						typeName(leftType)),
					node.left->span
				});
			}
			if (rightType != T::Error && rightType != T::Int) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("comparison requires Int operands, right is {}",
						typeName(rightType)),
					node.right->span
				});
			}
			return T::Bool;

		// Arithmetic: both sides must be Int.
		case ast::BinaryOp::Add:
		case ast::BinaryOp::Sub:
		case ast::BinaryOp::Mul:
		case ast::BinaryOp::Div:
			if (leftType != T::Error && leftType != T::Int) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("arithmetic requires Int operands, left is {}",
						typeName(leftType)),
					node.left->span
				});
			}
			if (rightType != T::Error && rightType != T::Int) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("arithmetic requires Int operands, right is {}",
						typeName(rightType)),
					node.right->span
				});
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
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("ternary condition must be Bool, got {}",
					typeName(condType)),
				node.condition->span
			});
		}

		// Determine result type from branches.
		if (thenType == T::Error) return elseType == T::Error ? T::Error : elseType;
		if (elseType == T::Error) return thenType;

		if (thenType == elseType) return thenType;

		// Both bool-compatible but different (e.g. Int + Bool) → unify to Bool.
		if (isBoolCompatible(thenType) && isBoolCompatible(elseType)) {
			diags.push_back({
				ast::DiagnosticLevel::Warning,
				std::format("ternary branches have types {} and {}, implicitly converted to Bool",
					typeName(thenType), typeName(elseType)),
				expr.span
			});
			return T::Bool;
		}

		diags.push_back({
			ast::DiagnosticLevel::Error,
			std::format("ternary branches have different types: {} and {}",
				typeName(thenType), typeName(elseType)),
			expr.span
		});
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
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format("'{}' unknown named argument '{}'",
							function, arg.name->text),
						arg.value->span
					});
					continue;
				}

				size_t paramIndex = it->second;
				if (result.paramBound[paramIndex]) {
					result.hasError = true;
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format("'{}' duplicate argument for parameter '{}'",
							function, arg.name->text),
						arg.value->span
					});
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
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' expects {} argument(s), got {}",
					function, count, nArgs),
				expr.span
			});
			result.hasError = true;
		}

		if (result.hasNamedArgs && hadTooManyArgs) {
			auto count = required == nParams
				? std::format("{}", required)
				: std::format("{}-{}", required, nParams);
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' expects {} argument(s), got {}",
					function, count, nArgs),
				expr.span
			});
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
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("'{}' missing required argument(s): {}",
						function, missing),
					expr.span
				});
				result.hasError = true;
			}
		}

		return result;
	}

	template <typename GetParamType>
	void validateBoundArgTypes(
		const std::string& function,
		std::vector<T>& argTypes,
		const ArgBindingResult& binding,
		const ast::CallExpr& node,
		GetParamType&& getParamType)
	{
		auto isCallArgCompatible = [](T expected, T actual) {
			if (expected == T::Condition) {
				return actual == T::Condition || isBoolCompatible(actual);
			}
			if (expected == T::Callable) {
				return actual == T::Callable || actual == T::Condition || isBoolCompatible(actual);
			}
			return actual == expected;
		};

		for (size_t argIndex = 0; argIndex < argTypes.size(); ++argIndex) {
			if (!binding.argToParam[argIndex]) continue;

			size_t paramIndex = *binding.argToParam[argIndex];
			auto paramType = getParamType(paramIndex);
			if (!paramType) continue;

			if (argTypes[argIndex] == T::Error
				&& inferUntypedParamIdentifier(*node.args[argIndex].value, *paramType)) {
				argTypes[argIndex] = *paramType;
			}

			if (argTypes[argIndex] == T::Error) continue;
			if (isCallArgCompatible(*paramType, argTypes[argIndex])) continue;

			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' argument {} expected {}, got {}",
					function, argIndex + 1,
					typeName(*paramType), typeName(argTypes[argIndex])),
				node.args[argIndex].value->span
			});
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
			if (auto t = typeFromAnnotation(ext.params[index].type->name.text)) {
				paramType = *t;
				project.setType(&ext.params[index], *t);
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
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("'{}' is not callable (type {})",
						node.callee.text, typeName(calleeType)),
					expr.span
				});
				return T::Error;
			}

			if (!node.args.empty()) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("'{}' expects 0 argument(s), got {}",
						node.callee.text, node.args.size()),
					expr.span
				});
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
				[&](size_t i) { return resolveExternParamType(ext, i); });

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
			if (auto returnType = typeFromAnnotation(ext.returnType->name.text)) {
				return *returnType;
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
				[&](size_t i) { return project.getType(&def.params[i]); });

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
				return *bodyType;
			}
			// Body not yet resolved — proper ordering in Step 7.
			return T::Error;
		}

		// Unknown function.
		diags.push_back({
			ast::DiagnosticLevel::Error,
			std::format("unknown function '{}'", node.callee.text),
			expr.span
		});
		return T::Error;
	}

	ast::Type resolve(const ast::InvokeExpr& node, const ast::Expr& expr) {
		auto calleeType = resolveExpr(*node.callee);
		if (calleeType == T::Error) {
			return T::Error;
		}

		if (calleeType != T::Callable && calleeType != T::Condition) {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("expression is not callable (type {})", typeName(calleeType)),
				expr.span
			});
			return T::Error;
		}

		// TODO: Nested Function Invocation Not Supported — InvokeExpr always returns Bool.
		// To support foo()()(), InvokeExpr would need to return Callable/Condition types
		// for chaining. Requires: (1) parameterized callable types, (2) higher-order logic,
		// or (3) explicit curry syntax.

		return T::Bool;
	}

	ast::Type resolve(const ast::SharedBlock& node, const ast::Expr&) {
		for (auto& branch : node.branches) {
			inferUntypedParamIdentifier(*branch.condition, ast::Type::Bool);
			auto branchType = resolveExpr(*branch.condition);
			if (branchType != ast::Type::Error && !isBoolCompatible(branchType)) {
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format("shared branch condition must be Bool, got {}",
						typeName(branchType)),
					branch.condition->span
				});
			}
		}
		return ast::Type::Bool;
	}

	ast::Type resolve(const ast::AnyAgeBlock& node, const ast::Expr&) {
		inferUntypedParamIdentifier(*node.body, ast::Type::Bool);
		auto bodyType = resolveExpr(*node.body);
		if (bodyType != ast::Type::Error && !isBoolCompatible(bodyType)) {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("any_age body must be Bool, got {}",
					typeName(bodyType)),
				node.body->span
			});
		}
		return ast::Type::Bool;
	}

	ast::Type resolve(const ast::MatchExpr& node, const ast::Expr& expr) {
		using T = ast::Type;

		auto identifierText = [](const ast::Expr& matchExpr) -> std::optional<std::string_view> {
			if (auto* id = std::get_if<ast::Identifier>(&matchExpr.node)) {
				return id->name.text;
			}
			return std::nullopt;
		};

		// --- Discriminant --------------------------------------------------
		auto discrimType = resolveExpr(*node.discriminant);
		auto discrimName = identifierText(*node.discriminant);

		// --- Arm patterns: all must be the same enum type ---------------
		std::optional<T> patternType;

		for (size_t armIndex = 0; armIndex < node.arms.size(); ++armIndex) {
			const auto& arm = node.arms[armIndex];

			if (arm.isDefault) {
				if (!arm.patterns.empty()) {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						"match wildcard '_' must be a standalone pattern",
						expr.span
					});
				}

				if (armIndex + 1 != node.arms.size()) {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						"match wildcard '_' arm must be last",
						expr.span
					});
				}

				continue;
			}

			for (const auto& pattern : arm.patterns) {
				auto currentPatternType = resolveExpr(*pattern);
				if (currentPatternType == T::Error) {
					continue;
				}

				if (!patternType) {
					patternType = currentPatternType;
				} else if (currentPatternType != *patternType) {
					auto patternName = identifierText(*pattern).value_or("<expr>");
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"match pattern '{}' is {} but expected {}",
							patternName, typeName(currentPatternType),
							typeName(*patternType)),
						expr.span
					});
				}
			}
		}

		// --- Unify discriminant type with pattern type ------------------
		if (patternType) {
			if (discrimType == T::Error) {
				if (!inferUntypedParamIdentifier(*node.discriminant, *patternType) && discrimName) {
					if (auto it = scope.find(std::string(*discrimName)); it != scope.end() && !it->second) {
						it->second = *patternType;
					}
				}
			} else if (discrimType != *patternType) {
				auto name = discrimName.value_or("<expr>");
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format(
						"match discriminant '{}' is {} but patterns are {}",
						name,
						typeName(discrimType),
						typeName(*patternType)),
					expr.span
				});
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
					diags.push_back({
						ast::DiagnosticLevel::Warning,
						std::format(
							"match arm type {} implicitly "
							"converted to Bool (previous arms are {})",
							typeName(armType), typeName(bodyType)),
						arm.body->span
					});
					bodyType = T::Bool;
				} else {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"match arm type {} doesn't match "
							"previous arms ({})",
							typeName(armType), typeName(bodyType)),
						arm.body->span
					});
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
		} else if constexpr (std::is_same_v<N, ast::SharedBlock>) {
			for (const auto& branch : node.branches) {
				collectDefineCalls(*branch.condition, defines, out);
			}
		} else if constexpr (std::is_same_v<N, ast::AnyAgeBlock>) {
			collectDefineCalls(*node.body, defines, out);
		} else if constexpr (std::is_same_v<N, ast::MatchExpr>) {
			collectDefineCalls(*node.discriminant, defines, out);
			for (const auto& arm : node.arms) {
				for (const auto& pattern : arm.patterns) {
					collectDefineCalls(*pattern, defines, out);
				}
				collectDefineCalls(*arm.body, defines, out);
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
		diags.push_back({
			ast::DiagnosticLevel::Error,
			std::format("cycle in define call graph: {}", names),
			{}
		});
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
		for (const auto& param : decl->params) {
			std::optional<ast::Type> annotatedType;
			if (param.type) {
				if (auto t = typeFromAnnotation(param.type->name.text)) {
					annotatedType = *t;
				} else {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"unknown type annotation '{}' "
							"for parameter '{}'",
							param.type->name.text, param.name.text),
						decl->span
					});
				}
			}

			std::optional<ast::Type> defaultType;
			if (param.defaultValue) {
				Scope noScope;
				ExprResolver defaultResolver{project, noScope, diags};
				auto resolvedDefaultType =
					defaultResolver.resolveExpr(*param.defaultValue);
				if (resolvedDefaultType != ast::Type::Error) {
					defaultType = resolvedDefaultType;
				}
			}

			if (annotatedType) {
				scope[param.name.text] = *annotatedType;
				project.setType(&param, *annotatedType);
			} else if (defaultType) {
				scope[param.name.text] = *defaultType;
				project.setType(&param, *defaultType);
			} else {
				// No annotation, no default — type unknown
				// until body-usage inference.
				scope[param.name.text] = std::nullopt;
			}
		}
		ExprResolver resolver{project, scope, diags};
		resolver.resolveExpr(*decl->body);

		for (const auto& param : decl->params) {
			if (project.getType(&param).has_value()) {
				continue;
			}

			auto scopeIt = scope.find(param.name.text);
			if (scopeIt != scope.end() && scopeIt->second.has_value()) {
				project.setType(&param, *scopeIt->second);
			}
		}
	}

	// Resolve extern define parameter annotations/defaults.
	for (const auto& [name, decl] : project.ExternDefineDecls) {
		for (const auto& param : decl->params) {
			std::optional<ast::Type> annotatedType;
			if (param.type) {
				if (auto t = typeFromAnnotation(param.type->name.text)) {
					annotatedType = *t;
				} else {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"unknown type annotation '{}' "
							"for parameter '{}'",
							param.type->name.text, param.name.text),
						decl->span
					});
				}
			}

			std::optional<ast::Type> defaultType;
			if (param.defaultValue) {
				Scope noScope;
				ExprResolver defaultResolver{project, noScope, diags};
				auto resolvedDefaultType =
					defaultResolver.resolveExpr(*param.defaultValue);
				if (resolvedDefaultType != ast::Type::Error) {
					defaultType = resolvedDefaultType;
				}
			}

			if (annotatedType) {
				project.setType(&param, *annotatedType);
			} else if (defaultType) {
				project.setType(&param, *defaultType);
			}
		}
	}

	// Resolve region entry conditions.
	{
		Scope regionScope;
		ExprResolver resolver{project, regionScope, diags};
		for (auto& [name, decl] : project.RegionDecls) {
			for (const auto& section : decl->body.sections) {
				for (const auto& entry : section.entries) {
					resolver.resolveExpr(*entry.condition);
				}
			}
		}
	}

	// Resolve extend-region entry conditions.
	{
		Scope extendScope;
		ExprResolver resolver{project, extendScope, diags};
		for (const auto& [name, decls] : project.ExtendRegionDecls) {
			for (const auto* decl : decls) {
				for (const auto& section : decl->sections) {
					for (const auto& entry : section.entries) {
						resolver.resolveExpr(*entry.condition);
					}
				}
			}
		}
	}

	// Resolve shared from here
	{
		std::function<void(const ast::Name&, const rls::ast::ExprPtr&)> populateHere = [&](const ast::Name& region, const rls::ast::ExprPtr& expr) {
			std::visit([&](auto& node) -> void {
				using T = std::decay_t<decltype(node)>;
				if constexpr (std::is_same_v<T, ast::UnaryExpr>) {
					populateHere(region, node.operand);
				}
				else if constexpr (std::is_same_v<T, ast::BinaryExpr>) {
					populateHere(region, node.left);
					populateHere(region, node.right);
				}
				else if constexpr (std::is_same_v<T, ast::TernaryExpr>) {
					populateHere(region, node.condition);
					populateHere(region, node.thenBranch);
					populateHere(region, node.elseBranch);
				}
				else if constexpr (std::is_same_v<T, ast::CallExpr>) {
					for (auto& arg : node.args) {
						populateHere(region, arg.value);
					}
				}
				else if constexpr (std::is_same_v<T, ast::SharedBlock>) {
					for (auto& branch : node.branches) {
						if (branch.region == std::nullopt) {
							branch.region = region;
						}
						populateHere(region, branch.condition);
					}
				}
				else if constexpr (std::is_same_v<T, ast::AnyAgeBlock>) {
					populateHere(region, node.body);
				}
				else if constexpr (std::is_same_v<T, ast::MatchExpr>) {
					for (auto& arm : node.arms) {
						populateHere(region, arm.body);
					}
				}
			}, expr->node);
		};

		for (auto& [name, region] : project.RegionDecls) {
			for (auto& section : region->body.sections) {
				for (auto& entry : section.entries) {
					populateHere(region->key, entry.condition);
				}
			}
		}

		for (const auto& [name, decls] : project.ExtendRegionDecls) {
			for (const auto* decl : decls) {
				for (const auto& section : decl->sections) {
					for (const auto& entry : section.entries) {
						populateHere(ast::Name(name), entry.condition);
					}
				}
			}
		}
	}

	return diags;
}

} // namespace rls::sema
