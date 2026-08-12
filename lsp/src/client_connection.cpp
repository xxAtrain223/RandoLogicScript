#include "rls/lsp/client_connection.h"

#include <exception>
#include <istream>
#include <ostream>

#include "rls/lsp/message_framer.h"
#include "rls/lsp/server_composition_root.h"

namespace rls::lsp {

ClientConnection::ClientConnection(
    std::istream& input, std::ostream& output, std::ostream& log)
    : input_(input), output_(output), log_(log) {}

int ClientConnection::run(ServerCompositionRoot& server) {
    MessageFramer framer;
    char byte = 0;

    try {
        while (!server.shouldExit() && input_.get(byte)) {
            framer.append(std::string_view(&byte, 1));
            while (const auto payload = framer.popMessage()) {
                for (const auto& response : server.handlePayload(*payload)) {
                    const std::string frame = MessageFramer::frame(response);
                    output_.write(frame.data(), static_cast<std::streamsize>(frame.size()));
                    output_.flush();
                }
                if (server.shouldExit()) {
                    break;
                }
            }
        }
    } catch (const std::exception& error) {
        log_ << "rls-language-server: " << error.what() << '\n';
        return 1;
    }

    return server.shouldExit() ? server.exitCode() : 1;
}

} // namespace rls::lsp