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

using json = nlohmann::json;

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

struct EndpointAccess {
    static const ServerCapabilities& capabilities(const LanguageServer& server);
    static void setShutdown(LanguageServer& server, bool value);
    static void setShouldExit(LanguageServer& server, bool value);
    static std::vector<std::string> publishDiagnostics(const LanguageServer& server, const std::string& uri);
};

struct EndpointDispatchInput {
    LanguageServer& server;
    std::string_view method;
    const json& params;
    bool hasId;
    const json& id;
};

struct EndpointDispatchResult {
    bool handled = false;
    std::vector<std::string> outbound;
};

EndpointDispatchResult dispatchEndpoint(const EndpointDispatchInput& input);

} // namespace detail
} // namespace rls::lsp
