#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "output.h"
#include "parser.h"
#include "project.h"
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
        << "  -p, --project <path>   Load an rls.json manifest.\n"
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
    if (config.name != "soh" && config.name != "ap") {
        std::cerr << "error: unknown transpiler '" << config.name << "'\n";
        return false;
    }

    fs::create_directories(config.outputDir);
    DirectoryWriter writer(config.outputDir);

    if (config.name == "soh") {
        auto diagnostics = rls::transpilers::soh::SohTranspiler(project).Transpile(writer);
        bool hasErrors = false;
        for (const auto& diagnostic : diagnostics) {
            printDiagnostic(diagnostic);
            hasErrors = hasErrors || diagnostic.level == rls::ast::DiagnosticLevel::Error;
        }
        if (hasErrors) {
            std::cerr << "aborting due to SoH transpiler errors\n";
            return false;
        }
    } else {
        rls::transpilers::ap::Transpile(project, writer);
    }

    return true;
}

// == main ====================================================================

int main(int argc, char* argv[]) {
    std::vector<TranspilerConfig> transpilers;
    std::vector<fs::path> inputs;
    std::optional<fs::path> manifestPath;

    // == parse arguments =================================================
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg == "-p" || arg == "--project") {
            if (++i >= argc) {
                std::cerr << "error: " << arg << " requires a value\n";
                return 1;
            }
            if (manifestPath) {
                std::cerr << "error: " << arg << " may only be specified once\n";
                return 1;
            }
            manifestPath.emplace(argv[i]);
            continue;
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
    if (manifestPath && !inputs.empty()) {
        std::cerr << "error: --project cannot be combined with explicit input paths\n";
        return 1;
    }

    if (!manifestPath && inputs.empty()) {
        manifestPath = rls::project::FindManifest(fs::current_path());
        if (!manifestPath) {
            std::cerr << "error: no input files or folders specified, and no rls.json was found\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // == collect source files ============================================
    rls::project::SourceCollection collection;
    std::optional<rls::project::ManifestConfig> manifest;
    if (manifestPath) {
        const auto path = fs::is_directory(*manifestPath) ? *manifestPath / "rls.json" : *manifestPath;
        auto loadResult = rls::project::LoadManifest(path);
        if (!loadResult.error.empty()) {
            std::cerr << "error: " << loadResult.error << "\n";
            return 1;
        }
        manifest = std::move(loadResult.config);
        collection = rls::project::CollectManifestSources(*manifest);

        for (const auto& [name, outputDir] : manifest->transpilerOutputs) {
            const bool overridden = std::ranges::any_of(transpilers, [&name](const TranspilerConfig& config) {
                return config.name == name;
            });
            if (!overridden)
                transpilers.push_back({name, outputDir});
        }
    } else {
        collection = rls::project::CollectExplicitSources(inputs);
    }
    if (!collection.error.empty()) {
        std::cerr << "error: " << collection.error << "\n";
        return 1;
    }
    for (const auto& warning : collection.warnings) {
        std::cerr << "warning: " << warning << "\n";
    }

    const auto& sourceFiles = collection.sourceFiles;

    if (sourceFiles.empty()) {
        std::cerr << "error: no source files to process\n";
        return 1;
    }

    if (transpilers.empty()) {
        std::cerr << "error: at least one -t <name> -o <dir> pair or manifest transpiler is required\n";
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
