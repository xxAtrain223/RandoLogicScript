#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
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
		std::vector<SourceInput> sources, uint64_t generation = 0,
		std::stop_token cancellation = {});

	uint64_t generation() const { return generation_; }
	size_t documentCount() const { return documents_.size(); }
	const SemanticIndex& semanticIndex() const { return semanticIndex_; }
	const ast::SourceText* sourceText(std::string_view path) const;
	const rls::parser::SourceIndex* sourceIndex(std::string_view path) const;
	std::optional<rls::parser::SyntaxContext> syntaxAt(std::string_view path, ast::Position position) const;
	std::optional<rls::parser::SourceNameContext> nameAt(std::string_view path, ast::Position position) const;
	std::optional<SymbolId> symbolAt(std::string_view path, ast::Position position) const;
	std::optional<OccurrenceRecord> occurrenceAt(std::string_view path, ast::Position position) const;
	std::optional<TypeRecord> typeAt(std::string_view path, ast::Position position) const;
	std::optional<ExpectedTypeRecord> expectedTypeAt(std::string_view path, ast::Position position) const;
	std::optional<CallRecord> callAt(std::string_view path, ast::Position position) const;
	std::optional<SymbolRecord> declaration(SymbolId symbol) const;
	std::vector<OccurrenceRecord> references(SymbolId symbol) const;
	std::vector<SymbolId> visibleSymbolsAt(std::string_view path, ast::Position position) const;
	std::vector<CompilerDiagnostic> diagnosticsFor(std::string_view path) const;

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
	std::vector<CompilerDiagnostic> compilerDiagnostics_;
};

} // namespace rls::sema