#include "sema.h"
#include "collect_declarations.h"
#include "resolve_types.h"
#include "validate_declarations.h"

#include <chrono>

namespace rls::sema {

std::vector<ast::Diagnostic> analyze(
	ast::Project& project, SemanticAnalysisTimings* timings) {
	std::vector<ast::Diagnostic> diagnostics;
	if (timings) *timings = {};

	// Pass 1: Collect all top-level declarations.
	const auto declarationsStarted = std::chrono::steady_clock::now();
	auto pass1 = collectDeclarations(project);
	if (timings) {
		timings->declarationCollection =
			std::chrono::steady_clock::now() - declarationsStarted;
	}
	diagnostics.insert(diagnostics.end(),
		std::make_move_iterator(pass1.begin()),
		std::make_move_iterator(pass1.end()));

	// Pass 2: Resolve and check types for all expressions.
	const auto typesStarted = std::chrono::steady_clock::now();
	auto pass2 = resolveTypes(project);
	if (timings) timings->typeResolution = std::chrono::steady_clock::now() - typesStarted;
	diagnostics.insert(diagnostics.end(),
		std::make_move_iterator(pass2.begin()),
		std::make_move_iterator(pass2.end()));

	// Pass 3: Validate declarations (domain-specific checks).
	const auto validationStarted = std::chrono::steady_clock::now();
	auto pass3 = validateDeclarations(project);
	if (timings) timings->validation = std::chrono::steady_clock::now() - validationStarted;
	diagnostics.insert(diagnostics.end(),
		std::make_move_iterator(pass3.begin()),
		std::make_move_iterator(pass3.end()));

	return diagnostics;
}

} // namespace rls::sema
