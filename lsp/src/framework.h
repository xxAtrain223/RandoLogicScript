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

template <typename TRequest>
struct RequestBinder;

inline json makeResponsePayload(const json& id, const json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result},
    };
}

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

inline json makeNotificationPayload(std::string_view method, const json& params) {
    return {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
    };
}

struct EndpointContext {
    LanguageServer& server;
    std::string_view method;
    bool hasId = false;
    const json& id;

    std::vector<std::string> ok(const json& result) const {
        if (!hasId) {
            return {};
        }
        return {makeResponsePayload(id, result).dump()};
    }

    std::vector<std::string> error(int code, std::string_view message) const {
        if (!hasId) {
            return {};
        }
        return {makeErrorPayload(id, code, message).dump()};
    }
};

enum class EndpointInvocation {
    Any,
    RequestOnly,
    NotificationOnly,
};

enum class BindFailureBehavior {
    Ignore,
    InvalidParams,
};

struct EndpointDefinition {
    std::string_view method;
    std::string_view summary;
    EndpointInvocation invocation = EndpointInvocation::Any;
    std::function<std::vector<std::string>(const EndpointContext&, const json&)> invoke;
};

using EndpointFactory = EndpointDefinition (*)();

inline std::vector<EndpointFactory>& endpointFactoryRegistry() {
    static std::vector<EndpointFactory> factories;
    return factories;
}

inline void registerEndpointFactory(EndpointFactory factory) {
    auto& factories = endpointFactoryRegistry();
    if (std::find(factories.begin(), factories.end(), factory) != factories.end()) {
        return;
    }

    factories.push_back(factory);
}

struct EndpointRegistrar {
    explicit EndpointRegistrar(EndpointFactory factory) {
        registerEndpointFactory(factory);
    }
};

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
