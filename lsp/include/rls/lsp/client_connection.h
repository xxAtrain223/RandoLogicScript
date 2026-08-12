#pragma once

#include <iosfwd>

namespace rls::lsp {

class ServerCompositionRoot;

class ClientConnection {
public:
    ClientConnection(std::istream& input, std::ostream& output, std::ostream& log);

    int run(ServerCompositionRoot& server);

private:
    std::istream& input_;
    std::ostream& output_;
    std::ostream& log_;
};

} // namespace rls::lsp