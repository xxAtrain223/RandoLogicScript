#include "framework.h"

#include "json_bind.h"
#include "language_server.h"
#include "symbol_support.h"

namespace rls::lsp::detail::endpoints {
namespace definition {

struct TextDocumentPositionRequest {
    std::string uri;
    int line = -1;
    int character = -1;
};

std::vector<std::string> handleDefinitionEndpoint(
    const EndpointContext& context,
    const TextDocumentPositionRequest& request) {
    if (!context.hasId) {
        return {};
    }

    const TextDocument* doc = context.server.documents().find(request.uri);
    if (doc == nullptr) {
        return context.ok(json::array());
    }

    const auto word = support::extractWordAtPosition(doc->text, request.line, request.character);
    if (!word.has_value()) {
        return context.ok(json::array());
    }

    const auto symbols = support::collectSymbols(context.server.documents());
    json locations = json::array();
    for (const auto& symbol : symbols) {
        // The indexed symbol list represents declaration sites, so the first
        // name match is the definition target we want to return.
        if (symbol.name == word->value) {
            locations.push_back(support::locationFromSpan(symbol.uri, symbol.span));
            break;
        }
    }

    return context.ok(locations);
}

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<TextDocumentPositionRequest>(
        "textDocument/definition",
        "Resolve symbol definition at a position",
        EndpointInvocation::RequestOnly,
        BindFailureBehavior::Ignore,
        handleDefinitionEndpoint);
});

} // namespace definition

template <>
struct RequestBinder<definition::TextDocumentPositionRequest> {
    static std::optional<definition::TextDocumentPositionRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        const json* position = findObjectMember(paramsObject, "position");
        if (textDocument == nullptr || position == nullptr) {
            return std::nullopt;
        }

        definition::TextDocumentPositionRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        request.line = intMemberOr(position, "line", -1);
        request.character = intMemberOr(position, "character", -1);
        return request;
    }
};

} // namespace rls::lsp::detail::endpoints
