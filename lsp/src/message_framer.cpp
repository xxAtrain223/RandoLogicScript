#include "rls/lsp/message_framer.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <stdexcept>

namespace rls::lsp {
namespace {

constexpr std::string_view HeaderTerminator = "\r\n\r\n";

std::string_view trim(std::string_view value) {
    const auto isWhitespace = [](char character) {
        return character == ' ' || character == '\t';
    };

    while (!value.empty() && isWhitespace(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && isWhitespace(value.back())) {
        value.remove_suffix(1);
    }
    return value;
}

bool equalsIgnoringAsciiCase(std::string_view left, std::string_view right) {
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a))
                == std::tolower(static_cast<unsigned char>(b));
        });
}

size_t parseContentLength(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        throw std::runtime_error("empty Content-Length header");
    }

    size_t contentLength = 0;
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), contentLength);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::runtime_error("invalid Content-Length header");
    }
    return contentLength;
}

} // namespace

MessageFramer::MessageFramer(size_t maximumPayloadSize, size_t maximumHeaderSize)
    : maximumPayloadSize_(maximumPayloadSize), maximumHeaderSize_(maximumHeaderSize) {
    if (maximumHeaderSize_ < HeaderTerminator.size()) {
        throw std::invalid_argument("maximum header size is too small");
    }
}

void MessageFramer::append(std::string_view bytes) {
    buffer_.append(bytes);
}

std::optional<std::string> MessageFramer::popMessage() {
    const size_t headerEnd = buffer_.find(HeaderTerminator);
    if (headerEnd == std::string::npos) {
        if (buffer_.size() > maximumHeaderSize_) {
            throw std::runtime_error("JSON-RPC header exceeds configured limit");
        }
        return std::nullopt;
    }
    if (headerEnd + HeaderTerminator.size() > maximumHeaderSize_) {
        throw std::runtime_error("JSON-RPC header exceeds configured limit");
    }

    std::optional<size_t> contentLength;
    size_t lineStart = 0;
    while (lineStart < headerEnd) {
        const size_t lineEnd = buffer_.find("\r\n", lineStart);
        if (lineEnd == std::string::npos || lineEnd > headerEnd) {
            throw std::runtime_error("malformed JSON-RPC header");
        }
        const std::string_view line(buffer_.data() + lineStart, lineEnd - lineStart);
        const size_t separator = line.find(':');
        if (separator == std::string_view::npos) {
            throw std::runtime_error("malformed JSON-RPC header");
        }

        if (equalsIgnoringAsciiCase(trim(line.substr(0, separator)), "Content-Length")) {
            if (contentLength.has_value()) {
                throw std::runtime_error("duplicate Content-Length header");
            }
            contentLength = parseContentLength(line.substr(separator + 1));
        }
        lineStart = lineEnd + 2;
    }

    if (!contentLength.has_value()) {
        throw std::runtime_error("missing Content-Length header");
    }
    if (*contentLength > maximumPayloadSize_) {
        throw std::runtime_error("JSON-RPC payload exceeds configured limit");
    }

    const size_t payloadStart = headerEnd + HeaderTerminator.size();
    if (*contentLength > std::numeric_limits<size_t>::max() - payloadStart) {
        throw std::runtime_error("JSON-RPC frame size overflow");
    }
    const size_t frameSize = payloadStart + *contentLength;
    if (buffer_.size() < frameSize) {
        return std::nullopt;
    }

    std::string payload = buffer_.substr(payloadStart, *contentLength);
    buffer_.erase(0, frameSize);
    return payload;
}

std::string MessageFramer::frame(std::string_view payload) {
    return "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n"
        + std::string(payload);
}

} // namespace rls::lsp