#include "framework.h"

#include "json_bind.h"
#include "language_server.h"
#include "symbol_support.h"

namespace rls::lsp::detail::endpoints::references {

struct ReferencesRequest {
    std::string uri;
    int line = -1;
    int character = -1;
    bool includeDeclaration = true;
};

std::vector<std::string> handleReferencesEndpoint(
    const EndpointContext& context,
    const ReferencesRequest& request) {
    if (!context.hasId) {
        return {};
    }

    const TextDocument* sourceDoc = context.server.documents().find(request.uri);
    if (sourceDoc == nullptr) {
        return context.ok(json::array());
    }

    const auto word = support::extractWordAtPosition(sourceDoc->text, request.line, request.character);
    if (!word.has_value()) {
        return context.ok(json::array());
    }

    const auto symbols = support::collectSymbols(context.server.documents());
    std::vector<std::pair<std::string, rls::ast::Span>> declarationSpans;
    for (const auto& symbol : symbols) {
        if (symbol.name == word->value) {
            declarationSpans.push_back({symbol.uri, symbol.span});
        }
    }

    json locations = json::array();
    for (const auto& openUri : context.server.documents().openUris()) {
        const TextDocument* doc = context.server.documents().find(openUri);
        if (doc == nullptr) {
            continue;
        }

        size_t pos = doc->text.find(word->value);
        while (pos != std::string::npos) {
            const bool leftOk = pos == 0 || !support::isIdentifierChar(doc->text[pos - 1]);
            const size_t rightIndex = pos + word->value.size();
            const bool rightOk = rightIndex >= doc->text.size() || !support::isIdentifierChar(doc->text[rightIndex]);

            if (leftOk && rightOk) {
                auto location = support::findWordRange(openUri, doc->text, word->value, pos);
                bool isDeclarationLocation = false;

                if (!request.includeDeclaration) {
                    for (const auto& [declUri, declSpan] : declarationSpans) {
                        const auto declUriValue = declSpan.file.empty() ? declUri : declSpan.file;
                        const auto& range = location["range"];
                        const int startLine = range["start"]["line"].get<int>();
                        const int startCharacter = range["start"]["character"].get<int>();
                        if (declUriValue == openUri
                            && startLine == static_cast<int>(declSpan.start.line > 0 ? declSpan.start.line - 1 : 0)
                            && startCharacter == static_cast<int>(declSpan.start.column > 0 ? declSpan.start.column - 1 : 0)) {
                            isDeclarationLocation = true;
                            break;
                        }
                    }
                }

                if (!isDeclarationLocation) {
                    locations.push_back(std::move(location));
                }
            }

            pos = doc->text.find(word->value, pos + word->value.size());
        }
    }

    return context.ok(locations);
}

} // namespace rls::lsp::detail::endpoints::references

namespace rls::lsp::detail::endpoints {

template <>
struct RequestBinder<references::ReferencesRequest> {
    static std::optional<references::ReferencesRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        const json* position = findObjectMember(paramsObject, "position");
        if (textDocument == nullptr || position == nullptr) {
            return std::nullopt;
        }

        references::ReferencesRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        request.line = intMemberOr(position, "line", -1);
        request.character = intMemberOr(position, "character", -1);

        const json* context = findObjectMember(paramsObject, "context");
        request.includeDeclaration = boolMemberOr(context, "includeDeclaration", true);
        return request;
    }
};

EndpointDefinition makeReferencesEndpoint() {
    return makeEndpoint<references::ReferencesRequest>(
        "textDocument/references",
        "Find symbol references across open documents",
        EndpointInvocation::RequestOnly,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const references::ReferencesRequest& request) {
            return references::handleReferencesEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterReferencesEndpoint(&makeReferencesEndpoint);
}

} // namespace rls::lsp::detail::endpoints
