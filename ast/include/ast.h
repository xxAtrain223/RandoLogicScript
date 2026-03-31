#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace rls::ast {

// == Source tracking ==========================================================

/// A position within a source file (1-based line and column).
struct Position {
	uint32_t line = 0;
	uint32_t column = 0;
};

/// A span of source text: the file it came from plus start/end positions.
struct Span {
	std::string file;
	Position start;
	Position end;
};

// == Forward declarations =====================================================

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

// == Enumerations =============================================================

enum class UnaryOp { Not };

enum class BinaryOp {
	// Logical
	And, Or,
	// Comparison
	Eq, NotEq, Lt, LtEq, Gt, GtEq,
	// Arithmetic
	Add, Sub, Mul, Div,
};

enum class Keyword {
	IsChild, IsAdult,
	AtDay, AtNight,
	IsVanilla, IsMq,
};

enum class SectionKind { Events, Locations, Exits };

enum class TimePasses { Auto, Yes, No };

enum class EnemyFieldKind { Kill, Pass, Drop, Avoid };

// == Expression leaf nodes ====================================================

/// Boolean literal: `true`, `false`, `always`, `never`.
struct BoolLiteral {
	bool value;
};

/// Integer literal: `0`, `1`, `48`, etc.
struct IntLiteral {
	int value;
};

/// Named identifier: enum values (`RG_HOOKSHOT`), parameters (`distance`), etc.
struct Identifier {
	std::string name;
};

/// Built-in keyword: `is_child`, `is_adult`, `at_day`, `at_night`,
/// `is_vanilla`, `is_mq`.
struct KeywordExpr {
	Keyword keyword;
};

// == Expression compound nodes ================================================

/// Unary expression: `not <operand>`.
struct UnaryExpr {
	UnaryOp op;
	ExprPtr operand;

	UnaryExpr(UnaryOp op, ExprPtr operand)
		: op(op), operand(std::move(operand)) {}
};

/// Binary expression: `<left> <op> <right>`.
struct BinaryExpr {
	BinaryOp op;
	ExprPtr left;
	ExprPtr right;

	BinaryExpr(BinaryOp op, ExprPtr left, ExprPtr right)
		: op(op), left(std::move(left)), right(std::move(right)) {}
};

/// Ternary expression: `<condition> ? <thenBranch> : <elseBranch>`.
struct TernaryExpr {
	ExprPtr condition;
	ExprPtr thenBranch;
	ExprPtr elseBranch;

	TernaryExpr(ExprPtr condition, ExprPtr thenBranch, ExprPtr elseBranch)
		: condition(std::move(condition)),
		  thenBranch(std::move(thenBranch)),
		  elseBranch(std::move(elseBranch)) {}
};

/// A function call argument, either positional or named (`param: value`).
struct Arg {
	std::optional<std::string> name;
	ExprPtr value;

	Arg(std::optional<std::string> name, ExprPtr value)
		: name(std::move(name)), value(std::move(value)) {}
};

/// Function call: `name(arg1, arg2, param: arg3)`.
struct CallExpr {
	std::string function;
	std::vector<Arg> args;

	CallExpr(std::string function, std::vector<Arg> args)
		: function(std::move(function)), args(std::move(args)) {}
};

/// One branch of a `shared` block: `from <region>: <condition>` or
/// `from here: <condition>`.
struct SharedBranch {
	std::optional<std::string> region; // nullopt means "from here"
	ExprPtr condition;

	SharedBranch(std::optional<std::string> region, ExprPtr condition)
		: region(std::move(region)), condition(std::move(condition)) {}
};

/// Shared/multi-region check: `shared [any_age] { from ... }`.
struct SharedBlock {
	bool anyAge;
	std::vector<SharedBranch> branches;

	SharedBlock(bool anyAge, std::vector<SharedBranch> branches)
		: anyAge(anyAge), branches(std::move(branches)) {}
};

/// Any-age evaluation block: `any_age { <body> }`.
struct AnyAgeBlock {
	ExprPtr body;

	AnyAgeBlock(ExprPtr body) : body(std::move(body)) {}
};

/// One arm of a `match` expression.
struct MatchArm {
	std::vector<std::string> patterns;  // one or more enum identifiers
	ExprPtr body;
	bool fallthrough;                   // trailing `or` for OR-accumulation

	MatchArm(std::vector<std::string> patterns, ExprPtr body, bool fallthrough)
		: patterns(std::move(patterns)),
		  body(std::move(body)),
		  fallthrough(fallthrough) {}
};

/// Match expression: `match <discriminant> { <arms> }`.
struct MatchExpr {
	std::string discriminant;
	std::vector<MatchArm> arms;

	MatchExpr(std::string discriminant, std::vector<MatchArm> arms)
		: discriminant(std::move(discriminant)), arms(std::move(arms)) {}
};

// == Expr wrapper =============================================================

/// The central expression node. Wraps a variant of all expression types plus
/// a source location for error reporting.
struct Expr {
	using Variant = std::variant<
		BoolLiteral,
		IntLiteral,
		Identifier,
		KeywordExpr,
		UnaryExpr,
		BinaryExpr,
		TernaryExpr,
		CallExpr,
		SharedBlock,
		AnyAgeBlock,
		MatchExpr
	>;

	Variant node;
	Span span;

	Expr(Variant node, Span span = {})
		: node(std::move(node)), span(span) {}
};

/// Helper to construct an ExprPtr from any expression node type.
template <typename T>
ExprPtr makeExpr(T&& node, Span span = {}) {
	return std::make_unique<Expr>(std::forward<T>(node), span);
}

// == Declaration support types ================================================

/// A parameter in a `define` or enemy field: `name`, optional `: type`,
/// optional `= default`.
struct Param {
	std::string name;
	std::optional<std::string> type;
	ExprPtr defaultValue; // nullptr if no default

	Param(std::string name, std::optional<std::string> type, ExprPtr defaultValue)
		: name(std::move(name)),
		  type(std::move(type)),
		  defaultValue(std::move(defaultValue)) {}
};

/// A single entry in a region section: `NAME: condition`.
struct Entry {
	std::string name;
	ExprPtr condition;
	Span span;

	Entry(std::string name, ExprPtr condition, Span span = {})
		: name(std::move(name)),
		  condition(std::move(condition)),
		  span(span) {}
};

/// A region section: `events { ... }`, `locations { ... }`, or `exits { ... }`.
struct Section {
	SectionKind kind;
	std::vector<Entry> entries;

	Section(SectionKind kind, std::vector<Entry> entries)
		: kind(kind), entries(std::move(entries)) {}
};

/// Region body: properties and sections shared by `region` and `extend region`.
struct RegionBody {
	std::optional<std::string> scene;
	TimePasses timePasses = TimePasses::Auto;
	std::vector<std::string> areas;
	std::vector<Section> sections;

	RegionBody(
		std::optional<std::string> scene,
		TimePasses timePasses,
		std::vector<std::string> areas,
		std::vector<Section> sections)
		: scene(std::move(scene)),
		  timePasses(timePasses),
		  areas(std::move(areas)),
		  sections(std::move(sections)) {}
};

// == Top-level declarations ===================================================

/// `region RR_NAME { ... }`
struct RegionDecl {
	std::string name;
	RegionBody body;
	Span span;

	RegionDecl(std::string name, RegionBody body, Span span = {})
		: name(std::move(name)),
		  body(std::move(body)),
		  span(span) {}
};

/// `extend region RR_NAME { ... }`
/// Extensions can only add sections, not redefine scene, time_passes, or areas.
struct ExtendRegionDecl {
	std::string name;
	std::vector<Section> sections;
	Span span;

	ExtendRegionDecl(std::string name, std::vector<Section> sections,
	                 Span span = {})
		: name(std::move(name)),
		  sections(std::move(sections)),
		  span(span) {}
};

/// `define name(params): body`
struct DefineDecl {
	std::string name;
	std::vector<Param> params;
	ExprPtr body;
	Span span;

	DefineDecl(
		std::string name,
		std::vector<Param> params,
		ExprPtr body,
		Span span = {})
		: name(std::move(name)),
		  params(std::move(params)),
		  body(std::move(body)),
		  span(span) {}
};

/// One field of an `enemy` declaration: `kill`, `pass`, `drop`, or `avoid`.
struct EnemyField {
	EnemyFieldKind kind;
	std::vector<Param> params;
	ExprPtr body;
	Span span;

	EnemyField(
		EnemyFieldKind kind,
		std::vector<Param> params,
		ExprPtr body,
		Span span = {})
		: kind(kind),
		  params(std::move(params)),
		  body(std::move(body)),
		  span(span) {}
};

/// `enemy RE_NAME { kill: ..., pass: ..., drop: ..., avoid: ... }`
struct EnemyDecl {
	std::string name;
	std::vector<EnemyField> fields;
	Span span;

	EnemyDecl(std::string name, std::vector<EnemyField> fields,
	          Span span = {})
		: name(std::move(name)),
		  fields(std::move(fields)),
		  span(span) {}
};

/// A top-level declaration: region, extend region, define, or enemy.
using Decl = std::variant<RegionDecl, ExtendRegionDecl, DefineDecl, EnemyDecl>;

// == File =====================================================================

/// Root AST node representing an entire `.rls` file.
struct File {
	std::string path;  // owned canonical path of the source file
	std::vector<Decl> declarations;
};

// == Project ==================================================================

/// Aggregated AST for all `.rls` files in a project.
/// All top-level declarations are globally visible (no import mechanism).
struct Project {
	std::vector<File> files;

	/// Flat view of every declaration across all files.
	/// Useful for semantic analysis passes that don't care about file boundaries.
	std::vector<Decl*> allDeclarations() {
		std::vector<Decl*> result;
		for (auto& file : files)
			for (auto& decl : file.declarations)
				result.push_back(&decl);
		return result;
	}
};

} // namespace rls::ast
