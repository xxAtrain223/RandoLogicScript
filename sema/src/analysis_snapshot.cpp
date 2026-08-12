#include "analysis_snapshot.h"

#include "parser.h"
#include "sema.h"

#include <algorithm>
#include <map>

namespace rls::sema {

std::optional<std::shared_ptr<const AnalysisSnapshot>> AnalysisSnapshot::Create(
	std::vector<SourceInput> sources, uint64_t generation, std::stop_token cancellation) {
	if (cancellation.stop_requested()) return std::nullopt;
	auto snapshot = std::make_shared<AnalysisSnapshot>();
	snapshot->generation_ = generation;
	std::map<std::string, std::string> effectiveSources;
	for (auto& source : sources) effectiveSources[std::move(source.path)] = std::move(source.content);

	for (auto& [path, content] : effectiveSources) {
		if (cancellation.stop_requested()) return std::nullopt;
		const auto sourceText = ast::SourceText::FromUtf8(content);
		if (!sourceText) return std::nullopt;
		auto parsed = rls::parser::ParseStringWithIndex(content, path);
		if (cancellation.stop_requested()) return std::nullopt;
		snapshot->documents_.push_back({path, *sourceText, std::move(parsed.sourceIndex)});
		snapshot->project_.files.push_back(std::move(parsed.file));
	}

	if (cancellation.stop_requested()) return std::nullopt;
	snapshot->diagnostics_ = analyze(snapshot->project_);
	if (cancellation.stop_requested()) return std::nullopt;
	for (const auto& file : snapshot->project_.files) {
		for (const auto& diagnostic : file.diagnostics) {
			snapshot->diagnostics_.push_back(diagnostic);
		}
	}
	if (cancellation.stop_requested()) return std::nullopt;
	snapshot->semanticIndex_ = buildSemanticIndex(snapshot->project_, snapshot->diagnostics_);
	if (cancellation.stop_requested()) return std::nullopt;
	for (const auto& diagnostic : snapshot->diagnostics_) {
		if (diagnostic.code.starts_with("RLS-V")) continue;
		snapshot->compilerDiagnostics_.push_back({diagnostic.code, diagnostic.level,
			diagnostic.message, diagnostic.span, {}});
	}
	for (const auto& diagnostic : snapshot->semanticIndex_.diagnostics()) {
		snapshot->compilerDiagnostics_.push_back(diagnostic);
	}
	return std::shared_ptr<const AnalysisSnapshot>(std::move(snapshot));
}

std::vector<std::string> AnalysisSnapshot::documentPaths() const {
	std::vector<std::string> paths;
	paths.reserve(documents_.size());
	for (const auto& document : documents_) paths.push_back(document.path);
	return paths;
}

const ast::SourceText* AnalysisSnapshot::sourceText(std::string_view path) const {
	const auto it = std::find_if(documents_.begin(), documents_.end(), [&](const Document& document) {
		return document.path == path;
	});
	return it == documents_.end() ? nullptr : &it->sourceText;
}

const rls::parser::SourceIndex* AnalysisSnapshot::sourceIndex(std::string_view path) const {
	const auto it = std::find_if(documents_.begin(), documents_.end(), [&](const Document& document) {
		return document.path == path;
	});
	return it == documents_.end() ? nullptr : &it->sourceIndex;
}

std::optional<rls::parser::SyntaxContext> AnalysisSnapshot::syntaxAt(std::string_view path, ast::Position position) const {
	const auto* index = sourceIndex(path);
	return index ? index->syntaxAt(position) : std::nullopt;
}

std::optional<rls::parser::SourceNameContext> AnalysisSnapshot::nameAt(std::string_view path, ast::Position position) const {
	const auto* index = sourceIndex(path);
	return index ? index->nameAt(position) : std::nullopt;
}

std::optional<OccurrenceRecord> AnalysisSnapshot::occurrenceAt(std::string_view path, ast::Position position) const {
	return semanticIndex_.occurrenceAt(path, position);
}

std::optional<SymbolId> AnalysisSnapshot::symbolAt(std::string_view path, ast::Position position) const {
	const auto occurrence = occurrenceAt(path, position);
	return occurrence ? occurrence->symbol : std::nullopt;
}

std::optional<TypeRecord> AnalysisSnapshot::typeAt(std::string_view path, ast::Position position) const {
	return semanticIndex_.typeAt(path, position);
}

std::optional<ExpectedTypeRecord> AnalysisSnapshot::expectedTypeAt(std::string_view path, ast::Position position) const {
	return semanticIndex_.expectedTypeAt(path, position);
}

std::optional<CallRecord> AnalysisSnapshot::callAt(std::string_view path, ast::Position position) const {
	return semanticIndex_.callAt(path, position);
}

std::optional<SymbolRecord> AnalysisSnapshot::declaration(SymbolId symbol) const {
	return semanticIndex_.declaration(symbol);
}

std::vector<OccurrenceRecord> AnalysisSnapshot::references(SymbolId symbol) const {
	return semanticIndex_.occurrencesFor(symbol);
}

std::vector<SymbolId> AnalysisSnapshot::visibleSymbolsAt(std::string_view path, ast::Position position) const {
	return semanticIndex_.visibleSymbolsAt(path, position);
}

std::vector<CompilerDiagnostic> AnalysisSnapshot::diagnosticsFor(std::string_view path) const {
	std::vector<CompilerDiagnostic> result;
	for (const auto& diagnostic : compilerDiagnostics_) {
		if (diagnostic.span.file == path) result.push_back(diagnostic);
	}
	return result;
}

} // namespace rls::sema