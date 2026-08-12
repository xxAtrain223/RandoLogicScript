#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "rls/lsp/client_connection.h"
#include "rls/lsp/server_composition_root.h"

int main() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    rls::lsp::ServerCompositionRoot server;
    rls::lsp::ClientConnection connection(std::cin, std::cout, std::cerr);
    return connection.run(server);
}