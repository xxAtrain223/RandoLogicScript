#include "framework.h"

#include "json_bind.h"
#include "language_server.h"
#include "symbol_support.h"

namespace rls::lsp::detail::endpoints {
namespace hover {

struct TextDocumentPositionRequest {
    std::string uri;
    int line = -1;
    int character = -1;
};

std::vector<std::string> handleHoverEndpoint(
    const EndpointContext& context,
    const TextDocumentPositionRequest& request) {
    if (!context.hasId) {
        return {};
    }

    const TextDocument* doc = context.server.documents().find(request.uri);
    if (doc == nullptr) {
        return context.ok(nullptr);
    }

    const auto word = support::extractWordAtPosition(doc->text, request.line, request.character);
    if (!word.has_value()) {
        return context.ok(nullptr);
    }

    const auto symbols = support::collectSymbols(context.server.documents());
    for (const auto& symbol : symbols) {
        if (symbol.name == word->value) {
            const json hoverResult = {
                {"contents", {
                    {"kind", "plaintext"},
                    {"value", symbol.detail + " " + symbol.name},
                }},
                {"range", {
                    {"start", {{"line", request.line}, {"character", word->startCharacter}}},
                    {"end", {{"line", request.line}, {"character", word->endCharacter}}},
                }}
            };
            return context.ok(hoverResult);
        }
    }

    return context.ok(nullptr);
}

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<TextDocumentPositionRequest>(
        "textDocument/hover",
        "Provide hover information for symbol under cursor",
        EndpointInvocation::RequestOnly,
        BindFailureBehavior::Ignore,
        handleHoverEndpoint);
});

} // namespace hover

template <>
struct RequestBinder<hover::TextDocumentPositionRequest> {
    static std::optional<hover::TextDocumentPositionRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        const json* position = findObjectMember(paramsObject, "position");
        if (textDocument == nullptr || position == nullptr) {
            return std::nullopt;
        }

        hover::TextDocumentPositionRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        request.line = intMemberOr(position, "line", -1);
        request.character = intMemberOr(position, "character", -1);
        return request;
    }
};

} // namespace rls::lsp::detail::endpoints
