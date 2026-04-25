#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "ap.h"
#include "ast.h"
#include "output.h"
#include "parser.h"
#include "sema.h"
#include "soh.h"

namespace rls::acceptance_tests {

namespace fs = std::filesystem;

// Writes transpiler output files into a real directory on disk.
// Acceptance tests use this to compare emitted files against golden baselines.
class DirectoryWriter : public rls::OutputWriter {
public:
	explicit DirectoryWriter(fs::path dir) : dir_(std::move(dir)) {
		fs::create_directories(dir_);
	}

	std::ostream& open(const std::string& filename) override {
		auto fullPath = dir_ / filename;
		if (!fullPath.parent_path().empty()) {
			fs::create_directories(fullPath.parent_path());
		}

		auto stream = std::make_unique<std::ofstream>(fullPath);
		if (!*stream) {
			throw std::runtime_error("could not write " + fullPath.string());
		}

		streams_.push_back(std::move(stream));
		return *streams_.back();
	}

private:
	fs::path dir_;
	std::vector<std::unique_ptr<std::ofstream>> streams_;
};

// Temporary test output directory that is automatically cleaned up.
class TempDirectory {
public:
	explicit TempDirectory(const std::string& label) {
		auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		path_ = fs::temp_directory_path() / ("rls_acceptance_" + label + "_" + std::to_string(now));
		fs::create_directories(path_);
	}

	~TempDirectory() {
		std::error_code ec;
		fs::remove_all(path_, ec);
	}

	const fs::path& path() const {
		return path_;
	}

private:
	fs::path path_;
};

inline fs::path repoPath(const fs::path& relativePath) {
	return fs::path(RLS_REPO_ROOT) / relativePath;
}

// Normalize CRLF/LF differences so text comparisons are cross-platform.
inline std::string normalizeLineEndings(std::string value) {
	std::string normalized;
	normalized.reserve(value.size());

	for (size_t i = 0; i < value.size(); ++i) {
		if (value[i] == '\r') {
			if (i + 1 < value.size() && value[i + 1] == '\n') {
				++i;
			}
			normalized.push_back('\n');
		} else {
			normalized.push_back(value[i]);
		}
	}

	return normalized;
}

inline std::string readTextFile(const fs::path& path) {
	std::ifstream in(path);
	if (!in) {
		throw std::runtime_error("could not open " + path.string());
	}

	std::ostringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

inline std::string joinLines(const std::vector<std::string>& lines) {
	std::ostringstream out;
	for (const auto& line : lines) {
		out << line << "\n";
	}
	return out.str();
}

inline std::vector<std::string> collectParseErrors(const rls::ast::Project& project) {
	std::vector<std::string> errors;
	for (const auto& file : project.files) {
		for (const auto& diagnostic : file.diagnostics) {
			if (diagnostic.level != rls::ast::DiagnosticLevel::Error)
				continue;

			std::ostringstream msg;
			if (!diagnostic.span.file.empty()) {
				msg << diagnostic.span.file;
				if (diagnostic.span.start.line != 0) {
					msg << ":" << diagnostic.span.start.line << ":" << diagnostic.span.start.column;
				}
				msg << ": ";
			}
			msg << diagnostic.message;
			errors.push_back(msg.str());
		}
	}
	return errors;
}

inline std::vector<std::string> collectSemaErrors(const std::vector<rls::ast::Diagnostic>& diagnostics) {
	std::vector<std::string> errors;
	for (const auto& diagnostic : diagnostics) {
		if (diagnostic.level != rls::ast::DiagnosticLevel::Error)
			continue;

		std::ostringstream msg;
		if (!diagnostic.span.file.empty()) {
			msg << diagnostic.span.file;
			if (diagnostic.span.start.line != 0) {
				msg << ":" << diagnostic.span.start.line << ":" << diagnostic.span.start.column;
			}
			msg << ": ";
		}
		msg << diagnostic.message;
		errors.push_back(msg.str());
	}
	return errors;
}

// Parse all .rls files in a directory and run semantic analysis.
// Any parse/sema errors are returned as display-friendly lines.
inline rls::ast::Project parseAndAnalyzeProject(
	const fs::path& sourceDirectory,
	std::vector<std::string>& errors)
{
	auto project = rls::parser::ParseProject(sourceDirectory);

	auto parseErrors = collectParseErrors(project);
	errors.insert(errors.end(), parseErrors.begin(), parseErrors.end());

	auto semaDiagnostics = rls::sema::analyze(project);
	auto semaErrors = collectSemaErrors(semaDiagnostics);
	errors.insert(errors.end(), semaErrors.begin(), semaErrors.end());

	return project;
}

// Collect every regular file under rootDir as a normalized relative path.
// Sorting keeps missing/extra diffs deterministic.
inline std::vector<fs::path> collectRelativeFilesRecursively(const fs::path& rootDir) {
	std::vector<fs::path> files;
	for (const auto& entry : fs::recursive_directory_iterator(rootDir)) {
		if (entry.is_regular_file()) {
			auto relativePath = fs::relative(entry.path(), rootDir);
			files.push_back(relativePath.lexically_normal());
		}
	}

	std::sort(files.begin(), files.end());
	return files;
}

inline std::string joinPathLines(const std::vector<fs::path>& paths) {
	std::ostringstream out;
	for (const auto& path : paths) {
		out << " - " << path.generic_string() << "\n";
	}
	return out.str();
}

inline std::string makeRegenerateNote(const std::string& regenerateCommand) {
	if (regenerateCommand.empty()) {
		return "";
	}

	return "\nRegenerate golden examples with:\n  " + regenerateCommand;
}

// Compare two output folders recursively:
// 1) report missing/extra files between expected and actual
// 2) compare normalized text content for files present in both
inline void expectDirectoryMatchesGolden(
	const fs::path& actualDir,
	const fs::path& expectedDir,
	const std::string& regenerateCommand = "")
{
	ASSERT_TRUE(fs::exists(actualDir)) << "Missing actual output directory: " << actualDir.string();
	ASSERT_TRUE(fs::exists(expectedDir)) << "Missing golden output directory: " << expectedDir.string();

	auto actualFiles = collectRelativeFilesRecursively(actualDir);
	auto expectedFiles = collectRelativeFilesRecursively(expectedDir);

	std::vector<fs::path> missingInActual;
	std::vector<fs::path> extraInActual;

	std::set_difference(
		expectedFiles.begin(), expectedFiles.end(),
		actualFiles.begin(), actualFiles.end(),
		std::back_inserter(missingInActual));

	std::set_difference(
		actualFiles.begin(), actualFiles.end(),
		expectedFiles.begin(), expectedFiles.end(),
		std::back_inserter(extraInActual));

	EXPECT_TRUE(missingInActual.empty())
		<< "Files present in expected folder but missing in actual folder:\n"
		<< joinPathLines(missingInActual)
		<< "Expected root: " << expectedDir.string() << "\n"
		<< "Actual root: " << actualDir.string()
		<< makeRegenerateNote(regenerateCommand);

	EXPECT_TRUE(extraInActual.empty())
		<< "Files present in actual folder but missing in expected folder:\n"
		<< joinPathLines(extraInActual)
		<< "Actual root: " << actualDir.string() << "\n"
		<< "Expected root: " << expectedDir.string()
		<< makeRegenerateNote(regenerateCommand);

	for (const auto& relativePath : expectedFiles) {
		if (!std::binary_search(actualFiles.begin(), actualFiles.end(), relativePath)) {
			continue;
		}

		const auto actualPath = actualDir / relativePath;
		const auto expectedPath = expectedDir / relativePath;
		ASSERT_TRUE(fs::exists(actualPath)) << "Missing actual output: " << actualPath.string();
		ASSERT_TRUE(fs::exists(expectedPath)) << "Missing golden file: " << expectedPath.string();

		const auto actual = normalizeLineEndings(readTextFile(actualPath));
		const auto expected = normalizeLineEndings(readTextFile(expectedPath));
		EXPECT_EQ(actual, expected)
			<< "Output mismatch for " << relativePath.generic_string()
			<< " against golden " << expectedPath.string()
			<< makeRegenerateNote(regenerateCommand);
	}
}

} // namespace rls::acceptance_tests
