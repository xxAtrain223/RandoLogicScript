#include "framework.h"

#include "json_bind.h"
#include "language_server.h"

namespace rls::lsp::detail::endpoints {
namespace did_open {

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

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<DidOpenRequest>(
        "textDocument/didOpen",
        "Open a document and publish diagnostics",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        handleDidOpenEndpoint);
});

} // namespace did_open

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

} // namespace rls::lsp::detail::endpoints
