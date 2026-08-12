#include "rls/lsp/client_connection.h"

#include <exception>
#include <istream>
#include <ostream>
#include <thread>

#include "rls/lsp/message_framer.h"
#include "rls/lsp/server_composition_root.h"

namespace rls::lsp {

ClientConnection::ClientConnection(
    std::istream& input, std::ostream& output, std::ostream& log)
    : input_(input), output_(output), log_(log) {}

int ClientConnection::run(ServerCompositionRoot& server) {
    MessageFramer framer;
    char byte = 0;
    std::jthread writer([&] {
        while (const auto payload = server.outbound().waitPop()) {
            const std::string frame = MessageFramer::frame(*payload);
            output_.write(frame.data(), static_cast<std::streamsize>(frame.size()));
            output_.flush();
        }
    });

    try {
        while (!server.shouldExit() && input_.get(byte)) {
            framer.append(std::string_view(&byte, 1));
            while (const auto payload = framer.popMessage()) {
                for (const auto& response : server.handlePayload(*payload)) {
                    server.outbound().push(response);
                }
                if (server.shouldExit()) {
                    break;
                }
            }
        }
    } catch (const std::exception& error) {
        server.outbound().close();
        writer.join();
        log_ << "rls-language-server: " << error.what() << '\n';
        return 1;
    }

    server.outbound().close();
    writer.join();
    return server.shouldExit() ? server.exitCode() : 1;
}

} // namespace rls::lsp