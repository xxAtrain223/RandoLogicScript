#pragma once

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <unordered_map>

#include "ast.h"
#include "parser.h"
#include "sema.h"

#include "soh_ap.h"

namespace rls::transpilers::soh_ap_tests {

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

} // namespace rls::transpilers::soh_ap_tests

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
		// A normal (non-extern) enum, mirroring the stdlib: its values map into Events.
		"enum WaterLevel { WL_LOW, WL_MID, WL_HIGH, WL_LOW_OR_MID, WL_HIGH_OR_MID }\n"
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

// Resolve several named sources as one project. Needed wherever the generated output depends
// on *which file* declared something -- REGION_QUEST reads the declaring file's name, which a
// single in-memory source cannot express.
struct SourceFile {
	std::string name;
	std::string source;
};

inline rls::ast::Project resolveRawFiles(const std::vector<SourceFile>& files) {
	rls::ast::Project project;
	for (const auto& f : files) {
		auto file = rls::parser::ParseString(f.source, f.name);
		for (const auto& d : file.diagnostics) {
			if (d.level == rls::ast::DiagnosticLevel::Error)
				printDiagnostic(d);
		}
		project.files.push_back(std::move(file));
	}

	const auto& diags = rls::sema::analyze(project);

	for (const auto& d : diags) {
		if (d.level == rls::ast::DiagnosticLevel::Error)
			printDiagnostic(d);
	}

	return project;
}

// An analyzed project paired with one expression borrowed out of it. The project must
// outlive the expression, so the two travel together.
struct ResolvedExpression {
	rls::ast::Project project;
	rls::ast::ExprPtr expr;
};

// Resolve a define from inline RLS source and hand back its body expression.
inline ResolvedExpression sourceToExpression(const std::string& source, const std::string& defineName) {
	auto project = resolveFromSource(source);
	auto defineDecl = project.DefineDecls.find(defineName);
	if (defineDecl == project.DefineDecls.end()) {
		return { std::move(project), nullptr };
	}

	return {
		std::move(project),
		std::move(const_cast<rls::ast::DefineDecl*>(defineDecl->second)->body)
	};
}

inline std::string GenerateExpression(const ResolvedExpression& resolved) {
	return rls::transpilers::soh_ap::SohApTranspiler(resolved.project).GenerateExpression(resolved.expr);
}
