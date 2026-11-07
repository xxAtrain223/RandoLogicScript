#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "ast.h"

namespace rls::transpilers::soh_tracker::ir {

struct NodeId {
	uint32_t value;
	bool operator==(const NodeId&) const = default;
};

struct NodeCount {
	uint64_t value;
	bool operator==(const NodeCount&) const = default;
};

// Handles refer to descriptor slots, independently of final consumer numbering.
struct NodeHandle {
	size_t slot;
	bool operator==(const NodeHandle&) const = default;
};

struct HelperHandle {
	size_t slot;
	bool operator==(const HelperHandle&) const = default;
};

struct AdapterHandle {
	size_t slot;
	bool operator==(const AdapterHandle&) const = default;
};

enum class IntegralTarget { Bool, Int8, Int16, Int32, UInt8, UInt16, UInt32 };
struct ConditionTarget {
	bool operator==(const ConditionTarget&) const = default;
};
using Representation = std::variant<IntegralTarget, ConditionTarget>;

struct ValueType {
	ast::Type sourceType;
	const ast::EnumInfo* enumInfo;
	Representation representation;
};

using ValueTypeResult = std::variant<ValueType, ast::Diagnostic>;
std::variant<int32_t, ast::Diagnostic> CheckInt32Value(int64_t value, const ast::Span& span);
ValueTypeResult GetValueType(const ast::Project& project, const ast::Expr& expression);
ValueTypeResult GetValueType(const ast::Project& project, const ast::Param& parameter);

struct Provenance {
	ast::Span span;
	// Retain eliminated operator occurrences when a chain becomes one n-ary node.
	std::vector<const ast::Expr*> expressions;
};

struct ChainSources {
	Provenance provenance;
	std::vector<const ast::Expr*> operands;
};

// Shared lowering policy: logical chains flatten both sides; arithmetic only the left.
std::variant<ChainSources, ast::Diagnostic> CollectChainSources(
	const ast::Project& project, const ast::Expr& expression);

struct EnumConstant {
	const ast::EnumInfo* declaration;
	const ast::Name* member;
};

struct KeyConstant {
	const ast::Name* name;
};

struct ParameterRead {
	const ast::Param* declaration;
};

struct HereValue {
	const ast::RegionDecl* region;
};

struct Value {
	std::variant<bool, int32_t, EnumConstant, KeyConstant, ParameterRead, HereValue> value;
};

enum class LogicalOperator { And, Or };
struct Logical {
	LogicalOperator operation;
	std::vector<NodeHandle> children;
};

struct Not {
	NodeHandle operand;
};

enum class ArithmeticOperator { Add, Subtract, Multiply, Divide };
struct Arithmetic {
	ArithmeticOperator operation;
	std::vector<NodeHandle> children;
};

enum class ComparisonOperator { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };
struct Comparison {
	ComparisonOperator operation;
	NodeHandle left;
	NodeHandle right;
};

struct Ternary {
	NodeHandle condition;
	NodeHandle trueBranch;
	NodeHandle falseBranch;
};

struct HelperCall {
	HelperHandle definition;
	std::vector<NodeHandle> parameters;
};

struct ExternCall {
	AdapterHandle adapter;
	std::vector<NodeHandle> parameters;
};

struct Parameter {
	uint8_t position;
	const ast::Param* declaration;
	NodeHandle value;
};

struct Conversion {
	IntegralTarget target;
	NodeHandle operand;
};

struct Condition {
	NodeHandle body;
};

struct Invoke {
	NodeHandle callable;
};

struct Match {
	IntegralTarget target;
	NodeHandle discriminant;
	std::vector<NodeHandle> arms;
};

struct MatchArm {
	NodeHandle patterns;
	NodeHandle body;
	bool fallThrough;
};

struct MatchPatterns {
	std::vector<NodeHandle> patterns;
};

using Payload = std::variant<Value, Logical, Not, Arithmetic, Comparison, Ternary,
	HelperCall, ExternCall, Parameter, Conversion, Condition, Invoke,
	Match, MatchArm, MatchPatterns>;

struct Node {
	Payload payload;
	// Pattern lists and match arms are structural nodes, not source-language values.
	std::optional<ValueType> valueType;
	Provenance provenance;
};

struct HelperDefinition {
	const ast::DefineDecl* declaration;
	// Register the handle before lowering the body, including recursive references.
	std::optional<NodeHandle> body;
};

struct HostAdapter {
	const ast::ExternDefineDecl* declaration;
	ValueType result;
	std::vector<ValueType> parameters;
};

struct Root {
	ast::SectionKind kind;
	const ast::RegionDecl* owner;
	const ast::Entry* entry;
	uint32_t occurrence;
	NodeHandle expression;
	Provenance provenance;
};

// Borrowed AST/sema pointers remain valid for the lifetime of this lowering graph.
struct Graph {
	std::vector<Node> nodes;
	std::vector<HelperDefinition> helpers;
	std::vector<HostAdapter> adapters;
	std::vector<Root> roots;

	HelperHandle RegisterHelper(const ast::DefineDecl& declaration);
};

} // namespace rls::transpilers::soh_tracker::ir
