#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "ast.h"

namespace rls::lsp {

class LanguageServer;
struct ServerCapabilities;

namespace detail {

/// Shared JSON type used throughout the private LSP dispatch layer.
using json = nlohmann::json;

/// Normalizes a URI for internal comparisons by converting backslashes to
/// forward slashes and collapsing repeated path separators in file URIs.
inline std::string normalizeUri(std::string uri) {
    std::replace(uri.begin(), uri.end(), '\\', '/');

    constexpr std::string_view filePrefix = "file://";
    if (uri.rfind(filePrefix, 0) != 0) {
        return uri;
    }

    std::string normalized = std::string(filePrefix);
    const std::string path = uri.substr(filePrefix.size());

    bool previousWasSlash = false;
    for (char ch : path) {
        if (ch == '/') {
            if (!previousWasSlash) {
                normalized.push_back(ch);
            }
            previousWasSlash = true;
        } else {
            normalized.push_back(ch);
            previousWasSlash = false;
        }
    }

    return normalized;
}

/// Converts an AST source span into the zero-based LSP range object expected
/// by protocol responses and notifications.
inline json toLspRange(const rls::ast::Span& span) {
    auto toLine = [](uint32_t oneBased) {
        return oneBased > 0 ? static_cast<int>(oneBased - 1) : 0;
    };

    auto toCharacter = [](uint32_t oneBased) {
        return oneBased > 0 ? static_cast<int>(oneBased - 1) : 0;
    };

    const int startLine = toLine(span.start.line);
    const int startChar = toCharacter(span.start.column);

    int endLine = toLine(span.end.line);
    int endChar = toCharacter(span.end.column);

    if (span.end.line == 0 && span.end.column == 0) {
        endLine = startLine;
        endChar = startChar;
    }

    return {
        {"start", {{"line", startLine}, {"character", startChar}}},
        {"end", {{"line", endLine}, {"character", endChar}}},
    };
}

/// Grants endpoint implementations access to private LanguageServer state that
/// should not be exposed through the public interface.
struct EndpointAccess {
    /// Returns the server capability set advertised during initialization.
    static const ServerCapabilities& capabilities(const LanguageServer& server);
    /// Marks the server as shut down or active.
    static void setShutdown(LanguageServer& server, bool value);
    /// Marks whether the host process should exit after processing the current
    /// request.
    static void setShouldExit(LanguageServer& server, bool value);
    /// Produces diagnostics notifications for the specified document URI.
    static std::vector<std::string> publishDiagnostics(const LanguageServer& server, const std::string& uri);
};

/// Captures the untyped JSON-RPC fields needed to dispatch one inbound message
/// to a registered endpoint.
struct EndpointDispatchInput {
    /// Server instance receiving the message.
    LanguageServer& server;
    /// JSON-RPC method name used for endpoint lookup.
    std::string_view method;
    /// Params payload from the inbound message.
    const json& params;
    /// Indicates whether the message is a request with an id.
    bool hasId;
    /// Request id to echo in any response payload.
    const json& id;
};

/// Represents the outcome of attempting to dispatch an inbound endpoint call.
struct EndpointDispatchResult {
    /// True when a matching endpoint accepted the message.
    bool handled = false;
    /// Serialized JSON-RPC messages to emit back to the client.
    std::vector<std::string> outbound;
};

/// Dispatches one inbound message to the registered endpoint set.
EndpointDispatchResult dispatchEndpoint(const EndpointDispatchInput& input);

} // namespace detail
} // namespace rls::lsp
