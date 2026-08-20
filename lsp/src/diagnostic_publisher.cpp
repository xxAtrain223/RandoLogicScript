#include "rls/lsp/diagnostic_publisher.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <fstream>
#include <iterator>
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

Json rangeFor(const project::ConfigurationDiagnostic& diagnostic) {
    std::ifstream input(diagnostic.path, std::ios::binary);
    if (!input) {
        return zeroRange();
    }
    const std::string content{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto source = ast::SourceText::FromUtf8(content);
    if (!source) {
        return zeroRange();
    }
    const size_t startOffset = std::min(diagnostic.startByte, content.size());
    const size_t endOffset = std::min(
        std::max(diagnostic.endByte, startOffset), content.size());
    const auto start = source->utf16PositionAtByteOffset(startOffset);
    const auto end = source->utf16PositionAtByteOffset(endOffset);
    if (!start || !end) {
        return zeroRange();
    }
    return {
        {"start", {{"line", start->line - 1}, {"character", start->column - 1}}},
        {"end", {{"line", end->line - 1}, {"character", end->column - 1}}},
    };
}

std::optional<std::string> uriForSource(std::string_view identity) {
    const bool windowsDrivePath = identity.size() >= 3
        && std::isalpha(static_cast<unsigned char>(identity[0]))
        && identity[1] == ':' && identity[2] == '/';
    if (!windowsDrivePath) {
        if (const auto normalized = NormalizeDocumentUri(identity)) {
            return normalized;
        }
    }
    return PathToFileUri(std::filesystem::path(identity));
}

std::optional<std::string> canonicalUriKey(std::string_view uri) {
    const auto path = FileUriToPath(uri);
    if (!path) {
        return DocumentUriKey(uri);
    }
    const auto canonicalUri = PathToFileUri(*path);
    return canonicalUri ? DocumentUriKey(*canonicalUri) : std::nullopt;
}

Json actionData(const ast::DiagnosticActionData& data) {
    return {
        {"version", data.version},
        {"actionKind", data.actionKind},
        {"arguments", data.arguments},
    };
}

Json actionData(const project::ConfigurationDiagnosticData& data) {
    return {
        {"version", data.version},
        {"actionKind", data.actionKind},
        {"arguments", data.arguments},
    };
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
            const auto relatedUri = uriForSource(related.span.file);
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
        if (diagnostic.data) {
            value["data"] = actionData(*diagnostic.data);
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
    const auto key = canonicalUriKey(uri);
    const auto normalized = NormalizeDocumentUri(uri);
    if (!key || !normalized) {
        return;
    }
    std::lock_guard lock(mutex_);
    suppressed_.erase(*key);
    openedUris_[*key] = *normalized;
}

void DiagnosticPublisher::documentClosed(std::string_view uri, bool standalone) {
    const auto normalized = NormalizeDocumentUri(uri);
    const auto key = canonicalUriKey(uri);
    if (!normalized || !key) {
        return;
    }

    {
        std::lock_guard lock(mutex_);
        openedUris_.erase(*key);
        if (!standalone) {
            return;
        }
        suppressed_.insert(*key);
        for (auto& [projectId, documents] : published_) {
            documents.erase(*key);
        }
    }
    outbound_.push(notification(*normalized, Json::array()));
}

void DiagnosticPublisher::clearProject(std::string_view projectId) {
    std::vector<std::string> messages;
    {
        std::lock_guard lock(mutex_);
        const auto project = published_.find(std::string(projectId));
        if (project == published_.end()) {
            return;
        }
        for (const auto& [key, document] : project->second) {
            if (!suppressed_.contains(key)) {
                messages.push_back(notification(document.uri, Json::array()));
            }
        }
        published_.erase(project);
    }
    for (auto& message : messages) {
        outbound_.push(std::move(message));
    }
}

void DiagnosticPublisher::publishConfigurationDiagnostics(
    const std::vector<project::ConfigurationDiagnostic>& diagnostics) {
    std::unordered_map<std::string, Json> grouped;
    std::unordered_map<std::string, std::string> uris;
    for (const auto& diagnostic : diagnostics) {
        const auto uri = PathToFileUri(diagnostic.path);
        if (!uri) {
            continue;
        }
        const auto key = DocumentUriKey(*uri);
        if (!key) {
            continue;
        }
        if (!grouped.contains(*key)) grouped[*key] = Json::array();
        Json value = {
            {"range", rangeFor(diagnostic)},
            {"severity", 1},
            {"code", diagnostic.code},
            {"source", "rls"},
            {"message", diagnostic.message},
        };
        if (diagnostic.data) {
            value["data"] = actionData(*diagnostic.data);
        }
        grouped[*key].push_back(std::move(value));
        uris[*key] = *uri;
    }

    DocumentPayloads current;
    for (auto& [key, values] : grouped) {
        current[key] = PublishedDocument{uris.at(key), values.dump()};
    }

    std::vector<std::string> messages;
    {
        std::lock_guard lock(mutex_);
        for (const auto& [key, document] : current) {
            const auto old = configurationPublished_.find(key);
            if (old == configurationPublished_.end()
                || old->second.diagnostics != document.diagnostics) {
                messages.push_back(notification(
                    document.uri, Json::parse(document.diagnostics)));
            }
        }
        for (const auto& [key, document] : configurationPublished_) {
            if (!current.contains(key)) {
                messages.push_back(notification(document.uri, Json::array()));
            }
        }
        configurationPublished_ = std::move(current);
    }
    for (auto& message : messages) {
        outbound_.push(std::move(message));
    }
}

void DiagnosticPublisher::acceptedSnapshot(
    std::string projectId, std::shared_ptr<const sema::AnalysisSnapshot> snapshot) {
    if (!snapshot) {
        return;
    }

    DocumentPayloads current;
    for (const auto& path : snapshot->documentPaths()) {
        const auto uri = uriForSource(path);
        if (!uri) {
            continue;
        }
        const auto key = canonicalUriKey(*uri);
        if (!key) {
            continue;
        }
        std::string publishedUri = *uri;
        {
            std::lock_guard lock(mutex_);
            const auto opened = openedUris_.find(*key);
            if (opened != openedUris_.end()) {
                publishedUri = opened->second;
            }
        }
        current[*key] = PublishedDocument{
            std::move(publishedUri),
            diagnosticsFor(*snapshot, path).dump(),
        };
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