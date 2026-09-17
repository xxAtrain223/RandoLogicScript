#include "analysis_snapshot.h"

#include "parser.h"
#include "sema.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <tuple>

namespace rls::sema {

namespace {

ast::ExprPtr cloneExpr(const ast::ExprPtr& expression);

ast::Expr::Variant cloneExprNode(const ast::Expr::Variant& node) {
	return std::visit([](const auto& value) -> ast::Expr::Variant {
		using T = std::decay_t<decltype(value)>;
		if constexpr (std::is_same_v<T, ast::UnaryExpr>) {
			return ast::UnaryExpr(value.op, cloneExpr(value.operand));
		} else if constexpr (std::is_same_v<T, ast::BinaryExpr>) {
			return ast::BinaryExpr(
				value.op, cloneExpr(value.left), cloneExpr(value.right), value.operatorSpan);
		} else if constexpr (std::is_same_v<T, ast::TernaryExpr>) {
			return ast::TernaryExpr(
				cloneExpr(value.condition), cloneExpr(value.thenBranch),
				cloneExpr(value.elseBranch));
		} else if constexpr (std::is_same_v<T, ast::CallExpr>) {
			std::vector<ast::Arg> arguments;
			arguments.reserve(value.args.size());
			for (const auto& argument : value.args) {
				arguments.emplace_back(argument.name, cloneExpr(argument.value));
			}
			return ast::CallExpr(value.callee, std::move(arguments));
		} else if constexpr (std::is_same_v<T, ast::InvokeExpr>) {
			return ast::InvokeExpr(cloneExpr(value.callee));
		} else if constexpr (std::is_same_v<T, ast::MatchExpr>) {
			std::vector<ast::MatchArm> arms;
			arms.reserve(value.arms.size());
			for (const auto& arm : value.arms) {
				std::vector<ast::ExprPtr> patterns;
				patterns.reserve(arm.patterns.size());
				for (const auto& pattern : arm.patterns) patterns.push_back(cloneExpr(pattern));
				arms.emplace_back(
					std::move(patterns), arm.isDefault, cloneExpr(arm.body), arm.fallthrough);
			}
			return ast::MatchExpr(cloneExpr(value.discriminant), std::move(arms));
		} else if constexpr (std::is_same_v<T, ast::ListExpr>) {
			std::vector<ast::ExprPtr> elements;
			elements.reserve(value.elements.size());
			for (const auto& element : value.elements) elements.push_back(cloneExpr(element));
			return ast::ListExpr(std::move(elements));
		} else {
			return value;
		}
	}, node);
}

ast::ExprPtr cloneExpr(const ast::ExprPtr& expression) {
	if (!expression) return nullptr;
	return std::make_unique<ast::Expr>(cloneExprNode(expression->node), expression->span);
}

std::vector<ast::Param> cloneParams(const std::vector<ast::Param>& parameters) {
	std::vector<ast::Param> result;
	result.reserve(parameters.size());
	for (const auto& parameter : parameters) {
		result.emplace_back(
			parameter.name, parameter.type, cloneExpr(parameter.defaultValue), parameter.span);
	}
	return result;
}

std::vector<ast::Section> cloneSections(const std::vector<ast::Section>& sections) {
	std::vector<ast::Section> result;
	result.reserve(sections.size());
	for (const auto& section : sections) {
		std::vector<ast::Entry> entries;
		entries.reserve(section.entries.size());
		for (const auto& entry : section.entries) {
			entries.emplace_back(entry.name, cloneExpr(entry.condition), entry.span);
		}
		result.emplace_back(section.kind, std::move(entries), section.span);
	}
	return result;
}

ast::Decl cloneDecl(const ast::Decl& declaration) {
	return std::visit([](const auto& value) -> ast::Decl {
		using T = std::decay_t<decltype(value)>;
		if constexpr (std::is_same_v<T, ast::RegionDecl>) {
			std::vector<ast::RegionDataEntry> data;
			data.reserve(value.body.data.size());
			for (const auto& entry : value.body.data) {
				data.emplace_back(entry.key, cloneExpr(entry.value), entry.span);
			}
			return ast::RegionDecl(
				value.key,
				ast::RegionBody(std::move(data), cloneSections(value.body.sections)),
				value.span);
		} else if constexpr (std::is_same_v<T, ast::ExtendRegionDecl>) {
			return ast::ExtendRegionDecl(value.name, cloneSections(value.sections), value.span);
		} else if constexpr (std::is_same_v<T, ast::DefineDecl>) {
			return ast::DefineDecl(
				value.name, cloneParams(value.params), cloneExpr(value.body), value.span);
		} else if constexpr (std::is_same_v<T, ast::ExternDefineDecl>) {
			return ast::ExternDefineDecl(
				value.name, cloneParams(value.params), value.returnType, value.span);
		} else {
			return value;
		}
	}, declaration);
}

ast::File cloneFile(const ast::File& file) {
	ast::File result;
	result.path = file.path;
	result.diagnostics = file.diagnostics;
	result.declarations.reserve(file.declarations.size());
	for (const auto& declaration : file.declarations) {
		result.declarations.push_back(cloneDecl(declaration));
	}
	return result;
}

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
	std::vector<SourceInput> sources, uint64_t generation, std::stop_token cancellation,
	AnalysisSnapshotTimings* timings, std::shared_ptr<const AnalysisSnapshot> previous) {
	if (cancellation.stop_requested()) return std::nullopt;
	if (timings) *timings = {};
	auto snapshot = std::make_shared<AnalysisSnapshot>();
	snapshot->generation_ = generation;
	std::map<std::string, std::string> effectiveSources;
	for (auto& source : sources) effectiveSources[std::move(source.path)] = std::move(source.content);
	std::map<std::string, std::shared_ptr<const ParsedDocument>> previousDocuments;
	if (previous) {
		for (const auto& document : previous->documents_) {
			previousDocuments.emplace(document->path, document);
		}
	}

	for (auto& [path, content] : effectiveSources) {
		if (cancellation.stop_requested()) return std::nullopt;
		std::shared_ptr<const ParsedDocument> document;
		const auto cached = previousDocuments.find(path);
		if (cached != previousDocuments.end()
			&& cached->second->sourceText.content() == content) {
			document = cached->second;
			if (timings) ++timings->documentsReused;
		} else {
			const auto parseStarted = std::chrono::steady_clock::now();
			const auto sourceText = ast::SourceText::FromUtf8(content);
			if (!sourceText) return std::nullopt;
			auto parsed = rls::parser::ParseStringWithIndex(
				content, path, rls::parser::ParseMode::Editor);
			document = std::make_shared<const ParsedDocument>(ParsedDocument{
				path, std::move(*sourceText), std::move(parsed.sourceIndex),
				std::move(parsed.file),
			});
			if (timings) {
				timings->parse += std::chrono::steady_clock::now() - parseStarted;
				++timings->documentsParsed;
			}
		}
		if (cancellation.stop_requested()) return std::nullopt;
		snapshot->documents_.push_back(document);
		const auto materializationStarted = std::chrono::steady_clock::now();
		snapshot->project_.files.push_back(cloneFile(document->file));
		if (timings) {
			timings->astMaterialization +=
				std::chrono::steady_clock::now() - materializationStarted;
		}
	}

	if (cancellation.stop_requested()) return std::nullopt;
	snapshot->diagnostics_ = analyze(
		snapshot->project_, timings ? &timings->analysis : nullptr);
	if (cancellation.stop_requested()) return std::nullopt;
	for (const auto& file : snapshot->project_.files) {
		for (const auto& diagnostic : file.diagnostics) {
			snapshot->diagnostics_.push_back(diagnostic);
		}
	}
	if (cancellation.stop_requested()) return std::nullopt;
	const auto indexStarted = std::chrono::steady_clock::now();
	snapshot->semanticIndex_ = buildSemanticIndex(snapshot->project_, snapshot->diagnostics_);
	if (timings) timings->semanticIndex = std::chrono::steady_clock::now() - indexStarted;
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
	for (const auto& document : documents_) paths.push_back(document->path);
	return paths;
}

const ast::SourceText* AnalysisSnapshot::sourceText(std::string_view path) const {
	const auto it = std::find_if(documents_.begin(), documents_.end(), [&](const auto& document) {
		return document->path == path;
	});
	return it == documents_.end() ? nullptr : &(*it)->sourceText;
}

const rls::parser::SourceIndex* AnalysisSnapshot::sourceIndex(std::string_view path) const {
	const auto it = std::find_if(documents_.begin(), documents_.end(), [&](const auto& document) {
		return document->path == path;
	});
	return it == documents_.end() ? nullptr : &(*it)->sourceIndex;
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