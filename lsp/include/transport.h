#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rls::lsp {

/// Incrementally frames and deframes Language Server Protocol messages that
/// use JSON-RPC payloads with Content-Length headers.
class JsonRpcMessageFramer {
public:
    /// Appends raw bytes received from the transport to the internal frame
    /// buffer.
    void append(std::string_view chunk);

    /// Returns the next complete JSON-RPC payload without transport headers.
    /// Returns std::nullopt when the buffer does not yet contain a full frame.
    /// Throws std::runtime_error when a complete header block is present but it
    /// does not contain a valid Content-Length header.
    std::optional<std::string> popMessage();

    /// Wraps a raw JSON-RPC payload in an LSP Content-Length frame suitable for
    /// writing to stdout or another byte stream.
    static std::string encode(std::string_view payload);

private:
    /// Accumulates raw transport bytes until a complete frame can be decoded.
    std::string buffer_;
};

} // namespace rls::lsp
