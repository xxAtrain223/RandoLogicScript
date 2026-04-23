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

// == Step 2: Built-in function signatures ====================================

struct HostParam {
	ast::Type type;
	bool required;
};

struct HostSignature {
	ast::Type returnType;
	std::vector<HostParam> params;
};

/// Enemy built-in function signatures.
/// These take an Enemy as first arg and dispatch to enemy declarations.
static const std::unordered_map<std::string, HostSignature>& enemyBuiltins() {
	using T = ast::Type;
	static const std::unordered_map<std::string, HostSignature> table = {
		// can_kill(enemy, distance = ED_CLOSE, wallOrFloor = true, quantity = 1, timer = false, inWater = false)
		{"can_kill",     {T::Bool, {{T::Enemy, true}, {T::Distance, false}, {T::Bool, false}, {T::Int, false}, {T::Bool, false}, {T::Bool, false}}}},
		// can_pass(enemy, distance = ED_CLOSE, wallOrFloor = true)
		{"can_pass",     {T::Bool, {{T::Enemy, true}, {T::Distance, false}, {T::Bool, false}}}},
		// can_avoid(enemy, grounded = false, quantity = 1)
		{"can_avoid",    {T::Bool, {{T::Enemy, true}, {T::Bool, false}, {T::Int, false}}}},
		// can_get_drop(enemy, distance = ED_CLOSE, aboveLink = false)
		{"can_get_drop", {T::Bool, {{T::Enemy, true}, {T::Distance, false}, {T::Bool, false}}}},
	};
	return table;
}

// == Step 4: Scope for parameters ============================================

/// Maps parameter names to their types. nullopt = not yet inferred.
using Scope = std::unordered_map<std::string, std::optional<ast::Type>>;



/// Map an EnemyFieldKind to its corresponding built-in function name.
static const char* enemyFieldBuiltinName(ast::EnemyFieldKind kind) {
	switch (kind) {
	case ast::EnemyFieldKind::Kill:  return "can_kill";
	case ast::EnemyFieldKind::Pass:  return "can_pass";
	case ast::EnemyFieldKind::Avoid: return "can_avoid";
	case ast::EnemyFieldKind::Drop:  return "can_get_drop";
	}
	return nullptr;
}

/// Validate a call's arguments against a known signature.
/// Returns the signature's return type.
static ast::Type validateCallArgs(
	const std::string& function, const HostSignature& sig,
	const std::vector<ast::Type>& argTypes,
	const ast::CallExpr& node, const ast::Expr& expr,
	std::vector<ast::Diagnostic>& diags)
{
	using T = ast::Type;

	size_t required = 0;
	for (const auto& p : sig.params) {
		if (p.required) ++required;
	}

	if (argTypes.size() < required || argTypes.size() > sig.params.size()) {
		if (required == sig.params.size()) {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' expects {} argument(s), got {}",
					function, required, argTypes.size()),
				expr.span
			});
		} else {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' expects {}-{} argument(s), got {}",
					function, required, sig.params.size(),
					argTypes.size()),
				expr.span
			});
		}
		return sig.returnType;
	}

	// Check each provided argument's type.
	for (size_t i = 0; i < argTypes.size(); ++i) {
		if (argTypes[i] == T::Error) continue;
		if (argTypes[i] != sig.params[i].type) {
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' argument {} expected {}, got {}",
					function, i + 1,
					typeName(sig.params[i].type),
					typeName(argTypes[i])),
				node.args[i].value->span
			});
		}
	}
	return sig.returnType;
}

// == Step 3: Bottom-up expression typing =====================================

/// Visitor that resolves and type-checks all expression nodes.
/// Holds shared context (project, scope, diagnostics) so individual
/// handlers only need the node and its wrapping Expr.
struct ExprResolver {
	ast::Project& project;
	Scope& scope;
	std::vector<ast::Diagnostic>& diags;

	using T = ast::Type;

	// -- Node handlers -------------------------------------------------------

	ast::Type resolve(const ast::BoolLiteral&, const ast::Expr&) {
		return ast::Type::Bool;
	}

	ast::Type resolve(const ast::IntLiteral&, const ast::Expr&) {
		return ast::Type::Int;
	}

	ast::Type resolve(const ast::KeywordExpr&, const ast::Expr&) {
		return ast::Type::Bool;
	}

	ast::Type resolve(const ast::Identifier& node, const ast::Expr& expr) {
		// Check scope first (parameter names).
		if (auto it = scope.find(node.name); it != scope.end()) {
			if (it->second) {
				return *it->second;
			}
			// Parameter exists but type not yet inferred (Step 5).
			return ast::Type::Error;
		}

		if (auto t = typeFromIdentifier(node.name)) {
			return *t;
		}
		diags.push_back({
			ast::DiagnosticLevel::Error,
			std::format("unknown identifier '{}'", node.name),
			expr.span
		});
		return ast::Type::Error;
	}

	ast::Type resolve(const ast::UnaryExpr& node, const ast::Expr& expr) {
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

	ast::Type resolve(const ast::BinaryExpr& node, const ast::Expr& expr) {
		using T = ast::Type;
		auto leftType = resolveExpr(*node.left);
		auto rightType = resolveExpr(*node.right);

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

	ast::Type resolve(const ast::TernaryExpr& node, const ast::Expr& expr) {
		using T = ast::Type;
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
	ast::Type resolveExpr(const ast::Expr& expr) {
		if (auto cached = project.getType(&expr)) {
			return *cached;
		}

		auto result = std::visit(
			[&](auto& node) { return resolve(node, expr); },
			expr.node);

		project.setType(&expr, result);
		return result;
	}

	template <typename GetParamType>
	bool validateFunctionCallArgs(
		const std::string& function,
		size_t required,
		size_t nParams,
		const std::vector<T>& argTypes,
		const ast::CallExpr& node,
		const ast::Expr& expr,
		GetParamType&& getParamType)
	{
		size_t nArgs = argTypes.size();
		bool argCountOk = nArgs >= required && nArgs <= nParams;

		if (!argCountOk) {
			auto count = required == nParams
				? std::format("{}", required)
				: std::format("{}-{}", required, nParams);
			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' expects {} argument(s), got {}",
					function, count, nArgs),
				expr.span
			});
			return false;
		}

		for (size_t i = 0; i < nArgs; ++i) {
			if (argTypes[i] == T::Error) continue;

			auto paramType = getParamType(i);
			if (!paramType) continue;
			if (argTypes[i] == *paramType) continue;

			diags.push_back({
				ast::DiagnosticLevel::Error,
				std::format("'{}' argument {} expected {}, got {}",
					function, i + 1,
					typeName(*paramType), typeName(argTypes[i])),
				node.args[i].value->span
			});
		}

		return true;
	}

	std::optional<T> resolveExternParamType(const ast::ExternDefineDecl& ext, size_t index) {
		std::optional<T> paramType = project.getType(&ext.params[index]);
		if (!paramType && ext.params[index].type) {
			if (auto t = typeFromAnnotation(*ext.params[index].type)) {
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

		// Enemy built-in functions (can_kill, can_pass, etc.).
		auto& enemies = enemyBuiltins();
		if (auto it = enemies.find(node.function); it != enemies.end()) {
			return validateCallArgs(node.function, it->second, argTypes,
				node, expr, diags);
		}

		// Extern-defined host functions.
		if (auto it = project.ExternDefineDecls.find(node.function);
			it != project.ExternDefineDecls.end()) {
			const auto& ext = *it->second;

			size_t required = 0;
			for (const auto& p : ext.params) {
				if (!p.defaultValue) ++required;
			}

			validateFunctionCallArgs(
				node.function,
				required,
				ext.params.size(),
				argTypes,
				node,
				expr,
				[&](size_t i) { return resolveExternParamType(ext, i); });

			if (!ext.returnType) {
				return T::Error;
			}
			if (auto returnType = typeFromAnnotation(*ext.returnType)) {
				return *returnType;
			}
			return T::Error;
		}

		// User-defined functions (define declarations).
		if (auto it = project.DefineDecls.find(node.function);
			it != project.DefineDecls.end()) {
			const auto& def = *it->second;

			// Count required params (those without defaults).
			size_t required = 0;
			for (const auto& p : def.params) {
				if (!p.defaultValue) ++required;
			}

			validateFunctionCallArgs(
				node.function,
				required,
				def.params.size(),
				argTypes,
				node,
				expr,
				[&](size_t i) { return project.getType(&def.params[i]); });

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
			std::format("unknown function '{}'", node.function),
			expr.span
		});
		return T::Error;
	}

	ast::Type resolve(const ast::SharedBlock& node, const ast::Expr&) {
		for (auto& branch : node.branches) {
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

		// --- Discriminant: look up in scope (should be a parameter) ---
		std::optional<T> discrimType;
		bool discrimInScope = false;
		if (auto it = scope.find(node.discriminant); it != scope.end()) {
			discrimInScope = true;
			discrimType = it->second; // may be nullopt if untyped
		}

		// --- Arm patterns: all must be the same enum type ---------------
		std::optional<T> patternType;

		for (const auto& arm : node.arms) {
			for (const auto& pattern : arm.patterns) {
				if (auto t = typeFromIdentifier(pattern)) {
					if (!patternType) {
						patternType = *t;
					} else if (*t != *patternType) {
						diags.push_back({
							ast::DiagnosticLevel::Error,
							std::format(
								"match pattern '{}' is {} but expected {}",
								pattern, typeName(*t),
								typeName(*patternType)),
							expr.span
						});
					}
				} else {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format("unrecognized match pattern '{}'",
							pattern),
						expr.span
					});
				}
			}
		}

		// --- Unify discriminant type with pattern type ------------------
		if (patternType) {
			if (discrimType) {
				// Both typed — they must agree.
				if (*discrimType != *patternType) {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"match discriminant '{}' is {} "
							"but patterns are {}",
							node.discriminant,
							typeName(*discrimType),
							typeName(*patternType)),
						expr.span
					});
				}
			} else if (discrimInScope) {
				// Discriminant is a parameter with no type — infer it.
				scope[node.discriminant] = *patternType;
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
	std::unordered_set<std::string> allCalls;
	collectCallNames(expr, allCalls);
	for (auto& name : allCalls) {
		if (defines.contains(name)) {
			out.insert(std::move(name));
		}
	}
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
				if (auto t = typeFromAnnotation(*param.type)) {
					annotatedType = *t;
				} else {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"unknown type annotation '{}' "
							"for parameter '{}'",
							*param.type, param.name),
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
				scope[param.name] = *annotatedType;
				project.setType(&param, *annotatedType);
			} else if (defaultType) {
				scope[param.name] = *defaultType;
				project.setType(&param, *defaultType);
			} else {
				// No annotation, no default — type unknown
				// until body-usage inference.
				scope[param.name] = std::nullopt;
			}
		}
		ExprResolver resolver{project, scope, diags};
		resolver.resolveExpr(*decl->body);
	}

	// Resolve extern define parameter annotations/defaults.
	for (const auto& [name, decl] : project.ExternDefineDecls) {
		for (const auto& param : decl->params) {
			std::optional<ast::Type> annotatedType;
			if (param.type) {
				if (auto t = typeFromAnnotation(*param.type)) {
					annotatedType = *t;
				} else {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"unknown type annotation '{}' "
							"for parameter '{}'",
							*param.type, param.name),
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

	// Resolve enemy field bodies.
	for (auto& [name, decl] : project.EnemyDecls) {
		for (const auto& field : decl->fields) {
			Scope scope;
			auto* builtinName = enemyFieldBuiltinName(field.kind);
			auto& builtins = enemyBuiltins();
			if (auto it = builtins.find(builtinName);
				it != builtins.end())
			{
				const auto& sig = it->second;
				// Skip first param (Enemy) — field params
				// correspond to remaining signature params.
				for (size_t i = 0; i < field.params.size(); ++i) {
					size_t sigIdx = i + 1;
					if (sigIdx < sig.params.size()) {
						scope[field.params[i].name] =
							sig.params[sigIdx].type;
					}
				}
			}
			ExprResolver resolver{project, scope, diags};
			auto bodyType = resolver.resolveExpr(*field.body);
			if (bodyType != ast::Type::Error
				&& !isBoolCompatible(bodyType))
			{
				diags.push_back({
					ast::DiagnosticLevel::Error,
					std::format(
						"enemy field '{}' body must be Bool, got {}",
						builtinName, typeName(bodyType)),
					field.body->span
				});
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
		for (auto& [name, decl] : project.ExtendRegionDecls) {
			for (const auto& section : decl->sections) {
				for (const auto& entry : section.entries) {
					resolver.resolveExpr(*entry.condition);
				}
			}
		}
	}

	// Resolve shared from here
	{
		std::function<void(const std::string&, const rls::ast::ExprPtr&)> populateHere = [&](const std::string& region, const rls::ast::ExprPtr& expr) {
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
					populateHere(name, entry.condition);
				}
			}
		}

		for (auto& [name, region] : project.ExtendRegionDecls) {
			for (auto& section : region->sections) {
				for (auto& entry : section.entries) {
					populateHere(name, entry.condition);
				}
			}
		}
	}

	return diags;
}

} // namespace rls::sema
