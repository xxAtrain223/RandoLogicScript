#include "tracker_emission.h"

#include <algorithm>
#include <locale>
#include <stdexcept>
#include <type_traits>

#include "soh/enum_mappings.h"
#include "tracker_graph.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

std::string literal(std::string_view text) {
	std::string result = "\"";
	for (const unsigned char c : text) {
		if (c == '"' || c == '\\') {
			result += '\\';
			result += static_cast<char>(c);
		} else if (c < 32 || c >= 127) {
			// Fixed-width octal preserves UTF-8 bytes and cannot absorb a following digit.
			result += '\\';
			result += static_cast<char>('0' + ((c >> 6) & 7));
			result += static_cast<char>('0' + ((c >> 3) & 7));
			result += static_cast<char>('0' + (c & 7));
		} else {
			result += static_cast<char>(c);
		}
	}
	return result + "\"";
}

std::string_view targetType(IntegralTarget target) {
	switch (target) {
	case IntegralTarget::Bool: return "bool";
	case IntegralTarget::Int8: return "int8_t";
	case IntegralTarget::Int16: return "int16_t";
	case IntegralTarget::Int32: return "int32_t";
	case IntegralTarget::UInt8: return "uint8_t";
	case IntegralTarget::UInt16: return "uint16_t";
	case IntegralTarget::UInt32: return "uint32_t";
	}
	throw std::logic_error("Unvalidated tracker integral target.");
}

std::string_view operation(ArithmeticOperator op) {
	switch (op) {
	case ArithmeticOperator::Add: return "+";
	case ArithmeticOperator::Subtract: return "-";
	case ArithmeticOperator::Multiply: return "*";
	case ArithmeticOperator::Divide: return "/";
	}
	throw std::logic_error("Unvalidated tracker arithmetic operator.");
}

std::string_view operation(ComparisonOperator op) {
	switch (op) {
	case ComparisonOperator::Equal: return "==";
	case ComparisonOperator::NotEqual: return "!=";
	case ComparisonOperator::Less: return "<";
	case ComparisonOperator::LessEqual: return "<=";
	case ComparisonOperator::Greater: return ">";
	case ComparisonOperator::GreaterEqual: return ">=";
	}
	throw std::logic_error("Unvalidated tracker comparison operator.");
}

std::string_view rootKind(ast::SectionKind kind) {
	switch (kind) {
	case ast::SectionKind::Locations: return "Location";
	case ast::SectionKind::Events: return "Event";
	case ast::SectionKind::Exits: return "Exit";
	}
	throw std::logic_error("Unvalidated tracker root kind.");
}

std::string qualified(std::string_view enumName, std::string_view member)
{
	if (const auto* mapping = soh::findHostEnumMapping(enumName)) {
		return mapping->cppValueNamespace.empty() ? std::string(member) :
			std::string(mapping->cppValueNamespace) + "::" + std::string(member);
	}
	throw std::logic_error("Unvalidated tracker host key namespace.");
}

std::string_view keyType(ast::Type type) {
	switch (type) {
	case ast::Type::Region: return "Region";
	case ast::Type::Event: return "Event";
	case ast::Type::Location: return "Location";
	default: throw std::logic_error("Unvalidated tracker key type.");
	}
}

class Renderer {
public:
	Renderer(const Graph& graph, const Numbering& numbering,
		std::span<const HostEnumBinding> customEnums)
		: graph(graph), numbering(numbering), customEnums(customEnums) {}

	std::string id(NodeHandle handle) const {
		return std::to_string(numbering.idsBySlot.at(handle.slot).value().value);
	}

	std::string node(const Node& descriptor) const {
		return std::visit([&](const auto& payload) -> std::string {
			using T = std::decay_t<decltype(payload)>;
			if constexpr (std::is_same_v<T, Value>) {
				std::string text;
				const auto evaluator = std::visit([&](const auto& value) -> std::string {
					using V = std::decay_t<decltype(value)>;
					if constexpr (std::is_same_v<V, bool>) {
						text = value ? "true" : "false";
						return text;
					} else if constexpr (std::is_same_v<V, int32_t>) {
						text = std::to_string(value);
						return "int32_t{" + text + "}";
					} else if constexpr (std::is_same_v<V, ParameterRead>) {
						text = value.declaration->name.text;
						return "scope.get(" + literal(text) + ")";
					} else {
						std::string symbol;
						if constexpr (std::is_same_v<V, EnumConstant>) {
							text = value.member->text;
							symbol = soh::findHostEnumMapping(value.declaration->name.text) ?
								qualified(value.declaration->name.text, text) :
								GetHostEnumType(*value.declaration, customEnums).value() + "::" + text;
						} else if constexpr (std::is_same_v<V, KeyConstant>) {
							text = value.name->text;
							symbol = qualified(keyType(descriptor.valueType->sourceType), text);
						} else if constexpr (std::is_same_v<V, HereValue>) {
							text = "here";
							symbol = qualified("Region", value.region->key.text);
						}
						return "CheckedHostInt(" + symbol + ")";
					}
				}, payload.value);
				const auto scope = std::holds_alternative<ParameterRead>(payload.value) ?
					"VariableScope& scope" : "VariableScope&";
				return construct("ValueExpression", "[](" + std::string(scope) + ") -> value_t { return " +
					evaluator + "; }, " + literal(text) + ", false");
			} else if constexpr (std::is_same_v<T, Logical>) {
				return construct(payload.operation == LogicalOperator::And ? "AndExpression" :
					"OrExpression", ids(payload.children));
			} else if constexpr (std::is_same_v<T, Arithmetic>) {
				return construct("ArithmeticExpression", literal(operation(payload.operation)) + ", " +
					ids(payload.children));
			} else if constexpr (std::is_same_v<T, Not>) {
				return construct("NotExpression", id(payload.operand));
			} else if constexpr (std::is_same_v<T, Comparison>) {
				return construct("ComparisonExpression", literal(operation(payload.operation)) + ", " +
					id(payload.left) + ", " + id(payload.right));
			} else if constexpr (std::is_same_v<T, Ternary>) {
				return construct("TernaryExpression", id(payload.condition) + ", " +
					id(payload.trueBranch) + ", " + id(payload.falseBranch));
			} else if constexpr (std::is_same_v<T, HelperCall>) {
				const auto& definition = graph.helpers[payload.definition.slot];
				return construct("FunctionCallExpression", literal(definition.declaration->name.text) +
					", " + ids(payload.parameters) + ", " + id(*definition.body));
			} else if constexpr (std::is_same_v<T, ExternCall>) {
				const auto& declaration = *graph.adapters[payload.adapter.slot].declaration;
				return construct("ExternFunctionCallExpression", declaration.name.text + "_adapter, " +
					literal(declaration.name.text) + ", " + ids(payload.parameters));
			} else if constexpr (std::is_same_v<T, Parameter>) {
				return construct("ParameterExpression", std::to_string(payload.position) +
					", " + literal(payload.declaration->name.text) + ", " + id(payload.value));
			} else if constexpr (std::is_same_v<T, Conversion>) {
				return construct("ConversionExpression<" + std::string(targetType(payload.target)) + ">",
					id(payload.operand));
			} else if constexpr (std::is_same_v<T, Condition>) {
				return construct("ConditionExpression", id(payload.body));
			} else if constexpr (std::is_same_v<T, Invoke>) {
				return construct("InvokeExpression", id(payload.callable));
			} else if constexpr (std::is_same_v<T, Match>) {
				return construct("MatchExpression<" + std::string(targetType(payload.target)) + ">",
					id(payload.discriminant) + ", " + ids(payload.arms));
			} else if constexpr (std::is_same_v<T, MatchArm>) {
				return construct("MatchArm", id(payload.patterns) + ", " + id(payload.body) +
					(payload.fallThrough ? ", true" : ", false"));
			} else if constexpr (std::is_same_v<T, MatchPatterns>) {
				return construct("MatchArmPatterns", ids(payload.patterns));
			} else {
				static_assert(std::is_same_v<T, void>, "Unhandled tracker payload");
			}
		}, descriptor.payload);
	}

private:
	const Graph& graph;
	const Numbering& numbering;
	std::span<const HostEnumBinding> customEnums;

	static std::string construct(std::string_view type, const std::string& arguments) {
		return "std::make_unique<" + std::string(type) + ">(" + arguments + ")";
	}

	std::string ids(const std::vector<NodeHandle>& children) const {
		std::string result = "std::vector<index_t>{";
		for (size_t i = 0; i < children.size(); ++i) {
			if (i) result += ", ";
			result += id(children[i]);
		}
		return result + "}";
	}
};

void GenerateHeader(rls::OutputWriter& out) {
	auto& header = out.open("logic_tracker.gen.h");
	header << R"(#pragma once

#include "logic_expression.h"

uint64_t GetExpressionCount();
LogicExpression* GetExpression(index_t index);
const LogicRootMap& GetLogicRoots();
)";
}

void GenerateSource(rls::OutputWriter& out, const Graph& graph, const Numbering& numbering,
	std::span<const HostEnumBinding> customEnums)
{
	auto& source = out.open("logic_tracker.gen.cpp");
	source.imbue(std::locale::classic());
	source << R"(#include "logic_tracker.gen.h"
#include "rls_host.h"
#include "functions.gen.h"

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T>
static int32_t CheckedHostInt(T value) {
	if constexpr (std::is_enum_v<T>) {
		return CheckedHostInt(static_cast<std::underlying_type_t<T>>(value));
	} else if constexpr (std::is_same_v<T, bool>) {
		return value ? int32_t{1} : int32_t{0};
	} else {
		static_assert(std::is_integral_v<T>);
		using Wide = std::conditional_t<std::is_signed_v<T>, intmax_t, uintmax_t>;
		if (!std::in_range<int32_t>(static_cast<Wide>(value))) {
			throw std::out_of_range("Tracker host value is outside int32_t range.");
		}
		return static_cast<int32_t>(value);
	}
}

)";
	std::vector<const HostAdapter*> adapters;
	for (const auto& adapter : graph.adapters) adapters.push_back(&adapter);
	std::sort(adapters.begin(), adapters.end(), [](const HostAdapter* a, const HostAdapter* b) {
		return a->declaration->name.text + "_adapter" < b->declaration->name.text + "_adapter";
	});
	for (const auto* adapter : adapters) {
		GenerateHostAdapter(source, *adapter, customEnums);
		source << "\n";
	}
	source << R"(static std::unique_ptr<LogicExpression> CreateExpression(index_t index) {
	switch (index) {
)";
	const Renderer renderer(graph, numbering, customEnums);
	for (size_t i = 0; i < numbering.nodesById.size(); ++i) {
		const auto& node = graph.nodes[numbering.nodesById[i].slot];
		source << "\tcase " << i << ": return " << renderer.node(node) << ";\n";
	}
	source << R"(	default: throw std::out_of_range("Invalid tracker expression index.");
	}
}

uint64_t GetExpressionCount() {
	return )" << numbering.count.value << R"(;
}

LogicExpression* GetExpression(index_t index) {
	static LogicExpressionCache cache(GetExpressionCount(), CreateExpression);
	return cache.GetExpression(index);
}

const LogicRootMap& GetLogicRoots() {
	static const LogicRootMap roots{
)";
	for (const auto slot : numbering.rootsInOrder) {
		const auto& root = graph.roots[slot];
		source << "\t\t{LogicRootKey{LogicRootKind::" << rootKind(root.kind) << ", "
			<< literal(root.owner->key.text) << ", " << literal(root.entry->name.text) << ", "
			<< root.occurrence << "}, " << renderer.id(root.expression) << "},\n";
	}
	source << R"(	};
	return roots;
}
)";
}

} // namespace

std::vector<ast::Diagnostic> EmitArtifacts(const ast::Project& project, const Graph& graph,
	rls::OutputWriter& out, const std::filesystem::path& sourceRoot,
	std::span<const HostEnumBinding> customEnums)
{
	auto diagnostics = ValidateGraph(project, graph, sourceRoot, customEnums);
	if (!diagnostics.empty()) return diagnostics;
	auto numbered = NumberDistinctGraph(graph, sourceRoot);
	if (const auto* errors = std::get_if<std::vector<ast::Diagnostic>>(&numbered)) return *errors;
	const auto& numbering = std::get<Numbering>(numbered);
	for (size_t slot = 0; slot < graph.nodes.size(); ++slot) {
		if (!numbering.idsBySlot[slot]) continue;
		auto path = NormalizeSourcePath(graph.nodes[slot].provenance.span, sourceRoot);
		if (const auto* error = std::get_if<ast::Diagnostic>(&path)) {
			diagnostics.push_back(*error);
		}
	}
	if (!diagnostics.empty()) return diagnostics;

	GenerateHeader(out);
	GenerateSource(out, graph, numbering, customEnums);
	return diagnostics;
}

} // namespace rls::transpilers::soh_tracker::ir
