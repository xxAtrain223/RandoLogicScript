#pragma once

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <unordered_map>

#include "ast.h"
#include "parser.h"
#include "sema.h"

#include "soh.h"

namespace rls::transpilers::soh_tests {

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

} // namespace rls::transpilers::soh_tests

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

inline std::string withHostExterns(const std::string& source) {
	return
		"extern define has(item: Item) -> Bool\n"
		"extern define can_use(item: Item) -> Bool\n"
		"extern define keys(sc: Scene, amount: Int) -> Bool\n"
		"extern define flag(key: Logic) -> Bool\n"
		"extern define setting(key: Setting) -> Setting\n"
		"extern define trick(key: Trick) -> Bool\n"
		"extern define hearts() -> Int\n"
		"extern define effective_health() -> Int\n"
		"extern define trial_skipped(key: Trial) -> Bool\n"
		"extern define check_price(chk: Check = RC_UNKNOWN_CHECK) -> Int\n"
		"extern define can_plant_bean(reg: Region, bean: Item) -> Bool\n"
		"extern define triforce_pieces() -> Int\n"
		"extern define big_poes() -> Int\n"
		+ source;
}

inline rls::ast::Project resolveFromSource(
	const std::string& source)
{
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