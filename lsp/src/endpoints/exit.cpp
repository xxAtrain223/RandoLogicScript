#include "framework.h"

#include "language_server.h"

namespace rls::lsp::detail::endpoints::exit_endpoint {

struct EmptyRequest {
};

std::vector<std::string> handleExitEndpoint(const EndpointContext& context, const EmptyRequest&) {
    EndpointAccess::setShouldExit(context.server, true);
    return {};
}

} // namespace rls::lsp::detail::endpoints::exit_endpoint

namespace rls::lsp::detail::endpoints {

template <>
struct RequestBinder<exit_endpoint::EmptyRequest> {
    static std::optional<exit_endpoint::EmptyRequest> bind(const json&) {
        return exit_endpoint::EmptyRequest{};
    }
};

EndpointDefinition makeExitEndpoint() {
    return makeEndpoint<exit_endpoint::EmptyRequest>(
        "exit",
        "Signal server process exit",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const exit_endpoint::EmptyRequest& request) {
            return exit_endpoint::handleExitEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterExitEndpoint(&makeExitEndpoint);
}

} // namespace rls::lsp::detail::endpoints
