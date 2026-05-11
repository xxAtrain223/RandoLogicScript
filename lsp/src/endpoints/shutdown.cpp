#include "framework.h"

#include "language_server.h"

namespace rls::lsp::detail::endpoints {
namespace shutdown {

struct EmptyRequest {
};

std::vector<std::string> handleShutdownEndpoint(const EndpointContext& context, const EmptyRequest&) {
    // Shutdown records the state transition and acknowledges the request, but
    // leaves actual process termination to a later exit notification.
    EndpointAccess::setShutdown(context.server, true);
    return context.ok(nullptr);
}

const EndpointRegistrar registerEndpoint([]{
    return makeEndpoint<EmptyRequest>(
        "shutdown",
        "Enter shutdown state",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        handleShutdownEndpoint);
});

} // namespace shutdown

template <>
struct RequestBinder<shutdown::EmptyRequest> {
    static std::optional<shutdown::EmptyRequest> bind(const json&) {
        return shutdown::EmptyRequest{};
    }
};

} // namespace rls::lsp::detail::endpoints
