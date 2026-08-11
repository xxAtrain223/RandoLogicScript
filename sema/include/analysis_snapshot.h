#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ast.h"
#include "semantic_index.h"
#include "source_index.h"

namespace rls::sema {

struct SourceInput {
	std::string path;
	std::string content;
};

/// Immutable result of analyzing one explicit source set.
class AnalysisSnapshot {
public:
	static std::optional<std::shared_ptr<const AnalysisSnapshot>> Create(
		std::vector<SourceInput> sources, uint64_t generation = 0);

	uint64_t generation() const { return generation_; }
	const ast::Project& project() const { return project_; }
	const SemanticIndex& semanticIndex() const { return semanticIndex_; }
	const std::vector<ast::Diagnostic>& diagnostics() const { return diagnostics_; }
	const std::vector<CompilerDiagnostic>& compilerDiagnostics() const {
		return semanticIndex_.diagnostics();
	}
	const ast::SourceText* sourceText(std::string_view path) const;
	const rls::parser::SourceIndex* sourceIndex(std::string_view path) const;

private:
	struct Document {
		std::string path;
		ast::SourceText sourceText;
		rls::parser::SourceIndex sourceIndex;
	};

	uint64_t generation_ = 0;
	std::vector<Document> documents_;
	ast::Project project_;
	std::vector<ast::Diagnostic> diagnostics_;
	SemanticIndex semanticIndex_;
};

} // namespace rls::sema