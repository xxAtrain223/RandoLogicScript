#include "ap_transpiler.h"

#include <sstream>

namespace rls::transpilers::ap {

void ApTranspiler::GenerateFunctionDefinitionsSource(rls::OutputWriter& out) const {
	auto& source = out.open("functions.gen.py");
	source << functionsPreamble();

	// Resolve an AST node's RLS type to its Python type name, deferring the
	// concrete mapping to the game hook.
	auto typeName = [&](const auto* node) -> std::string {
		auto type = project.getType(node);
		if (!type.has_value()) {
			return "missing_type";
		}
		return pythonTypeName(type.value());
	};

	for (const auto& [name, decl] : project.DefineDecls) {
		source << "\n";

		std::ostringstream sig;
		sig << "def " << decl->name << "(";
		const std::string receiver = ruleContextParam();
		bool needComma = false;
		if (!receiver.empty()) {
			sig << receiver;
			needComma = true;
		}
		for (size_t i = 0; i < decl->params.size(); i++) {
			const auto& param = decl->params[i];
			if (needComma) {
				sig << ", ";
			}
			needComma = true;
			sig << param.name << ": " << typeName(&param);
			if (param.defaultValue != nullptr) {
				sig << " = " + GenerateExpression(param.defaultValue);
			}
		}
		sig << ") -> " << typeName(decl->body.get());

		source << sig.str() << ":\n";
		source << "    return " << GenerateExpression(decl->body) << "\n";
	}
}

} // namespace rls::transpilers::ap
