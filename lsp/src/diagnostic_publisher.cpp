#include "rls/lsp/diagnostic_publisher.h"

#include <optional>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

int severity(ast::DiagnosticLevel level) {
    switch (level) {
    case ast::DiagnosticLevel::Error:
        return 1;
    case ast::DiagnosticLevel::Warning:
        return 2;
    case ast::DiagnosticLevel::Info:
        return 3;
    }
    return 3;
}

Json zeroRange() {
    return {
        {"start", {{"line", 0}, {"character", 0}}},
        {"end", {{"line", 0}, {"character", 0}}},
    };
}

Json rangeFor(const sema::AnalysisSnapshot& snapshot, const ast::Span& span) {
    const ast::SourceText* source = snapshot.sourceText(span.file);
    if (!source) {
        return zeroRange();
    }
    const auto startOffset = source->byteOffsetFromUtf8Position(span.start);
    const auto endOffset = source->byteOffsetFromUtf8Position(span.end);
    if (!startOffset || !endOffset) {
        return zeroRange();
    }
    const auto start = source->utf16PositionAtByteOffset(*startOffset);
    const auto end = source->utf16PositionAtByteOffset(*endOffset);
    if (!start || !end) {
        return zeroRange();
    }
    return Json{
        {"start", {{"line", start->line - 1}, {"character", start->column - 1}}},
        {"end", {{"line", end->line - 1}, {"character", end->column - 1}}},
    };
}

std::optional<std::string> uriForPath(std::string_view path) {
    return PathToFileUri(std::filesystem::path(path));
}

Json diagnosticsFor(const sema::AnalysisSnapshot& snapshot, std::string_view path) {
    Json diagnostics = Json::array();
    for (const auto& diagnostic : snapshot.diagnosticsFor(path)) {
        Json value = {
            {"range", rangeFor(snapshot, diagnostic.span)},
            {"severity", severity(diagnostic.level)},
            {"code", diagnostic.code},
            {"source", "rls"},
            {"message", diagnostic.message},
        };
        Json relatedInformation = Json::array();
        for (const auto& related : diagnostic.related) {
            const auto relatedUri = uriForPath(related.span.file);
            if (!relatedUri) {
                continue;
            }
            relatedInformation.push_back({
                {"location", {
                    {"uri", *relatedUri},
                    {"range", rangeFor(snapshot, related.span)},
                }},
                {"message", related.message},
            });
        }
        if (!relatedInformation.empty()) {
            value["relatedInformation"] = std::move(relatedInformation);
        }
        diagnostics.push_back(std::move(value));
    }
    return diagnostics;
}

std::string notification(std::string_view uri, Json diagnostics) {
    return Json{
        {"jsonrpc", "2.0"},
        {"method", "textDocument/publishDiagnostics"},
        {"params", {{"uri", uri}, {"diagnostics", std::move(diagnostics)}}},
    }.dump();
}

} // namespace

DiagnosticPublisher::DiagnosticPublisher(OutboundMessageQueue& outbound)
    : outbound_(outbound) {}

void DiagnosticPublisher::documentOpened(std::string_view uri) {
    const auto key = DocumentUriKey(uri);
    if (!key) {
        return;
    }
    std::lock_guard lock(mutex_);
    suppressed_.erase(*key);
}

void DiagnosticPublisher::documentClosed(std::string_view uri, bool standalone) {
    if (!standalone) {
        return;
    }
    const auto normalized = NormalizeDocumentUri(uri);
    const auto key = DocumentUriKey(uri);
    if (!normalized || !key) {
        return;
    }

    {
        std::lock_guard lock(mutex_);
        suppressed_.insert(*key);
        for (auto& [projectId, documents] : published_) {
            documents.erase(*key);
        }
    }
    outbound_.push(notification(*normalized, Json::array()));
}

void DiagnosticPublisher::acceptedSnapshot(
    std::string projectId, std::shared_ptr<const sema::AnalysisSnapshot> snapshot) {
    if (!snapshot) {
        return;
    }

    DocumentPayloads current;
    for (const auto& path : snapshot->documentPaths()) {
        const auto uri = uriForPath(path);
        if (!uri) {
            continue;
        }
        const auto key = DocumentUriKey(*uri);
        if (!key) {
            continue;
        }
        current[*key] = PublishedDocument{*uri, diagnosticsFor(*snapshot, path).dump()};
    }

    std::vector<std::string> messages;
    {
        std::lock_guard lock(mutex_);
        DocumentPayloads& previous = published_[projectId];
        for (const auto& [key, document] : current) {
            if (suppressed_.contains(key)) {
                continue;
            }
            const auto old = previous.find(key);
            if (old == previous.end() || old->second.diagnostics != document.diagnostics) {
                messages.push_back(notification(
                    document.uri, Json::parse(document.diagnostics)));
            }
        }
        for (const auto& [key, document] : previous) {
            if (!current.contains(key) && !suppressed_.contains(key)) {
                messages.push_back(notification(document.uri, Json::array()));
            }
        }
        for (const auto& key : suppressed_) {
            current.erase(key);
        }
        previous = std::move(current);
    }

    for (auto& message : messages) {
        outbound_.push(std::move(message));
    }
}

} // namespace rls::lsp