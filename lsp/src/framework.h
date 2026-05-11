#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "language_server_endpoints_private.h"

namespace rls::lsp::detail::endpoints {

/// Binds an untyped JSON parameter object into the strongly typed request model
/// expected by an endpoint handler.
template <typename TRequest>
struct RequestBinder;

/// Builds a JSON-RPC success response payload for a request that produced a
/// result value.
inline json makeResponsePayload(const json& id, const json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result},
    };
}

/// Builds a JSON-RPC error response payload for a request that failed.
inline json makeErrorPayload(const json& id, int code, std::string_view message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", std::string(message)},
        }},
    };
}

/// Builds a JSON-RPC notification payload for a server-initiated message.
inline json makeNotificationPayload(std::string_view method, const json& params) {
    return {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
    };
}

/// Carries the request-scoped state shared with endpoint handlers.
struct EndpointContext {
    /// Owning language server instance handling the current message.
    LanguageServer& server;
    /// JSON-RPC method name currently being dispatched.
    std::string_view method;
    /// Indicates whether the inbound message is a request with a response id.
    bool hasId = false;
    /// JSON-RPC id to echo in any response payload.
    const json& id;

    /// Serializes a success response when the current message is a request.
    std::vector<std::string> ok(const json& result) const {
        if (!hasId) {
            return {};
        }
        return {makeResponsePayload(id, result).dump()};
    }

    /// Serializes an error response when the current message is a request.
    std::vector<std::string> error(int code, std::string_view message) const {
        if (!hasId) {
            return {};
        }
        return {makeErrorPayload(id, code, message).dump()};
    }
};

/// Restricts whether an endpoint may handle requests, notifications, or both.
enum class EndpointInvocation {
    /// Accept either a request or a notification.
    Any,
    /// Accept only messages that carry a JSON-RPC id.
    RequestOnly,
    /// Accept only messages that do not carry a JSON-RPC id.
    NotificationOnly,
};

/// Controls how endpoint binding failures are surfaced to the client.
enum class BindFailureBehavior {
    /// Silently ignore payloads that do not bind to the expected request type.
    Ignore,
    /// Return a JSON-RPC invalid params error when binding fails.
    InvalidParams,
};

/// Describes one callable endpoint exposed by the language server.
struct EndpointDefinition {
    /// JSON-RPC method name used for dispatch.
    std::string_view method;
    /// Human-readable description used by diagnostics and tests.
    std::string_view summary;
    /// Whether this endpoint accepts requests, notifications, or both.
    EndpointInvocation invocation = EndpointInvocation::Any;
    /// Dispatch function that executes the endpoint and returns serialized
    /// outbound messages.
    std::function<std::vector<std::string>(const EndpointContext&, const json&)> invoke;
};

/// Function signature used by static endpoint registration hooks.
using EndpointFactory = EndpointDefinition (*)();

/// Returns the process-wide registry of endpoint factory functions.
inline std::vector<EndpointFactory>& endpointFactoryRegistry() {
    static std::vector<EndpointFactory> factories;
    return factories;
}

/// Adds an endpoint factory to the registry if it has not already been added.
inline void registerEndpointFactory(EndpointFactory factory) {
    auto& factories = endpointFactoryRegistry();
    if (std::find(factories.begin(), factories.end(), factory) != factories.end()) {
        return;
    }

    factories.push_back(factory);
}

/// Helper that registers an endpoint factory during static initialization.
struct EndpointRegistrar {
    /// Registers the supplied factory with the global endpoint registry.
    explicit EndpointRegistrar(EndpointFactory factory) {
        registerEndpointFactory(factory);
    }
};

/// Creates an endpoint definition that binds JSON parameters into a typed
/// request object before invoking the supplied handler.
template <typename TRequest, typename THandler>
EndpointDefinition makeEndpoint(
    std::string_view method,
    std::string_view summary,
    EndpointInvocation invocation,
    BindFailureBehavior bindFailure,
    THandler&& typedHandler) {
    EndpointDefinition endpoint;
    endpoint.method = method;
    endpoint.summary = summary;
    endpoint.invocation = invocation;
    endpoint.invoke = [bindFailure, typedHandler = std::forward<THandler>(typedHandler)]
        (const EndpointContext& context, const json& params) -> std::vector<std::string> {
        auto request = RequestBinder<TRequest>::bind(params);
        if (!request.has_value()) {
            if (bindFailure == BindFailureBehavior::InvalidParams) {
                return context.error(-32602, "Invalid params");
            }
            return {};
        }

        return typedHandler(context, *request);
    };

    return endpoint;
}

} // namespace rls::lsp::detail::endpoints
