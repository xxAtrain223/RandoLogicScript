#include "language_server.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "ast.h"
#include "language_server_endpoints_private.h"
#include "parser.h"
#include "sema.h"

namespace rls::lsp {

namespace {

using json = nlohmann::json;

enum class TraceLevel {
    Off,
    Error,
    Info,
    Debug,
};

TraceLevel configuredTraceLevel() {
    static const TraceLevel level = [] {
        std::string traceValue;
#ifdef _MSC_VER
        char* value = nullptr;
        size_t valueLen = 0;
        if (_dupenv_s(&value, &valueLen, "RLS_LSP_TRACE") == 0 && value != nullptr) {
            traceValue.assign(value);
            free(value);
        }
#else
        const char* value = std::getenv("RLS_LSP_TRACE");
        if (value != nullptr) {
            traceValue.assign(value);
        }
#endif

        if (traceValue.empty()) {
            return TraceLevel::Off;
        }

        std::string v = traceValue;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (v == "error") {
            return TraceLevel::Error;
        }
        if (v == "info") {
            return TraceLevel::Info;
        }
        if (v == "debug") {
            return TraceLevel::Debug;
        }
        return TraceLevel::Off;
    }();

    return level;
}

bool shouldTrace(TraceLevel level) {
    return static_cast<int>(configuredTraceLevel()) >= static_cast<int>(level)
        && configuredTraceLevel() != TraceLevel::Off;
}

void trace(TraceLevel level, std::string_view message, const json& data = nullptr) {
    if (!shouldTrace(level)) {
        return;
    }

    const char* levelName = "debug";
    switch (level) {
    case TraceLevel::Error:
        levelName = "error";
        break;
    case TraceLevel::Info:
        levelName = "info";
        break;
    case TraceLevel::Debug:
        levelName = "debug";
        break;
    case TraceLevel::Off:
        return;
    }

    json payload = {
        {"component", "rls_lsp"},
        {"level", levelName},
        {"message", message},
    };
    if (!data.is_null()) {
        payload["data"] = data;
    }

    std::cerr << payload.dump() << "\n";
}

int severityFromDiagnosticLevel(rls::ast::DiagnosticLevel level) {
    switch (level) {
    case rls::ast::DiagnosticLevel::Error:
        return 1;
    case rls::ast::DiagnosticLevel::Warning:
        return 2;
    case rls::ast::DiagnosticLevel::Info:
        return 3;
    }

    return 3;
}

json toLspDiagnostic(const rls::ast::Diagnostic& diagnostic) {
    return {
        {"range", detail::toLspRange(diagnostic.span)},
        {"severity", severityFromDiagnosticLevel(diagnostic.level)},
        {"source", "rls"},
        {"message", diagnostic.message},
    };
}

json makeErrorResponse(const json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message},
        }},
    };
}

json makeNotification(std::string_view method, const json& params) {
    return {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
    };
}

const json kNullJson = nullptr;

} // namespace

LanguageServer::LanguageServer() = default;

std::vector<std::string> LanguageServer::handlePayload(std::string_view payload) {
    json message;
    try {
        message = json::parse(payload);
    } catch (const std::exception& e) {
        trace(TraceLevel::Error, "json parse failure", {{"error", e.what()}});
        return {makeErrorResponse(nullptr, -32700, "Parse error").dump()};
    }

    if (!message.is_object()) {
        return {makeErrorResponse(nullptr, -32600, "Invalid Request").dump()};
    }

    if (!message.contains("jsonrpc") || message["jsonrpc"] != "2.0") {
        const bool hasId = message.contains("id");
        const json id = hasId ? message["id"] : json(nullptr);
        return {makeErrorResponse(id, -32600, "Invalid Request").dump()};
    }

    const bool hasId = message.contains("id");
    const json id = hasId ? message["id"] : json(nullptr);

    if (!message.contains("method") || !message["method"].is_string()) {
        if (hasId) {
            return {makeErrorResponse(id, -32600, "Invalid Request").dump()};
        }
        return {};
    }

    const std::string method = message.value("method", "");
    const json& params = message.contains("params") ? message["params"] : kNullJson;

    trace(TraceLevel::Debug, "handle method", {{"method", method}});

    try {
        const auto dispatch = detail::dispatchEndpoint(detail::EndpointDispatchInput{
            *this,
            method,
            params,
            hasId,
            id,
        });

        if (dispatch.handled) {
            return dispatch.outbound;
        }
    } catch (const std::exception& e) {
        trace(TraceLevel::Error, "handler failure", {{"method", method}, {"error", e.what()}});
        if (hasId) {
            return {makeErrorResponse(id, -32603, "Internal error").dump()};
        }
        return {};
    }

    if (hasId) {
        return {makeErrorResponse(id, -32601, "Method not found").dump()};
    }

    return {};
}

const ServerCapabilities& LanguageServer::capabilities() const {
    return capabilities_;
}

DocumentStore& LanguageServer::documents() {
    return documents_;
}

const DocumentStore& LanguageServer::documents() const {
    return documents_;
}

bool LanguageServer::shouldExit() const {
    return shouldExit_;
}

std::vector<std::string> LanguageServer::publishDiagnostics(const std::string& uri) const {
    std::vector<std::string> outbound;

    const TextDocument* doc = documents_.find(uri);
    if (doc == nullptr) {
        return outbound;
    }

    rls::ast::File file;
    try {
        file = rls::parser::ParseString(doc->text, uri);
    } catch (const std::exception& e) {
        trace(TraceLevel::Error, "parse exception", {{"uri", uri}, {"error", e.what()}});
        const json lspDiagnostics = json::array({
            {
                {"range", {
                    {"start", {{"line", 0}, {"character", 0}}},
                    {"end", {{"line", 0}, {"character", 0}}},
                }},
                {"severity", 1},
                {"source", "rls"},
                {"message", std::string("internal parser failure: ") + e.what()},
            }
        });

        outbound.push_back(makeNotification(
            "textDocument/publishDiagnostics",
            {
                {"uri", uri},
                {"diagnostics", lspDiagnostics},
            }).dump());
        return outbound;
    }

    std::vector<rls::ast::Diagnostic> diagnostics;
    diagnostics.reserve(file.diagnostics.size());
    for (const auto& diagnostic : file.diagnostics) {
        diagnostics.push_back(diagnostic);
    }

    bool hasParseErrors = false;
    for (const auto& diagnostic : file.diagnostics) {
        if (diagnostic.level == rls::ast::DiagnosticLevel::Error) {
            hasParseErrors = true;
            break;
        }
    }

    if (!hasParseErrors) {
        rls::ast::Project project;
        project.files.push_back(std::move(file));

        try {
            auto semaDiagnostics = rls::sema::analyze(project);
            diagnostics.insert(
                diagnostics.end(),
                std::make_move_iterator(semaDiagnostics.begin()),
                std::make_move_iterator(semaDiagnostics.end()));
        } catch (const std::exception& e) {
            trace(TraceLevel::Error, "sema exception", {{"uri", uri}, {"error", e.what()}});
            diagnostics.push_back({
                rls::ast::DiagnosticLevel::Error,
                std::string("internal semantic analysis failure: ") + e.what(),
                rls::ast::Span{uri, {1, 1}, {1, 1}},
            });
        }
    }

    json lspDiagnostics = json::array();
    for (const auto& diagnostic : diagnostics) {
        lspDiagnostics.push_back(toLspDiagnostic(diagnostic));
    }

    outbound.push_back(makeNotification(
        "textDocument/publishDiagnostics",
        {
            {"uri", uri},
            {"diagnostics", std::move(lspDiagnostics)},
        }).dump());

    return outbound;
}

} // namespace rls::lsp
