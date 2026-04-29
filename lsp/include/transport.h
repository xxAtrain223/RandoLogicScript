#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rls::lsp {

class JsonRpcMessageFramer {
public:
    void append(std::string_view chunk);

    // Returns the next complete JSON-RPC payload without headers.
    // Returns nullopt when the buffer does not contain a full frame yet.
    std::optional<std::string> popMessage();

    static std::string encode(std::string_view payload);

private:
    std::string buffer_;
};

} // namespace rls::lsp
