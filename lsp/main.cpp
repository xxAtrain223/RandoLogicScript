#include <iostream>
#include <string>

#include "lsp.h"

int main() {
    rls::lsp::JsonRpcMessageFramer framer;
    rls::lsp::LanguageServer server;

    std::string chunk;
    chunk.resize(4096);

    while (std::cin.good() && !server.shouldExit()) {
        std::cin.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const std::streamsize bytesRead = std::cin.gcount();
        if (bytesRead <= 0) {
            break;
        }

        framer.append(std::string_view(chunk.data(), static_cast<size_t>(bytesRead)));

        while (true) {
            auto payload = framer.popMessage();
            if (!payload.has_value()) {
                break;
            }

            auto outbound = server.handlePayload(*payload);
            for (const auto& message : outbound) {
                std::cout << rls::lsp::JsonRpcMessageFramer::encode(message);
                std::cout.flush();
            }
        }
    }

    return 0;
}
