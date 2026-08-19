#include "rls/lsp/json_rpc_router.h"

#include <optional>
#include <utility>

namespace rls::lsp {
namespace {

using Json = nlohmann::json;

Json errorResponse(const Json& id, int code, std::string_view message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", code}, {"message", message}}},
    };
}

bool isValidId(const Json& id) {
    return id.is_null() || id.is_string() || id.is_number_integer()
        || id.is_number_unsigned();
}

Json invalidRequestId(const Json& message) {
    if (message.is_object() && message.contains("id") && isValidId(message["id"])) {
        return message["id"];
    }
    return nullptr;
}

} // namespace

void JsonRpcRouter::registerRequest(std::string method, RequestHandler handler) {
    if (method.empty() || !handler) {
        throw std::invalid_argument("request route requires a method and handler");
    }
    if (contains(method)) {
        throw std::logic_error("duplicate JSON-RPC route: " + method);
    }
    requests_.emplace(std::move(method), std::move(handler));
}

void JsonRpcRouter::registerNotification(std::string method, NotificationHandler handler) {
    if (method.empty() || !handler) {
        throw std::invalid_argument("notification route requires a method and handler");
    }
    if (contains(method)) {
        throw std::logic_error("duplicate JSON-RPC route: " + method);
    }
    notifications_.emplace(std::move(method), std::move(handler));
}

bool JsonRpcRouter::contains(std::string_view method) const {
    return requests_.contains(std::string(method))
        || notifications_.contains(std::string(method));
}

void JsonRpcRouter::requireRoutes(std::initializer_list<std::string_view> methods) const {
    for (const std::string_view method : methods) {
        if (!contains(method)) {
            throw std::logic_error("missing JSON-RPC route: " + std::string(method));
        }
    }
}

std::vector<std::string> JsonRpcRouter::handlePayload(std::string_view payload) const {
    Json parsed;
    try {
        parsed = Json::parse(payload);
    } catch (const Json::parse_error&) {
        return {errorResponse(nullptr, -32700, "Parse error").dump()};
    }

    const auto dispatch = [this](const Json& message) -> std::optional<Json> {
        if (!message.is_object() || message.value("jsonrpc", "") != "2.0"
            || !message.contains("method") || !message["method"].is_string()) {
            return errorResponse(invalidRequestId(message), -32600, "Invalid Request");
        }

        const bool isRequest = message.contains("id");
        if (isRequest && !isValidId(message["id"])) {
            return errorResponse(nullptr, -32600, "Invalid Request");
        }
        if (message.contains("params")
            && !message["params"].is_object() && !message["params"].is_array()) {
            if (isRequest) {
                return errorResponse(message["id"], -32602, "Invalid params");
            }
            return std::nullopt;
        }

        const std::string method = message["method"].get<std::string>();
        const Json params = message.value("params", Json(nullptr));

        if (isRequest) {
            const auto route = requests_.find(method);
            if (route == requests_.end()) {
                return errorResponse(message["id"], -32601, "Method not found");
            }

            try {
                return Json{
                    {"jsonrpc", "2.0"},
                    {"id", message["id"]},
                    {"result", route->second(params)},
                };
            } catch (const InvalidParams&) {
                return errorResponse(message["id"], -32602, "Invalid params");
            } catch (const RequestFailed& error) {
                return errorResponse(message["id"], -32803, error.what());
            } catch (const Json::exception&) {
                return errorResponse(message["id"], -32602, "Invalid params");
            } catch (const std::exception&) {
                return errorResponse(message["id"], -32603, "Internal error");
            }
        }

        const auto route = notifications_.find(method);
        if (route == notifications_.end()) {
            return std::nullopt;
        }
        try {
            route->second(params);
        } catch (const std::exception&) {
        }
        return std::nullopt;
    };

    if (!parsed.is_array()) {
        const auto response = dispatch(parsed);
        return response ? std::vector<std::string>{response->dump()} : std::vector<std::string>{};
    }
    if (parsed.empty()) {
        return {errorResponse(nullptr, -32600, "Invalid Request").dump()};
    }

    Json responses = Json::array();
    for (const auto& message : parsed) {
        if (const auto response = dispatch(message)) {
            responses.push_back(*response);
        }
    }
    return responses.empty() ? std::vector<std::string>{}
        : std::vector<std::string>{responses.dump()};
}

} // namespace rls::lsp