#include "framework.h"

#include "json_bind.h"
#include "language_server.h"

namespace rls::lsp::detail::endpoints::did_change {

struct DidChangeRequest {
    std::string uri;
    int version = 0;
    std::string text;
};

std::vector<std::string> handleDidChangeEndpoint(const EndpointContext& context, const DidChangeRequest& request) {
    if (!context.server.documents().applyFullChange(request.uri, request.version, request.text)) {
        return {};
    }

    return EndpointAccess::publishDiagnostics(context.server, request.uri);
}

} // namespace rls::lsp::detail::endpoints::did_change

namespace rls::lsp::detail::endpoints {

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

        const json* firstChange = asObject(&(*changes)[0]);
        const json* textValue = findMember(firstChange, "text");
        if (textValue == nullptr || !textValue->is_string()) {
            return std::nullopt;
        }

        request.text = textValue->get<std::string>();
        return request;
    }
};

EndpointDefinition makeDidChangeEndpoint() {
    return makeEndpoint<did_change::DidChangeRequest>(
        "textDocument/didChange",
        "Apply full document update and publish diagnostics",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const did_change::DidChangeRequest& request) {
            return did_change::handleDidChangeEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterDidChangeEndpoint(&makeDidChangeEndpoint);
}

} // namespace rls::lsp::detail::endpoints
