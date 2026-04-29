#include "transport.h"

#include <cctype>
#include <stdexcept>

namespace rls::lsp {

void JsonRpcMessageFramer::append(std::string_view chunk) {
    buffer_.append(chunk.begin(), chunk.end());
}

std::optional<std::string> JsonRpcMessageFramer::popMessage() {
    constexpr std::string_view kHeaderTerminator = "\r\n\r\n";
    const auto headerEndPos = buffer_.find(kHeaderTerminator);
    if (headerEndPos == std::string::npos) {
        return std::nullopt;
    }

    const auto header = buffer_.substr(0, headerEndPos + kHeaderTerminator.size());

    size_t contentLength = 0;
    bool foundContentLength = false;

    size_t lineStart = 0;
    while (lineStart < header.size()) {
        const auto lineEnd = header.find("\r\n", lineStart);
        if (lineEnd == std::string::npos) {
            break;
        }

        const auto line = header.substr(lineStart, lineEnd - lineStart);
        lineStart = lineEnd + 2;

        constexpr std::string_view kContentLength = "Content-Length:";
        if (line.rfind(kContentLength, 0) == 0) {
            std::string value = line.substr(kContentLength.size());
            size_t firstNonSpace = 0;
            while (firstNonSpace < value.size()
                && std::isspace(static_cast<unsigned char>(value[firstNonSpace]))) {
                ++firstNonSpace;
            }
            value.erase(0, firstNonSpace);

            if (value.empty()) {
                throw std::runtime_error("invalid Content-Length header");
            }

            contentLength = static_cast<size_t>(std::stoul(value));
            foundContentLength = true;
        }
    }

    if (!foundContentLength) {
        throw std::runtime_error("missing Content-Length header");
    }

    const size_t payloadStart = headerEndPos + kHeaderTerminator.size();
    const size_t totalFrameSize = payloadStart + contentLength;
    if (buffer_.size() < totalFrameSize) {
        return std::nullopt;
    }

    std::string payload = buffer_.substr(payloadStart, contentLength);
    buffer_.erase(0, totalFrameSize);
    return payload;
}

std::string JsonRpcMessageFramer::encode(std::string_view payload) {
    return "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + std::string(payload);
}

} // namespace rls::lsp
