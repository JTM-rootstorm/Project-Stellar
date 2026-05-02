#include <cstdlib>
#include <utility>

#include "stellar/client/Application.hpp"
#include "stellar/client/ApplicationConfig.hpp"

#include <cstdio>

int main(int argc, char *argv[]) {
  auto config = stellar::client::parse_application_config(argc, argv);
  if (!config) {
    std::fprintf(stderr, "Client startup failed: %s\n",
                 config.error().message.c_str());
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
