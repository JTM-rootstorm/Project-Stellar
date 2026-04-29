#include <cstdlib>
#include <cstring>
#include <expected>
#include <charconv>
#include <string>
#include <system_error>
#include <utility>

#include "stellar/client/Application.hpp"
#include "stellar/client/ApplicationConfig.hpp"

#include <cstdio>

namespace {

std::expected<std::size_t, stellar::platform::Error> parse_size_arg(const char* value,
                                                                    const char* option) {
    std::size_t parsed = 0;
    const char* end = value + std::strlen(value);
    const auto result = std::from_chars(value, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
    return std::unexpected(
        stellar::platform::Error(std::string(option) + " requires an integer"));
    }
    return parsed;
}

std::expected<bool, stellar::platform::Error> parse_bool_arg(const char* value,
                                                            const char* option) {
    if (std::strcmp(value, "true") == 0 || std::strcmp(value, "1") == 0 ||
        std::strcmp(value, "yes") == 0) {
        return true;
    }
    if (std::strcmp(value, "false") == 0 || std::strcmp(value, "0") == 0 ||
        std::strcmp(value, "no") == 0) {
        return false;
    }
    return std::unexpected(
        stellar::platform::Error(std::string(option) + " requires true or false"));
}

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

        if (std::strcmp(argv[i], "--animation-index") == 0) {
            if (i + 1 >= argc) {
                return std::unexpected(
                    stellar::platform::Error("--animation-index requires a value"));
            }
            auto index = parse_size_arg(argv[++i], "--animation-index");
            if (!index) {
                return std::unexpected(index.error());
            }
            config.animation_index = *index;
            continue;
        }

        if (std::strcmp(argv[i], "--animation-name") == 0) {
            if (i + 1 >= argc) {
                return std::unexpected(
                    stellar::platform::Error("--animation-name requires a value"));
            }
            config.animation_name = std::string(argv[++i]);
            continue;
        }

        if (std::strcmp(argv[i], "--animation-loop") == 0) {
            if (i + 1 >= argc) {
                return std::unexpected(
                    stellar::platform::Error("--animation-loop requires a value"));
            }
            auto loop = parse_bool_arg(argv[++i], "--animation-loop");
            if (!loop) {
                return std::unexpected(loop.error());
            }
            config.animation_loop = *loop;
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
