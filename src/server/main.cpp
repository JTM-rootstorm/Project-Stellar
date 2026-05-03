#include "stellar/server/DedicatedServer.hpp"

#include <iostream>

int main(int argc, const char* const argv[]) {
    auto config = stellar::server::DedicatedServer::parse_cli(argc, argv);
    if (!config) {
        std::cerr << "stellar-server: " << config.error().message << '\n';
        return 2;
    }

    auto server = stellar::server::DedicatedServer::create(*config);
    if (!server) {
        std::cerr << "stellar-server: " << server.error().message << '\n';
        return 1;
    }

    if (config->validate_only) {
        const stellar::server::DedicatedServerStatus status = server->status();
        std::cout << "stellar-server: configuration valid for BSP map " << status.map_id
                  << " (BSP30 TrenchBroom target; legacy BSP29 supported)\n";
        return 0;
    }

    const stellar::server::DedicatedServerStatus status = server->status();
    std::cout << "stellar-server: listening on " << status.bound_endpoint << " for map "
              << status.map_id << '\n';
    auto result = server->run();
    if (!result) {
        std::cerr << "stellar-server: " << result.error().message << '\n';
        return 1;
    }
    return 0;
}
