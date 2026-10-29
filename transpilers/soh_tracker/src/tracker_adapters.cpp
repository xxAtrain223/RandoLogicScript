#include "tracker_adapters.h"

#include <algorithm>
#include <sstream>

#include "soh/enum_mappings.h"

namespace rls::transpilers::soh_tracker::ir {
namespace {

std::optional<std::string> hostType(const ValueType& type,
	std::span<const HostEnumBinding> customEnums, bool result)
{
	switch (type.sourceType) {
	case ast::Type::Bool: return "bool";
	case ast::Type::Int: return "int32_t";
	case ast::Type::Condition: return result ? "condition_t" : "std::function<bool()>";
	case ast::Type::Region: return "RandomizerRegion";
	case ast::Type::Event: return "LogicVal";
	case ast::Type::Location: return "RandomizerCheck";
	case ast::Type::Enum:
		if (!type.enumInfo) return std::nullopt;
		if (const auto* mapping = soh::findHostEnumMapping(type.enumInfo->name.text)) {
			return std::string(mapping->cppType);
		}
		if (type.enumInfo->kind == ast::EnumKind::Normal) return type.enumInfo->name.text;
		for (const auto& mapping : customEnums) {
			if (mapping.rlsName == type.enumInfo->name.text) return mapping.cppType;
		}
		return std::nullopt;
	default: return std::nullopt;
	}
}

bool validRepresentation(const ValueType& type) {
	if (type.sourceType == ast::Type::Condition) {
		return std::holds_alternative<ConditionTarget>(type.representation);
	}
	const auto* target = std::get_if<IntegralTarget>(&type.representation);
	return target && *target == (type.sourceType == ast::Type::Bool ?
		IntegralTarget::Bool : IntegralTarget::Int32);
}

std::string argument(const ValueType& type, std::string_view cppType, size_t position) {
	const auto reference = "arguments[" + std::to_string(position) + "]";
	switch (type.sourceType) {
	case ast::Type::Bool: return "ToBool(" + reference + ".Result)";
	case ast::Type::Condition: return "MakeConditionCallback(" + reference + ")";
	case ast::Type::Int: return "ToInt<int32_t>(" + reference + ".Result)";
	default:
		return "static_cast<" + std::string(cppType) +
			">(ToInt<int32_t>(" + reference + ".Result))";
	}
}

} // namespace

std::vector<ast::Diagnostic> ValidateHostAdapters(const Graph& graph,
	std::span<const HostEnumBinding> customEnums)
{
	std::vector<ast::Diagnostic> diagnostics;
	const auto error = [&](const ast::Span& span, std::string message) {
		diagnostics.push_back({"", span, ast::DiagnosticLevel::Error, std::move(message)});
	};
	for (size_t i = 0; i < customEnums.size(); ++i) {
		const auto& mapping = customEnums[i];
		if (mapping.rlsName.empty() || mapping.cppType.empty() ||
			soh::findHostEnumMapping(mapping.rlsName) ||
			std::any_of(customEnums.begin(), customEnums.begin() + i,
				[&](const HostEnumBinding& previous) { return previous.rlsName == mapping.rlsName; })) {
			error({}, "Tracker host enum mapping is empty, duplicated, or overrides a baseline mapping: '" +
				mapping.rlsName + "'.");
		}
	}

	for (const auto& adapter : graph.adapters) {
		if (!adapter.declaration) {
			error({}, "Tracker host adapter is missing its extern declaration.");
			continue;
		}
		const auto& declaration = *adapter.declaration;
		const auto fail = [&](std::string message) {
			error(declaration.span, "Tracker host binding '" + declaration.name.text + "': " + message);
		};
		if (adapter.parameters.size() != declaration.params.size()) {
			fail("adapter parameter count does not match the extern declaration.");
			continue;
		}
		for (size_t i = 0; i <= adapter.parameters.size(); ++i) {
			const bool result = i == adapter.parameters.size();
			const auto& type = result ? adapter.result : adapter.parameters[i];
			const auto expected = hostType(type, customEnums, result);
			const auto label = result ? std::string("return") : "parameter " + std::to_string(i);
			if (!expected || expected->empty()) {
				fail("missing supported host type mapping for " + label + ".");
				continue;
			}
			if (!validRepresentation(type)) fail("incompatible " + label + " representation.");
		}
	}
	return diagnostics;
}

AdapterRenderingResult RenderHostAdapters(const Graph& graph,
	std::span<const HostEnumBinding> customEnums)
{
	auto diagnostics = ValidateHostAdapters(graph, customEnums);
	if (!diagnostics.empty()) return diagnostics;

	std::vector<RenderedAdapter> rendered;
	for (size_t slot = 0; slot < graph.adapters.size(); ++slot) {
		const auto& adapter = graph.adapters[slot];
		const auto& declaration = *adapter.declaration;
		const auto symbol = declaration.name.text + "_adapter";
		std::ostringstream out;
		out << "static value_t " << symbol << "(std::span<EvalResult> arguments) {\n";
		out << "\treturn ";
		const bool condition = adapter.result.sourceType == ast::Type::Condition;
		if (condition) out << "condition_t{";
		else out << "static_cast<" << (adapter.result.sourceType == ast::Type::Bool ? "bool" : "int32_t") << ">(";
		out << "::" << declaration.name.text << "(";
		for (size_t i = 0; i < adapter.parameters.size(); ++i) {
			if (i) out << ", ";
			out << argument(adapter.parameters[i], *hostType(adapter.parameters[i], customEnums, false), i);
		}
		out << ")";
		out << (condition ? "}" : ")");
		out << ";\n}\n";
		rendered.push_back({AdapterHandle{slot}, symbol, out.str()});
	}
	return rendered;
}

} // namespace rls::transpilers::soh_tracker::ir
