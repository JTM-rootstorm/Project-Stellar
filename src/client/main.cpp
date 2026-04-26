#include <cstdlib>

#include "stellar/client/Application.hpp"

#include <cstdio>

int main(int /*argc*/, char* /*argv*/[]) {
    stellar::client::Application application;
    if (auto result = application.run(); !result) {
        std::fprintf(stderr, "Client startup failed: %s\n",
                     result.error().message.c_str());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
