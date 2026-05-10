#include "framework.h"

#include "language_server.h"

namespace rls::lsp::detail::endpoints::shutdown {

struct EmptyRequest {
};

std::vector<std::string> handleShutdownEndpoint(const EndpointContext& context, const EmptyRequest&) {
    EndpointAccess::setShutdown(context.server, true);
    return context.ok(nullptr);
}

} // namespace rls::lsp::detail::endpoints::shutdown

namespace rls::lsp::detail::endpoints {

template <>
struct RequestBinder<shutdown::EmptyRequest> {
    static std::optional<shutdown::EmptyRequest> bind(const json&) {
        return shutdown::EmptyRequest{};
    }
};

EndpointDefinition makeShutdownEndpoint() {
    return makeEndpoint<shutdown::EmptyRequest>(
        "shutdown",
        "Enter shutdown state",
        EndpointInvocation::Any,
        BindFailureBehavior::Ignore,
        [](const EndpointContext& context, const shutdown::EmptyRequest& request) {
            return shutdown::handleShutdownEndpoint(context, request);
        });
}

namespace {
const EndpointRegistrar kRegisterShutdownEndpoint(&makeShutdownEndpoint);
}

} // namespace rls::lsp::detail::endpoints
