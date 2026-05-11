#include "framework.h"

#include "json_bind.h"
#include "language_server.h"

namespace rls::lsp::detail::endpoints {
namespace did_change {

struct DidChangeRequest {
    std::string uri;
    int version = 0;
    std::string text;
};

std::vector<std::string> handleDidChangeEndpoint(const EndpointContext& context, const DidChangeRequest& request) {
    // Full-document changes for unknown documents or stale versions are ignored
    // by the document store, matching the server's simple sync contract.
    if (!context.server.documents().applyFullChange(request.uri, request.version, request.text)) {
        return {};
    }

    return EndpointAccess::publishDiagnostics(context.server, request.uri);
}

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<DidChangeRequest>(
        "textDocument/didChange",
        "Apply full document update and publish diagnostics",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        handleDidChangeEndpoint);
});

} // namespace did_change

template <>
struct RequestBinder<did_change::DidChangeRequest> {
    static std::optional<did_change::DidChangeRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        if (textDocument == nullptr) {
            return std::nullopt;
        }

        did_change::DidChangeRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        request.version = intMemberOr(textDocument, "version", 0);

        const json* changes = findArrayMember(paramsObject, "contentChanges");
        if (request.uri.empty() || changes == nullptr || changes->empty()) {
            return std::nullopt;
        }

        // The server advertises full document sync, so it consumes the first
        // full replacement text from contentChanges.
        const json* firstChange = asObject(&(*changes)[0]);
        const json* textValue = findMember(firstChange, "text");
        if (textValue == nullptr || !textValue->is_string()) {
            return std::nullopt;
        }

        request.text = textValue->get<std::string>();
        return request;
    }
};

} // namespace rls::lsp::detail::endpoints
