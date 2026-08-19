#pragma once

#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace rls::lsp {

class InvalidParams : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class RequestFailed : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class JsonRpcRouter {
public:
    using Json = nlohmann::json;
    using RequestHandler = std::function<Json(const Json&)>;
    using NotificationHandler = std::function<void(const Json&)>;

    void registerRequest(std::string method, RequestHandler handler);
    void registerNotification(std::string method, NotificationHandler handler);

    bool contains(std::string_view method) const;
    void requireRoutes(std::initializer_list<std::string_view> methods) const;
    std::vector<std::string> handlePayload(std::string_view payload) const;

private:
    std::unordered_map<std::string, RequestHandler> requests_;
    std::unordered_map<std::string, NotificationHandler> notifications_;
};

} // namespace rls::lsp