#include "sema.h"
#include "collect_declarations.h"
#include "resolve_types.h"
#include "validate_declarations.h"

namespace rls::sema {

std::vector<ast::Diagnostic> analyze(ast::Project& project) {
	std::vector<ast::Diagnostic> diagnostics;

	// Pass 1: Collect all top-level declarations.
	auto pass1 = collectDeclarations(project);
	diagnostics.insert(diagnostics.end(),
		std::make_move_iterator(pass1.begin()),
		std::make_move_iterator(pass1.end()));

	// Pass 2: Resolve and check types for all expressions.
	auto pass2 = resolveTypes(project);
	diagnostics.insert(diagnostics.end(),
		std::make_move_iterator(pass2.begin()),
		std::make_move_iterator(pass2.end()));

	// Pass 3: Validate declarations (domain-specific checks).
	auto pass3 = validateDeclarations(project);
	diagnostics.insert(diagnostics.end(),
		std::make_move_iterator(pass3.begin()),
		std::make_move_iterator(pass3.end()));

	return diagnostics;
}

} // namespace rls::sema
