#include "framework.h"

#include "json_bind.h"
#include "language_server.h"

namespace rls::lsp::detail::endpoints::did_close {

struct DidCloseRequest {
    std::string uri;
};

std::vector<std::string> handleDidCloseEndpoint(const EndpointContext& context, const DidCloseRequest& request) {
    if (!context.server.documents().close(request.uri)) {
        return {};
    }

    return {
        makeNotificationPayload(
            "textDocument/publishDiagnostics",
            {
                {"uri", request.uri},
                {"diagnostics", json::array()},
            }).dump()
    };
}

} // namespace rls::lsp::detail::endpoints::did_close

namespace rls::lsp::detail::endpoints {

template <>
struct RequestBinder<did_close::DidCloseRequest> {
    static std::optional<did_close::DidCloseRequest> bind(const json& params) {
        const json* paramsObject = asObject(&params);
        const json* textDocument = findObjectMember(paramsObject, "textDocument");
        if (textDocument == nullptr) {
            return std::nullopt;
        }

        did_close::DidCloseRequest request;
        request.uri = normalizeUri(stringMemberOr(textDocument, "uri", ""));
        if (request.uri.empty()) {
            return std::nullopt;
        }

        return request;
    }
};

EndpointDefinition makeDidCloseEndpoint() {
    return makeEndpoint<did_close::DidCloseRequest>(
        "textDocument/didClose",
        "Close a document and clear diagnostics",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const did_close::DidCloseRequest& request) {
            return did_close::handleDidCloseEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterDidCloseEndpoint(&makeDidCloseEndpoint);
}

} // namespace rls::lsp::detail::endpoints
