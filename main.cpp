#include "client/client.h"
#include <iostream>
#include <string>
#include <cstring>

void printUsage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << "                          - Local mode (default port "
              << NProtocol::DEFAULT_PORT << ")\n"
              << "  " << prog << " --port <port>            - Local mode on specified port\n"
              << "  " << prog << " --open                   - Local mode, open for remote players\n"
              << "  " << prog << " --open --port <port>     - Local mode, open on specified port\n"
              << "  " << prog << " --connect <host> [port]  - Connect to remote server\n"
              << "\nControls:\n"
              << "  Left Mouse  - Draw\n"
              << "  Right Mouse - Color picker (eyedropper)\n"
              << "  +/-         - Brush size\n"
              << "  Ctrl+C      - Clear canvas\n"
              << "  Ctrl+S      - Export to BMP\n"
              << "  Escape      - Quit\n"
              << "  Click palette colors at bottom panel\n";
}

int main(int argc, char* argv[]) {
    uint16_t port = NProtocol::DEFAULT_PORT;
    bool open_port = false;
    bool remote = false;
    std::string host;

    // Разбор аргументов
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (strcmp(argv[i], "--open") == 0) {
            open_port = true;
        }
        else if (strcmp(argv[i], "--connect") == 0 && i + 1 < argc) {
            remote = true;
            host = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
        }
    }

    DrawClient client;

    if (remote) {
        std::cout << "[Main] Connecting to " << host << ":" << port << "...\n";
        if (!client.connectRemote(host, port)) {
            std::cerr << "[Main] Failed to connect!\n";
            return 1;
        }
    } else {
        std::cout << "[Main] Starting local server on port " << port;
        if (open_port) std::cout << " (open for remote)";
        std::cout << "...\n";

        if (!client.startLocal(port, open_port)) {
            std::cerr << "[Main] Failed to start!\n";
            return 1;
        }
    }

    client.run();

    return 0;
}
