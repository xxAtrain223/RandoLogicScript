#include "framework.h"

#include "json_bind.h"
#include "language_server.h"

namespace rls::lsp::detail::endpoints::did_open {

struct DidOpenRequest {
    std::string uri;
    std::string languageId;
    int version = 0;
    std::string text;
};

std::vector<std::string> handleDidOpenEndpoint(const EndpointContext& context, const DidOpenRequest& request) {
    context.server.documents().open(request.uri, request.languageId, request.version, request.text);
    return EndpointAccess::publishDiagnostics(context.server, request.uri);
}

} // namespace rls::lsp::detail::endpoints::did_open

namespace rls::lsp::detail::endpoints {

template <>
struct RequestBinder<did_open::DidOpenRequest> {
    static std::optional<did_open::DidOpenRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        if (textDocument == nullptr) {
            return std::nullopt;
        }

        did_open::DidOpenRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        request.languageId = stringMemberOr(textDocument, "languageId", "");
        request.version = intMemberOr(textDocument, "version", 0);
        request.text = stringMemberOr(textDocument, "text", "");

        if (request.uri.empty()) {
            return std::nullopt;
        }

        return request;
    }
};

EndpointDefinition makeDidOpenEndpoint() {
    return makeEndpoint<did_open::DidOpenRequest>(
        "textDocument/didOpen",
        "Open a document and publish diagnostics",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const did_open::DidOpenRequest& request) {
            return did_open::handleDidOpenEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterDidOpenEndpoint(&makeDidOpenEndpoint);
}

} // namespace rls::lsp::detail::endpoints
