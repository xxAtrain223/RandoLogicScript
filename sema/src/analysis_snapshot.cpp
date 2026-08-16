#include "analysis_snapshot.h"

#include "parser.h"
#include "sema.h"

#include <algorithm>
#include <map>
#include <tuple>

namespace rls::sema {

namespace {

std::vector<const SymbolRecord*> parametersFor(
	const SemanticIndex& index, SymbolId callable) {
	std::vector<const SymbolRecord*> result;
	for (const auto& symbol : index.symbols()) {
		if (symbol.category == SymbolCategory::Parameter
			&& symbol.container == callable) {
			result.push_back(&symbol);
		}
	}
	std::sort(result.begin(), result.end(), [](const auto* left, const auto* right) {
		return std::tie(left->selection.start.line, left->selection.start.column)
			< std::tie(right->selection.start.line, right->selection.start.column);
	});
	return result;
}

std::optional<SymbolId> uniqueCallable(
	const SemanticIndex& index, std::string_view name) {
	std::optional<SymbolId> result;
	for (const auto& symbol : index.symbols()) {
		const bool callable = symbol.category == SymbolCategory::Define
			|| symbol.category == SymbolCategory::ExternDefine;
		if (!callable || symbol.displayName != name) continue;
		if (result) return std::nullopt;
		result = symbol.id;
	}
	return result;
}

std::optional<CallRecord> resolveRecoveredCall(
	const SemanticIndex& semanticIndex,
	const parser::CallContext& call) {
	if (call.argumentRanges.size() != call.argumentLabels.size()
		|| call.argumentRanges.size() != call.argumentLabelNames.size()) {
		return std::nullopt;
	}
	const auto target = uniqueCallable(semanticIndex, call.calleeName);
	if (!target) return std::nullopt;
	const auto parameters = parametersFor(semanticIndex, *target);
	std::vector<bool> bound(parameters.size(), false);
	std::vector<std::optional<size_t>> bindings;
	bindings.reserve(call.argumentRanges.size());
	size_t nextPositional = 0;

	for (const auto& label : call.argumentLabelNames) {
		size_t parameterIndex = parameters.size();
		if (label) {
			for (size_t index = 0; index < parameters.size(); ++index) {
				if (parameters[index]->displayName == *label) {
					parameterIndex = index;
					break;
				}
			}
			if (parameterIndex == parameters.size() || bound[parameterIndex]) {
				return std::nullopt;
			}
		} else {
			while (nextPositional < bound.size() && bound[nextPositional]) {
				++nextPositional;
			}
			if (nextPositional == parameters.size()) return std::nullopt;
			parameterIndex = nextPositional++;
		}
		bound[parameterIndex] = true;
		bindings.push_back(parameterIndex);
	}

	return CallRecord{
		call.span, *target, call.argumentRanges, std::move(bindings)};
}

} // namespace

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
		auto parsed = rls::parser::ParseStringWithIndex(
			content, path, rls::parser::ParseMode::Editor);
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
			diagnostic.message, diagnostic.span, {}, diagnostic.data});
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
	if (const auto expected = semanticIndex_.expectedTypeAt(path, position)) {
		return expected;
	}
	const auto* index = sourceIndex(path);
	if (!index) return std::nullopt;
	const auto argument = index->callArgumentAt(position);
	const auto call = callAt(path, position);
	if (!argument || !call || !call->target
		|| argument->activeArgument >= call->normalizedBindings.size()) {
		return std::nullopt;
	}
	const auto binding = call->normalizedBindings[argument->activeArgument];
	if (!binding) return std::nullopt;
	const auto parameters = parametersFor(semanticIndex_, *call->target);
	if (*binding >= parameters.size() || !parameters[*binding]->type) {
		return std::nullopt;
	}
	return ExpectedTypeRecord{
		argument->valueSpan, *parameters[*binding]->type,
		parameters[*binding]->enumName};
}

std::optional<CallRecord> AnalysisSnapshot::callAt(std::string_view path, ast::Position position) const {
	if (const auto call = semanticIndex_.callAt(path, position)) return call;
	const auto* index = sourceIndex(path);
	if (!index) return std::nullopt;
	const auto call = index->enclosingCall(position);
	return call ? resolveRecoveredCall(semanticIndex_, *call) : std::nullopt;
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