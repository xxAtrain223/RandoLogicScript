#include "language_server_endpoints_private.h"

#include <vector>

#include "framework.h"
#include "language_server.h"

namespace rls::lsp::detail {

namespace {

const std::vector<endpoints::EndpointDefinition>& allEndpoints() {
    static const std::vector<endpoints::EndpointDefinition> endpoints = [] {
        std::vector<endpoints::EndpointDefinition> materialized;
        const auto& factories = endpoints::endpointFactoryRegistry();
        materialized.reserve(factories.size());
        for (auto factory : factories) {
            materialized.push_back(factory());
        }

        return materialized;
    }();

    return endpoints;
}

const endpoints::EndpointDefinition* findEndpoint(
    const std::vector<endpoints::EndpointDefinition>& endpoints,
    std::string_view method) {
    for (const auto& endpoint : endpoints) {
        if (endpoint.method == method) {
            return &endpoint;
        }
    }

    return nullptr;
}

} // namespace

const ServerCapabilities& EndpointAccess::capabilities(const LanguageServer& server) {
    return server.capabilities_;
}

void EndpointAccess::setShutdown(LanguageServer& server, bool value) {
    server.isShutdown_ = value;
}

void EndpointAccess::setShouldExit(LanguageServer& server, bool value) {
    server.shouldExit_ = value;
}

std::vector<std::string> EndpointAccess::publishDiagnostics(const LanguageServer& server, const std::string& uri) {
    return server.publishDiagnostics(uri);
}

EndpointDispatchResult dispatchEndpoint(const EndpointDispatchInput& input) {
    const auto& endpoints = allEndpoints();

    const endpoints::EndpointDefinition* endpoint = findEndpoint(endpoints, input.method);
    if (endpoint == nullptr) {
        return EndpointDispatchResult{false, {}};
    }

    if (endpoint->invocation == endpoints::EndpointInvocation::RequestOnly && !input.hasId) {
        return EndpointDispatchResult{true, {}};
    }

    if (endpoint->invocation == endpoints::EndpointInvocation::NotificationOnly && input.hasId) {
        return EndpointDispatchResult{
            true,
            {endpoints::makeErrorPayload(input.id, -32600, "Invalid Request").dump()},
        };
    }

    const endpoints::EndpointContext context{input.server, input.method, input.hasId, input.id};
    return EndpointDispatchResult{true, endpoint->invoke(context, input.params)};
}

} // namespace rls::lsp::detail
