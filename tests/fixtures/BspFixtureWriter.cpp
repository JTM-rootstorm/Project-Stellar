#include "BspFixture.hpp"

#include <cstdlib>
#include <filesystem>

int main(int argc, char *argv[]) {
  if (argc != 2 && argc != 3) {
    return EXIT_FAILURE;
  }
  stellar::tests::fixtures::write_bsp_fixture(
      std::filesystem::path(argv[1]), argc == 3 ? argv[2] : "playable");
  return EXIT_SUCCESS;
}
