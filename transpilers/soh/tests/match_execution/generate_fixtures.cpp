#include "parser.h"
#include "sema.h"
#include "soh.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

bool reportDiagnostics(const std::vector<rls::ast::Diagnostic>& diagnostics) {
    bool hasErrors = false;
    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.span.file << ":" << diagnostic.span.start.line
                  << ": " << diagnostic.message << "\n";
        hasErrors |= diagnostic.level == rls::ast::DiagnosticLevel::Error;
    }
    return hasErrors;
}

std::string parameterType(const rls::ast::Project& project, const rls::ast::Param& param) {
    switch (project.getType(&param).value_or(rls::ast::Type::Error)) {
    case rls::ast::Type::Bool: return "bool";
    case rls::ast::Type::Int: return "int";
    case rls::ast::Type::Enum:
        if (const auto name = project.getEnumType(&param)) {
            return std::string(*name);
        }
        break;
    default: break;
    }
    throw std::runtime_error("Unsupported fixture parameter type: " + param.name.text);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: soh_match_fixture_generator input.rls output.h\n";
        return 1;
    }

    rls::ast::Project project;
    project.files.push_back(rls::parser::ParseFile(argv[1]));
    if (reportDiagnostics(project.files.front().diagnostics)) return 1;
    if (reportDiagnostics(rls::sema::analyze(project))) return 1;

    std::ofstream output(argv[2]);
    if (!output) {
        std::cerr << "Cannot open fixture output: " << argv[2] << "\n";
        return 1;
    }

    const rls::transpilers::soh::SohTranspiler transpiler(project);
    output << "#pragma once\nnamespace generated {\n";
    for (const auto& [name, declaration] : project.DefineDecls) {
        output << "auto " << name << "(";
        for (size_t index = 0; index < declaration->params.size(); ++index) {
            if (index != 0) output << ", ";
            const auto& param = declaration->params[index];
            output << parameterType(project, param) << " " << param.name.text;
        }
        output << ") {\n    return " << transpiler.GenerateExpression(declaration->body)
               << ";\n}\n";
    }
    output << "}\n";
    output.close();
    if (!output) {
        std::cerr << "Cannot write fixture output: " << argv[2] << "\n";
        return 1;
    }
}
