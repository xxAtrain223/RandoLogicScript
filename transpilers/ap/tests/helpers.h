#pragma once

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <unordered_map>

#include "ast.h"
#include "parser.h"
#include "sema.h"

#include "ap_transpiler.h"

namespace rls::transpilers::ap_tests {

	/// In-memory OutputWriter for tests. Captures all output by filename.
	class MemoryWriter : public rls::OutputWriter {
	public:
		std::ostream& open(const std::string& filename) override {
			return buffers[filename];
		}

		std::string content(const std::string& filename) const {
			auto it = buffers.find(filename);
			return it != buffers.end() ? it->second.str() : "";
		}

		std::unordered_map<std::string, std::ostringstream> buffers;
	};

	// Minimal concrete ApTranspiler that supplies no game-specific behavior: every
	// scaffolding hook is stubbed and every defaulted hook is left at its base value.
	// This exercises the generic AP behavior (OptionFilter wrapping, precedence,
	// default call form) in isolation, independent of any derived world.
	class TestApTranspiler : public rls::transpilers::ap::ApTranspiler {
	public:
		explicit TestApTranspiler(const rls::ast::Project& project)
			: rls::transpilers::ap::ApTranspiler(project) {}

		void Transpile(rls::OutputWriter&) const override {}

		// Exposed for tests of the whole-file generation paths -- the top-of-rule checks run
		// there, not inside GenerateExpression.
		using ApTranspiler::GenerateFunctionDefinitionsSource;
		using ApTranspiler::GenerateRegionsSource;

	protected:
		std::string regionsPreamble(const std::set<std::string>&) const override { return ""; }
		std::string regionCreationArgs(const std::string&) const override { return ""; }
		std::string addEventsFn() const override { return ""; }
		std::string addLocationsFn() const override { return ""; }
		std::string connectRegionsFn() const override { return ""; }
		std::string eventEntryLine(const std::string&, const std::string&, const std::string&) const override {
			return "";
		}
		std::string locationEntryLine(const std::string&, const std::string&) const override { return ""; }
		std::string exitEntryLine(const std::string&, const std::string&) const override { return ""; }
		void writeEnums(rls::OutputWriter&) const override {}
		std::string functionsPreamble(const std::set<std::string>&) const override { return ""; }
		std::string pythonTypeName(rls::ast::Type, std::optional<std::string_view>) const override { return ""; }
	};

} // namespace rls::transpilers::ap_tests

inline void printDiagnostic(const rls::ast::Diagnostic& d) {
	std::ostringstream msg;
	if (!d.span.file.empty()) {
		msg << d.span.file;
		if (d.span.start.line != 0)
			msg << ":" << d.span.start.line << ":" << d.span.start.column;
		msg << ": ";
	}
	msg << levelToString(d.level) << ": " << d.message;
	ADD_FAILURE() << msg.str();
}

// Prepend the host-provided extern enums and defines the access-rule expressions rely
// on, so test sources only need to contain the expression under test. The enums must be
// declared for the parameter type annotations below to resolve.
inline std::string withHostExterns(const std::string& source) {
	return
		"extern enum Item { RG_* }\n"
		"extern enum Enemy { RE_* }\n"
		"extern enum Distance { ED_* }\n"
		"extern enum Trick { RT_* }\n"
		"extern enum Event { LOGIC_* }\n"
		"extern enum Scene { SCENE_* }\n"
		"extern enum Setting { RSK_*, RO_* }\n"
		"extern enum Region { RR_* }\n"
		"extern enum Location { RC_* }\n"
		"extern define has(item: Item) -> Bool\n"
		"extern define can_use(item: Item) -> Bool\n"
		"extern define flag(key: Event) -> Bool\n"
		"extern define setting(key: Setting) -> Int\n"
		"extern define trick(key: Trick) -> Bool\n"
		"extern define can_kill(e: Enemy) -> Bool\n"
		+ source;
}

inline rls::ast::Project resolveFromSource(const std::string& source) {
	auto file = rls::parser::ParseString(withHostExterns(source));

	for (const auto& d : file.diagnostics) {
		if (d.level == rls::ast::DiagnosticLevel::Error)
			printDiagnostic(d);
	}

	rls::ast::Project project;
	project.files.push_back(std::move(file));

	const auto& diags = rls::sema::analyze(project);

	for (const auto& d : diags) {
		if (d.level == rls::ast::DiagnosticLevel::Error)
			printDiagnostic(d);
	}

	return project;
}
