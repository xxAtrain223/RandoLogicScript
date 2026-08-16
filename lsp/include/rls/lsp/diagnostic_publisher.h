#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "analysis_snapshot.h"
#include "project.h"
#include "rls/lsp/outbound_message_queue.h"

namespace rls::lsp {

class DiagnosticPublisher {
public:
    explicit DiagnosticPublisher(OutboundMessageQueue& outbound);

    void documentOpened(std::string_view uri);
    void documentClosed(std::string_view uri, bool standalone);
    void clearProject(std::string_view projectId);
    void publishConfigurationDiagnostics(
        const std::vector<project::ConfigurationDiagnostic>& diagnostics);
    void acceptedSnapshot(
        std::string projectId, std::shared_ptr<const sema::AnalysisSnapshot> snapshot);

private:
    struct PublishedDocument {
        std::string uri;
        std::string diagnostics;
    };

    using DocumentPayloads = std::unordered_map<std::string, PublishedDocument>;

    OutboundMessageQueue& outbound_;
    std::mutex mutex_;
    std::unordered_map<std::string, DocumentPayloads> published_;
    DocumentPayloads configurationPublished_;
    std::unordered_set<std::string> suppressed_;
    std::unordered_map<std::string, std::string> openDocumentUris_;
};

} // namespace rls::lsp