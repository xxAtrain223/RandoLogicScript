#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace rls::lsp {

class MessageFramer {
public:
    static constexpr size_t DefaultMaximumPayloadSize = 16 * 1024 * 1024;
    static constexpr size_t DefaultMaximumHeaderSize = 8 * 1024;

    explicit MessageFramer(
        size_t maximumPayloadSize = DefaultMaximumPayloadSize,
        size_t maximumHeaderSize = DefaultMaximumHeaderSize);

    void append(std::string_view bytes);
    std::optional<std::string> popMessage();

    static std::string frame(std::string_view payload);

private:
    size_t maximumPayloadSize_;
    size_t maximumHeaderSize_;
    std::string buffer_;
};

} // namespace rls::lsp