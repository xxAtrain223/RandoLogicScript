#include <iostream>

#include "parser.h"
#include "soh_solver.h"

int main() {
    const auto ast = rls::parser::Parse("demo_rule");
    const auto output = rls::transpilers::soh_solver::Transpile(ast);
    std::cout << output << std::endl;
    return 0;
}
