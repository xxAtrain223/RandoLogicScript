#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "output.h"
#include "parser.h"
#include "sema.h"
#include "ap.h"
#include "soh.h"

namespace fs = std::filesystem;

// == helpers =================================================================

static void printUsage(const char* program) {
    std::cerr
        << "Usage: " << program
        << " [options] <files/folders...>\n"
        << "\n"
        << "Options:\n"
        << "  -t, --transpiler <name> -o, --output <dir>\n"
        << "                            Transpiler and output directory pair (may be repeated).\n"
        << "                            Available transpilers: soh, ap\n"
        << "  -h, --help                Show this help message.\n";
}

static void printDiagnostic(const rls::ast::Diagnostic& d) {
    if (!d.span.file.empty()) {
        std::cerr << d.span.file;
        if (d.span.start.line != 0)
            std::cerr << ":" << d.span.start.line << ":" << d.span.start.column;
        std::cerr << ": ";
    }
    std::cerr << levelToString(d.level) << ": " << d.message << "\n";
}

/// Recursively collect all `.rls` files under `dir`.
static std::vector<fs::path> collectFiles(const fs::path& dir) {
    std::vector<fs::path> result;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".rls")
            result.push_back(entry.path());
    }
    return result;
}

// == transpiler dispatch =====================================================

/// OutputWriter that creates files in a directory on disk.
class DirectoryWriter : public rls::OutputWriter {
public:
    explicit DirectoryWriter(fs::path dir) : dir_(std::move(dir)) {}

    std::ostream& open(const std::string& filename) override {
        auto stream = std::make_unique<std::ofstream>(dir_ / filename);
        if (!*stream)
            throw std::runtime_error("could not write " + (dir_ / filename).string());
        streams_.push_back(std::move(stream));
        return *streams_.back();
    }

private:
    fs::path dir_;
    std::vector<std::unique_ptr<std::ofstream>> streams_;
};

struct TranspilerConfig {
    std::string name;
    fs::path outputDir;
};

static bool runTranspiler(const TranspilerConfig& config, const rls::ast::Project& project) {
    fs::create_directories(config.outputDir);
    DirectoryWriter writer(config.outputDir);

    if (config.name == "soh") {
        rls::transpilers::soh::Transpile(project, writer);
    } else if (config.name == "ap") {
        rls::transpilers::ap::Transpile(project, writer);
    } else {
        std::cerr << "error: unknown transpiler '" << config.name << "'\n";
        return false;
    }

    return true;
}

// == main ====================================================================

int main(int argc, char* argv[]) {
    std::vector<TranspilerConfig> transpilers;
    std::vector<fs::path> inputs;

    // == parse arguments =================================================
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-t" || arg == "--transpiler") {
            if (++i >= argc) {
                std::cerr << "error: " << arg << " requires a value\n";
                return 1;
            }
            std::string name = argv[i];

            // Expect -o/--output immediately after
            if (i + 1 >= argc) {
                std::cerr << "error: -t " << name << " must be followed by -o <dir>\n";
                return 1;
            }
            std::string nextArg = argv[++i];
            if (nextArg != "-o" && nextArg != "--output") {
                std::cerr << "error: -t " << name << " must be followed by -o <dir>\n";
                return 1;
            }
            if (++i >= argc) {
                std::cerr << "error: " << nextArg << " requires a value\n";
                return 1;
            }

            transpilers.push_back({std::move(name), argv[i]});
            continue;
        }
        if (arg.starts_with("-")) {
            std::cerr << "error: unknown option '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        }

        inputs.emplace_back(arg);
    }

    // == validate arguments ==============================================
    if (inputs.empty()) {
        std::cerr << "error: no input files or folders specified\n";
        printUsage(argv[0]);
        return 1;
    }

    if (transpilers.empty()) {
        std::cerr << "error: at least one -t <name> -o <dir> pair must be specified\n";
        return 1;
    }

    // == collect source files ============================================
    std::vector<fs::path> sourceFiles;
    for (const auto& input : inputs) {
        if (!fs::exists(input)) {
            std::cerr << "error: path does not exist: " << input << "\n";
            return 1;
        }
        if (fs::is_directory(input)) {
            auto files = collectFiles(input);
            if (files.empty())
                std::cerr << "warning: no .rls files found in " << input << "\n";
            sourceFiles.insert(sourceFiles.end(), files.begin(), files.end());
        } else {
            sourceFiles.push_back(input);
        }
    }

    if (sourceFiles.empty()) {
        std::cerr << "error: no source files to process\n";
        return 1;
    }

    // == parse ===========================================================
    rls::ast::Project project;
    bool hasParseErrors = false;

    for (const auto& path : sourceFiles) {
        auto file = rls::parser::ParseFile(path);

        for (const auto& d : file.diagnostics) {
            printDiagnostic(d);
            if (d.level == rls::ast::DiagnosticLevel::Error)
                hasParseErrors = true;
        }

        project.files.push_back(std::move(file));
    }

    if (hasParseErrors) {
        std::cerr << "aborting due to parse errors\n";
        return 1;
    }

    // == semantic analysis ===============================================
    auto diagnostics = rls::sema::analyze(project);
    bool hasErrors = false;

    for (const auto& d : diagnostics) {
        printDiagnostic(d);
        if (d.level == rls::ast::DiagnosticLevel::Error)
            hasErrors = true;
    }

    if (hasErrors) {
        std::cerr << "aborting due to semantic errors\n";
        return 1;
    }

    // == transpile & write output ========================================
    for (const auto& config : transpilers) {
        if (!runTranspiler(config, project))
            return 1;
    }

    return 0;
}
