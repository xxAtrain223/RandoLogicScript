#pragma once

#include <string>
#include <vector>

#include "ast.h"

namespace rls::ast {

// == Diagnostics ==============================================================

enum class DiagnosticLevel { Error, Warning };

/// A diagnostic message produced during parsing or semantic analysis.
struct Diagnostic {
	DiagnosticLevel level;
	std::string message;
	Span span; // location of the offending construct
};

} // namespace rls::ast
