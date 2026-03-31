#include <iostream>

#include "parser.h"
#include "soh_solver.h"

int main() {
    const auto file = rls::parser::Parse("");
    const auto output = rls::transpilers::soh_solver::Transpile(file);
    std::cout << output << std::endl;
    return 0;
}
