#include "BspFixture.hpp"

#include <cstdlib>
#include <filesystem>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  stellar::tests::fixtures::write_bsp_fixture(std::filesystem::path(argv[1]));
  return EXIT_SUCCESS;
}
