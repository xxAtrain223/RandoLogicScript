#include "soh_solver.h"

namespace rls::transpilers::soh_solver {

std::string Transpile(const rls::AstNode& root) {
	return "soh_solver:" + root.name;
}

} // namespace rls::transpilers::soh_solver
