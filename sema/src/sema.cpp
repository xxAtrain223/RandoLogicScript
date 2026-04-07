#include "sema.h"
#include "collect_declarations.h"

namespace rls::sema {

std::vector<ast::Diagnostic> analyze(ast::Project& project) {
	std::vector<ast::Diagnostic> diagnostics;

	// Pass 1: Collect all top-level declarations.
	auto pass1 = collectDeclarations(project);
	diagnostics.insert(diagnostics.end(),
		std::make_move_iterator(pass1.begin()),
		std::make_move_iterator(pass1.end()));

	return diagnostics;
}

} // namespace rls::sema
