#include <array>
#include <cassert>
#include <filesystem>
#include <string_view>

#ifndef STELLAR_PROJECT_SOURCE_DIR
#error "STELLAR_PROJECT_SOURCE_DIR must be defined for generated footstep asset tests"
#endif

namespace {

void generated_footstep_wavs_exist_and_are_non_empty() {
    const std::filesystem::path root = STELLAR_PROJECT_SOURCE_DIR;
    constexpr std::array<std::string_view, 8> surfaces{
        "generic", "concrete", "metal", "wood", "stone", "dirt", "grass", "water"};

    for (std::string_view surface : surfaces) {
        for (int variant = 0; variant < 2; ++variant) {
            const std::filesystem::path path =
                root / "assets/audio/footsteps/generated" /
                ("footstep_" + std::string(surface) + "_" + std::to_string(variant) + ".wav");
            assert(std::filesystem::exists(path));
            assert(std::filesystem::file_size(path) > 44);
        }
    }
}

} // namespace

int main() {
    generated_footstep_wavs_exist_and_are_non_empty();
    return 0;
}
