#include "framework.h"

#include "language_server.h"

namespace rls::lsp::detail::endpoints {
namespace initialize {

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

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<EmptyRequest>(
        "initialize",
        "Initialize server and advertise capabilities",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        handleInitializeEndpoint);
});

} // namespace initialize

template <>
struct RequestBinder<initialize::EmptyRequest> {
    static std::optional<initialize::EmptyRequest> bind(const json&) {
        return initialize::EmptyRequest{};
    }
};

} // namespace rls::lsp::detail::endpoints
