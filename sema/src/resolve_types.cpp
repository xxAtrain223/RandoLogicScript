#include "resolve_types.h"

#include <format>
#include <string>
#include <unordered_map>

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

// == Step 2: Host function signatures ========================================

struct HostParam {
	ast::Type type;
	bool required;
};

struct HostSignature {
	ast::Type returnType;
	std::vector<HostParam> params;
};

static const std::unordered_map<std::string, HostSignature>& hostFunctions() {
	using T = ast::Type;
	static const std::unordered_map<std::string, HostSignature> table = {
		{"has",              {T::Bool, {{T::Item, true}}}},
		{"can_use",          {T::Bool, {{T::Item, true}}}},
		{"keys",             {T::Bool, {{T::Scene, true}, {T::Int, true}}}},
		{"flag",             {T::Bool, {{T::Logic, true}}}},
		{"setting",          {T::Setting, {{T::Setting, true}}}},
		{"trick",            {T::Bool, {{T::Trick, true}}}},
		{"fire_timer",       {T::Int, {}}},
		{"water_timer",      {T::Int, {}}},
		{"hearts",           {T::Int, {}}},
		{"effective_health", {T::Int, {}}},
		{"stone_count",      {T::Int, {}}},
		{"ocarina_buttons",  {T::Int, {}}},
		{"water_level",      {T::Bool, {{T::WaterLevel, true}}}},
		{"trial_skipped",    {T::Bool, {{T::Trial, true}}}},
		{"check_price",      {T::Int, {{T::Check, false}}}},
		{"wallet_capacity",  {T::Int, {}}},
		{"can_plant_bean",   {T::Bool, {{T::Region, true}, {T::Item, true}}}},
		{"bean_planted",     {T::Bool, {{T::Item, true}}}},
		{"triforce_pieces",  {T::Int, {}}},
		{"big_poes",         {T::Int, {}}},
	};
	return table;
}

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

// == Helpers ==================================================================

static std::string_view typeName(ast::Type t) {
	switch (t) {
	case ast::Type::Bool:       return "Bool";
	case ast::Type::Int:        return "Int";
	case ast::Type::Item:       return "Item";
	case ast::Type::Enemy:      return "Enemy";
	case ast::Type::Distance:   return "Distance";
	case ast::Type::Trick:      return "Trick";
	case ast::Type::Setting:    return "Setting";
	case ast::Type::Region:     return "Region";
	case ast::Type::Check:      return "Check";
	case ast::Type::Logic:      return "Logic";
	case ast::Type::Scene:      return "Scene";
	case ast::Type::Dungeon:    return "Dungeon";
	case ast::Type::Area:       return "Area";
	case ast::Type::Trial:      return "Trial";
	case ast::Type::WaterLevel: return "WaterLevel";
	case ast::Type::Void:       return "Void";
	case ast::Type::Error:      return "<error>";
	}
	return "<unknown>";
}

/// Returns true if the type can be implicitly used where Bool is expected.
/// Int and Setting both have truthiness (zero/non-zero).
static bool isBoolCompatible(ast::Type t) {
	return t == ast::Type::Bool
		|| t == ast::Type::Int
		|| t == ast::Type::Setting;
}

/// Validate a call's arguments against a known signature.
/// Returns the signature's return type.
static ast::Type validateCallArgs(
	const std::string& function, const HostSignature& sig,
	const std::vector<ast::Type>& argTypes,
	ast::CallExpr& node, ast::Expr& expr,
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

// Forward declaration — each node handler may recurse.
static ast::Type resolveExpr(
	ast::Expr& expr,
	ast::Project& project,
	std::vector<ast::Diagnostic>& diags);

// -- Node handlers -----------------------------------------------------------

static ast::Type resolve(
	ast::BoolLiteral&, ast::Expr&, ast::Project&,
	std::vector<ast::Diagnostic>&)
{
	return ast::Type::Bool;
}

static ast::Type resolve(
	ast::IntLiteral&, ast::Expr&, ast::Project&,
	std::vector<ast::Diagnostic>&)
{
	return ast::Type::Int;
}

static ast::Type resolve(
	ast::KeywordExpr&, ast::Expr&, ast::Project&,
	std::vector<ast::Diagnostic>&)
{
	return ast::Type::Bool;
}

static ast::Type resolve(
	ast::Identifier& node, ast::Expr& expr, ast::Project&,
	std::vector<ast::Diagnostic>& diags)
{
	if (auto t = typeFromIdentifier(node.name)) {
		return *t;
	}
	// No enum prefix — could be a parameter (Step 4 adds scope).
	diags.push_back({
		ast::DiagnosticLevel::Error,
		std::format("unknown identifier '{}'", node.name),
		expr.span
	});
	return ast::Type::Error;
}

static ast::Type resolve(
	ast::UnaryExpr& node, ast::Expr& expr, ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	auto opType = resolveExpr(*node.operand, project, diags);
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

static ast::Type resolve(
	ast::BinaryExpr& node, ast::Expr& expr, ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	using T = ast::Type;
	auto leftType = resolveExpr(*node.left, project, diags);
	auto rightType = resolveExpr(*node.right, project, diags);

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

static ast::Type resolve(
	ast::TernaryExpr& node, ast::Expr& expr, ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	using T = ast::Type;
	auto condType = resolveExpr(*node.condition, project, diags);
	auto thenType = resolveExpr(*node.thenBranch, project, diags);
	auto elseType = resolveExpr(*node.elseBranch, project, diags);

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

static ast::Type resolve(
	ast::CallExpr& node, ast::Expr& expr, ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	using T = ast::Type;

	// Resolve all argument types first.
	std::vector<T> argTypes;
	argTypes.reserve(node.args.size());
	for (auto& arg : node.args) {
		argTypes.push_back(resolveExpr(*arg.value, project, diags));
	}

	// Enemy built-in functions (can_kill, can_pass, etc.).
	auto& enemies = enemyBuiltins();
	if (auto it = enemies.find(node.function); it != enemies.end()) {
		return validateCallArgs(node.function, it->second, argTypes,
			node, expr, diags);
	}

	// Host functions.
	auto& hosts = hostFunctions();
	if (auto it = hosts.find(node.function); it != hosts.end()) {
		return validateCallArgs(node.function, it->second, argTypes,
			node, expr, diags);
	}

	// User-defined functions (define declarations).
	if (auto it = project.DefineDecls.find(node.function);
		it != project.DefineDecls.end()) {
		// If the define's body has been typed, use its return type.
		// Full arg checking is deferred to Step 5.
		if (auto bodyType = project.getType(it->second->body.get())) {
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

static ast::Type resolve(
	ast::SharedBlock& node, ast::Expr&, ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	for (auto& branch : node.branches) {
		auto branchType = resolveExpr(*branch.condition, project, diags);
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

static ast::Type resolve(
	ast::AnyAgeBlock& node, ast::Expr&, ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	auto bodyType = resolveExpr(*node.body, project, diags);
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

static ast::Type resolve(
	ast::MatchExpr& node, ast::Expr& expr, ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	using T = ast::Type;

	// Infer discriminant type from arm patterns.
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

	// Resolve arm bodies and find common type.
	T bodyType = T::Error;
	for (auto& arm : node.arms) {
		auto armType = resolveExpr(*arm.body, project, diags);
		if (armType != T::Error) {
			if (bodyType == T::Error) {
				bodyType = armType;
			} else if (armType != bodyType) {
				if (isBoolCompatible(armType) && isBoolCompatible(bodyType)) {
					diags.push_back({
						ast::DiagnosticLevel::Warning,
						std::format(
							"match arm body type {} implicitly "
							"converted to Bool (previous arms are {})",
							typeName(armType), typeName(bodyType)),
						arm.body->span
					});
					bodyType = T::Bool;
				} else {
					diags.push_back({
						ast::DiagnosticLevel::Error,
						std::format(
							"match arm body type {} doesn't match "
							"previous arms ({})",
							typeName(armType), typeName(bodyType)),
						arm.body->span
					});
				}
			}
		}
	}

	return bodyType;
}

// -- Dispatch ----------------------------------------------------------------

static ast::Type resolveExpr(
	ast::Expr& expr,
	ast::Project& project,
	std::vector<ast::Diagnostic>& diags)
{
	if (auto cached = project.getType(&expr)) {
		return *cached;
	}

	auto result = std::visit(
		[&](auto& node) { return resolve(node, expr, project, diags); },
		expr.node);

	project.setType(&expr, result);
	return result;
}

// == Top-level walk ===========================================================

std::vector<ast::Diagnostic> resolveTypes(ast::Project& project) {
	std::vector<ast::Diagnostic> diags;

	for (auto& file : project.files) {
		for (auto& decl : file.declarations) {
			std::visit([&](auto& d) {
				using D = std::decay_t<decltype(d)>;

				if constexpr (std::is_same_v<D, ast::DefineDecl>) {
					// Process define bodies first so their return types
					// are available when regions call them.
					resolveExpr(*d.body, project, diags);
				}
				else if constexpr (std::is_same_v<D, ast::EnemyDecl>) {
					for (auto& field : d.fields) {
						resolveExpr(*field.body, project, diags);
					}
				}
			}, decl);
		}
	}

	// Now resolve region/extend entry conditions (which may call defines).
	for (auto& file : project.files) {
		for (auto& decl : file.declarations) {
			std::visit([&](auto& d) {
				using D = std::decay_t<decltype(d)>;

				if constexpr (std::is_same_v<D, ast::RegionDecl>) {
					for (auto& section : d.body.sections) {
						for (auto& entry : section.entries) {
							resolveExpr(*entry.condition, project, diags);
						}
					}
				}
				else if constexpr (std::is_same_v<D, ast::ExtendRegionDecl>) {
					for (auto& section : d.sections) {
						for (auto& entry : section.entries) {
							resolveExpr(*entry.condition, project, diags);
						}
					}
				}
			}, decl);
		}
	}

	return diags;
}

} // namespace rls::sema
