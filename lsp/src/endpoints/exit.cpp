#include "framework.h"

#include "language_server.h"

namespace rls::lsp::detail::endpoints {
namespace exit_endpoint {

struct EmptyRequest {
};

std::vector<std::string> handleExitEndpoint(const EndpointContext& context, const EmptyRequest&) {
    // Exit is a notification in normal LSP flow, so it only flips server state
    // for the host process to observe after dispatch completes.
    EndpointAccess::setShouldExit(context.server, true);
    return {};
}

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<EmptyRequest>(
        "exit",
        "Signal server process exit",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        handleExitEndpoint);
});

} // namespace exit_endpoint

template <>
struct RequestBinder<exit_endpoint::EmptyRequest> {
    static std::optional<exit_endpoint::EmptyRequest> bind(const json&) {
        return exit_endpoint::EmptyRequest{};
    }
};

} // namespace rls::lsp::detail::endpoints
