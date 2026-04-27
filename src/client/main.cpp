#include <cstdlib>
#include <cstring>
#include <expected>
#include <string>
#include <utility>

#include "stellar/client/Application.hpp"
#include "stellar/client/ApplicationConfig.hpp"

#include <cstdio>

namespace {

std::expected<stellar::client::ApplicationConfig, stellar::platform::Error>
parse_application_config(int argc, char* argv[]) {
    stellar::client::ApplicationConfig config;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--validate-config") == 0) {
            config.validate_only = true;
            continue;
        }

        if (std::strcmp(argv[i], "--asset") == 0) {
            if (i + 1 >= argc) {
                return std::unexpected(stellar::platform::Error("--asset requires a path"));
            }
            config.asset_path = std::string(argv[++i]);
            continue;
        }

        if (std::strcmp(argv[i], "--renderer") == 0 ||
            std::strcmp(argv[i], "--graphics-backend") == 0) {
            if (i + 1 >= argc) {
                return std::unexpected(stellar::platform::Error("--renderer requires a backend"));
            }
            auto backend = stellar::graphics::parse_graphics_backend(argv[++i]);
            if (!backend) {
                return std::unexpected(backend.error());
            }
            config.graphics_backend = *backend;
            continue;
        }

        return std::unexpected(stellar::platform::Error("Unknown client argument"));
    }

    return config;
}

} // namespace

int main(int argc, char* argv[]) {
    auto config = parse_application_config(argc, argv);
    if (!config) {
        std::fprintf(stderr, "Client startup failed: %s\n", config.error().message.c_str());
        return EXIT_FAILURE;
    }

    stellar::client::Application application(std::move(*config));
    if (auto result = application.run(); !result) {
        std::fprintf(stderr, "Client startup failed: %s\n",
                     result.error().message.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
