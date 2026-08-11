#include "analysis_snapshot.h"

#include "parser.h"
#include "sema.h"

#include <algorithm>

namespace rls::sema {

std::optional<std::shared_ptr<const AnalysisSnapshot>> AnalysisSnapshot::Create(
	std::vector<SourceInput> sources, uint64_t generation) {
	auto snapshot = std::make_shared<AnalysisSnapshot>();
	snapshot->generation_ = generation;
	std::sort(sources.begin(), sources.end(), [](const SourceInput& left, const SourceInput& right) {
		return left.path < right.path;
	});

	for (auto& source : sources) {
		const auto sourceText = ast::SourceText::FromUtf8(source.content);
		if (!sourceText) return std::nullopt;
		auto parsed = rls::parser::ParseStringWithIndex(source.content, source.path);
		snapshot->documents_.push_back({source.path, *sourceText, std::move(parsed.sourceIndex)});
		snapshot->project_.files.push_back(std::move(parsed.file));
	}

	snapshot->diagnostics_ = analyze(snapshot->project_);
	for (const auto& file : snapshot->project_.files) {
		for (const auto& diagnostic : file.diagnostics) {
			snapshot->diagnostics_.push_back(diagnostic);
		}
	}
	snapshot->semanticIndex_ = buildSemanticIndex(snapshot->project_, snapshot->diagnostics_);
	return std::shared_ptr<const AnalysisSnapshot>(std::move(snapshot));
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

} // namespace rls::sema