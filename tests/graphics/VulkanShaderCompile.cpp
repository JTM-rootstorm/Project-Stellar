#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace {

[[nodiscard]] std::uint32_t read_magic(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return magic;
}

void verify_spirv_file(const std::filesystem::path& path) {
    assert(std::filesystem::exists(path));
    assert(std::filesystem::file_size(path) >= sizeof(std::uint32_t));
    assert(read_magic(path) == 0x07230203U);
}

} // namespace

int main() {
    verify_spirv_file(STELLAR_VULKAN_VERTEX_SPV);
    verify_spirv_file(STELLAR_VULKAN_FRAGMENT_SPV);
    return 0;
}
