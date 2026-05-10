#include "framework.h"

#include "language_server.h"

namespace rls::lsp::detail::endpoints::initialize {

struct EmptyRequest {
};

std::vector<std::string> handleInitializeEndpoint(const EndpointContext& context, const EmptyRequest&) {
    const auto& caps = EndpointAccess::capabilities(context.server);
    const json result = {
        {"capabilities", {
            {"textDocumentSync", static_cast<int>(caps.textDocumentSync)},
            {"definitionProvider", caps.definitionProvider},
            {"referencesProvider", caps.referencesProvider},
            {"hoverProvider", caps.hoverProvider},
            {"completionProvider", {
                {"resolveProvider", false},
                {"triggerCharacters", json::array({"(", ",", ":"})}
            }},
            {"documentSymbolProvider", caps.documentSymbolProvider},
            {"workspaceSymbolProvider", caps.workspaceSymbolProvider},
        }}
    };

    return context.ok(result);
}

} // namespace rls::lsp::detail::endpoints::initialize

namespace rls::lsp::detail::endpoints {

template <>
struct RequestBinder<initialize::EmptyRequest> {
    static std::optional<initialize::EmptyRequest> bind(const json&) {
        return initialize::EmptyRequest{};
    }
};

EndpointDefinition makeInitializeEndpoint() {
    return makeEndpoint<initialize::EmptyRequest>(
        "initialize",
        "Initialize server and advertise capabilities",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const initialize::EmptyRequest& request) {
            return initialize::handleInitializeEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterInitializeEndpoint(&makeInitializeEndpoint);
}

} // namespace rls::lsp::detail::endpoints
