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
		// Defines the host world supplies natively (e.g. has_bottle) or folds away at the
		// call site (wallet_capacity) are not emitted -- see isHostProvidedDefine.
		if (isHostProvidedDefine(name)) {
			continue;
		}
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
				// A default binds to the parameter exactly like a call argument: a Condition
				// default is thunked, a value default uses Python True/False, not True_()/
				// False_(). Reuse the call-argument path so the two stay consistent.
				sig << " = " + GenerateCallArgument(param.defaultValue.get(), project.getType(&param));
			}
		}
		sig << ") -> " << typeName(decl->body.get());

		source << sig.str() << ":\n";
		source << "    return " << GenerateExpression(decl->body) << "\n";
	}
}

} // namespace rls::transpilers::ap
