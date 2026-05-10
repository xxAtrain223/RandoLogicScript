#include "framework.h"

#include <set>
#include <string>
#include <vector>

#include "json_bind.h"
#include "language_server.h"
#include "symbol_support.h"

namespace rls::lsp::detail::endpoints::completion {

struct TextDocumentPositionRequest {
    std::string uri;
    int line = -1;
    int character = -1;
};

std::vector<std::string> handleCompletionEndpoint(
    const EndpointContext& context,
    const TextDocumentPositionRequest& request) {
    if (!context.hasId) {
        return {};
    }

    const TextDocument* doc = context.server.documents().find(request.uri);
    if (doc == nullptr) {
        return context.ok(json{{"isIncomplete", false}, {"items", json::array()}});
    }

    const std::string prefix = support::prefixAtPosition(doc->text, request.line, request.character);
    const auto callsiteName = support::callsiteFunctionNameAtPosition(doc->text, request.line, request.character);
    const auto symbols = support::collectSymbols(context.server.documents());
    const auto paramsByFunction = support::collectFunctionParams(context.server.documents());

    const std::vector<std::string> keywords = {
        "define", "extern", "region", "extend", "match", "shared", "any_age",
        "true", "false", "always", "never", "and", "or", "not"
    };

    json items = json::array();
    std::set<std::string> seen;

    for (const auto& keyword : keywords) {
        if (!support::startsWithCaseInsensitive(keyword, prefix)) {
            continue;
        }
        if (!seen.insert("kw:" + keyword).second) {
            continue;
        }
        items.push_back({
            {"label", keyword},
            {"kind", 14},
            {"detail", "keyword"},
        });
    }

    for (const auto& symbol : symbols) {
        if (!support::startsWithCaseInsensitive(symbol.name, prefix)) {
            continue;
        }
        if (!seen.insert("sym:" + symbol.name).second) {
            continue;
        }
        items.push_back({
            {"label", symbol.name},
            {"kind", symbol.kind == 12 ? 3 : symbol.kind},
            {"detail", symbol.detail},
        });
    }

    if (callsiteName.has_value()) {
        auto it = paramsByFunction.find(*callsiteName);
        if (it != paramsByFunction.end()) {
            for (const auto& param : it->second) {
                const std::string label = param + ": ";
                if (!support::startsWithCaseInsensitive(param, prefix)) {
                    continue;
                }
                if (!seen.insert("param:" + label).second) {
                    continue;
                }
                items.push_back({
                    {"label", label},
                    {"kind", 5},
                    {"detail", "parameter"},
                });
            }
        }
    }

    return context.ok(json{{"isIncomplete", false}, {"items", std::move(items)}});
}

} // namespace rls::lsp::detail::endpoints::completion

namespace rls::lsp::detail::endpoints {

template <>
struct RequestBinder<completion::TextDocumentPositionRequest> {
    static std::optional<completion::TextDocumentPositionRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        const json* position = findObjectMember(paramsObject, "position");
        if (textDocument == nullptr || position == nullptr) {
            return std::nullopt;
        }

        completion::TextDocumentPositionRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        request.line = intMemberOr(position, "line", -1);
        request.character = intMemberOr(position, "character", -1);
        return request;
    }
};

EndpointDefinition makeCompletionEndpoint() {
    return makeEndpoint<completion::TextDocumentPositionRequest>(
        "textDocument/completion",
        "Return completion items for cursor position",
        EndpointInvocation::RequestOnly,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const completion::TextDocumentPositionRequest& request) {
            return completion::handleCompletionEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterCompletionEndpoint(&makeCompletionEndpoint);
}

} // namespace rls::lsp::detail::endpoints
