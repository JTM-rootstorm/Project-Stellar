#include <bit>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "stellar/import/gltf/Loader.hpp"

namespace {

constexpr std::string_view kPngBase64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO7X"
    "5eoAAAAASUVORK5CYII=";

struct BufferFixture {
    std::vector<std::uint8_t> bytes;
    std::size_t positions_offset = 0;
    std::size_t indices_offset = 0;
    std::size_t image_offset = 0;
};

void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void append_f32_le(std::vector<std::uint8_t>& out, float value) {
    append_u32_le(out, std::bit_cast<std::uint32_t>(value));
}

std::string base64_encode(std::span<const std::uint8_t> input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    std::uint32_t accumulator = 0;
    int bits = -6;
    for (std::uint8_t byte : input) {
        accumulator = (accumulator << 8) | byte;
        bits += 8;
        while (bits >= 0) {
            output.push_back(kAlphabet[(accumulator >> bits) & 0x3F]);
            bits -= 6;
        }
    }

    if (bits > -6) {
        output.push_back(kAlphabet[((accumulator << 8) >> (bits + 8)) & 0x3F]);
    }
    while (output.size() % 4 != 0) {
        output.push_back('=');
    }

    return output;
}

std::string data_uri(std::string_view mime_type, std::span<const std::uint8_t> bytes) {
    return "data:" + std::string(mime_type) + ";base64," + base64_encode(bytes);
}

std::vector<std::uint8_t> base64_decode(std::string_view input) {
    static constexpr auto kInvalid = -1;
    std::array<int, 256> table{};
    table.fill(kInvalid);

    for (int c = 'A'; c <= 'Z'; ++c) {
        table[static_cast<std::size_t>(c)] = c - 'A';
    }
    for (int c = 'a'; c <= 'z'; ++c) {
        table[static_cast<std::size_t>(c)] = 26 + (c - 'a');
    }
    for (int c = '0'; c <= '9'; ++c) {
        table[static_cast<std::size_t>(c)] = 52 + (c - '0');
    }
    table[static_cast<std::size_t>('+')] = 62;
    table[static_cast<std::size_t>('/')] = 63;

    std::vector<std::uint8_t> decoded;
    decoded.reserve((input.size() * 3) / 4);

    int value = 0;
    int bits = -8;
    for (unsigned char ch : input) {
        if (ch == '=') {
            break;
        }
        const int digit = table[ch];
        if (digit == kInvalid) {
            continue;
        }
        value = (value << 6) | digit;
        bits += 6;
        if (bits >= 0) {
            decoded.push_back(static_cast<std::uint8_t>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return decoded;
}

void write_binary_file(const std::filesystem::path& path, std::span<const std::uint8_t> content) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(content.data()),
               static_cast<std::streamsize>(content.size()));
}

void write_text_file(const std::filesystem::path& path, std::string_view content) {
    std::ofstream file(path, std::ios::binary);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
}

BufferFixture build_buffer_fixture(std::span<const std::uint8_t> image_bytes) {
    BufferFixture fixture;
    fixture.positions_offset = 0;

    append_f32_le(fixture.bytes, 0.0f);
    append_f32_le(fixture.bytes, 0.0f);
    append_f32_le(fixture.bytes, 0.0f);
    append_f32_le(fixture.bytes, 1.0f);
    append_f32_le(fixture.bytes, 0.0f);
    append_f32_le(fixture.bytes, 0.0f);
    append_f32_le(fixture.bytes, 0.0f);
    append_f32_le(fixture.bytes, 1.0f);
    append_f32_le(fixture.bytes, 0.0f);

    fixture.indices_offset = fixture.bytes.size();
    append_u16_le(fixture.bytes, 0);
    append_u16_le(fixture.bytes, 1);
    append_u16_le(fixture.bytes, 2);

    while (fixture.bytes.size() % 4 != 0) {
        fixture.bytes.push_back(0);
    }

    fixture.image_offset = fixture.bytes.size();
    fixture.bytes.insert(fixture.bytes.end(), image_bytes.begin(), image_bytes.end());
    return fixture;
}

std::string build_scene_json(const std::string& buffer_uri, std::size_t buffer_size,
                             std::size_t image_offset, std::size_t image_size,
                             std::string_view image0_definition,
                             std::string_view external_image_definition,
                             bool include_external_image) {
    std::string json;
    json.reserve(2048);
    json += "{\n";
    json += "  \"asset\": { \"version\": \"2.0\" },\n";
    json += "  \"buffers\": [{ \"byteLength\": " + std::to_string(buffer_size) +
            ", \"uri\": \"" + buffer_uri + "\" }],\n";
    json += "  \"bufferViews\": [\n";
    json += "    { \"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36 },\n";
    json += "    { \"buffer\": 0, \"byteOffset\": " + std::to_string(image_offset) +
            ", \"byteLength\": " + std::to_string(image_size) + " },\n";
    json += "    { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 6 }\n";
    json += "  ],\n";
    json += "  \"accessors\": [\n";
    json += "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
            "\"type\": \"VEC3\", \"max\": [1, 1, 0], \"min\": [0, 0, 0] },\n";
    json += "    { \"bufferView\": 2, \"componentType\": 5123, \"count\": 3, "
            "\"type\": \"SCALAR\" }\n";
    json += "  ],\n";
    json += "  \"images\": [\n";
    json += "    " + std::string(image0_definition) + "\n";
    if (include_external_image) {
        json += ",    " + std::string(external_image_definition) + "\n";
    }
    json += "  ],\n";
    json += "  \"samplers\": [{ \"name\": \"nearestClamp\", \"magFilter\": 9728, "
            "\"minFilter\": 9986, \"wrapS\": 33071, \"wrapT\": 33648 }],\n";
    if (include_external_image) {
        json += "  \"textures\": [{ \"name\": \"externalTexture\", \"source\": 1, "
                "\"sampler\": 0 }, { \"name\": \"embeddedTexture\", \"source\": 0, "
                "\"sampler\": 0 }],\n";
    } else {
        json += "  \"textures\": [{ \"name\": \"embeddedTexture\", \"source\": 0, "
                "\"sampler\": 0 }],\n";
    }
    json += "  \"materials\": [{ \"name\": \"material0\", \"alphaMode\": \"MASK\", "
            "\"alphaCutoff\": 0.25, \"doubleSided\": true, \"pbrMetallicRoughness\": { "
            "\"baseColorFactor\": [0.25, 0.5, 0.75, 0.8], "
            "\"metallicFactor\": 0.6, \"roughnessFactor\": 0.4, "
            "\"baseColorTexture\": { \"index\": 0, \"texCoord\": 1 }, "
            "\"metallicRoughnessTexture\": { \"index\": 0, \"texCoord\": 2 } }, "
            "\"normalTexture\": { \"index\": 0, \"texCoord\": 3 } }],\n";
    json += "  \"meshes\": [{ \"name\": \"mesh0\", \"primitives\": [{ \"attributes\": { "
            "\"POSITION\": 0 }, \"indices\": 1, \"material\": 0, \"mode\": 4 }] }],\n";
    json += "  \"nodes\": [{ \"name\": \"node0\", \"mesh\": 0 }],\n";
    json += "  \"scenes\": [{ \"name\": \"scene0\", \"nodes\": [0] }],\n";
    json += "  \"scene\": 0\n";
    json += "}\n";
    return json;
}

bool check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

bool validate_scene(const stellar::assets::SceneAsset& scene, bool expect_external_image) {
    if (!check(scene.meshes.size() == 1, "expected one mesh")) {
        return false;
    }
    if (!check(scene.materials.size() == 1, "expected one material")) {
        return false;
    }
    if (!check(scene.nodes.size() == 1, "expected one node")) {
        return false;
    }
    if (!check(scene.scenes.size() == 1, "expected one scene")) {
        return false;
    }
    if (!check(scene.default_scene_index.has_value() && *scene.default_scene_index == 0,
               "expected default scene index 0")) {
        return false;
    }
    if (!check(scene.meshes[0].primitives.size() == 1, "expected one mesh primitive")) {
        return false;
    }
    if (!check(scene.meshes[0].primitives[0].vertices.size() == 3, "expected three vertices")) {
        return false;
    }
    if (!check(scene.meshes[0].primitives[0].indices.size() == 3, "expected three indices")) {
        return false;
    }
    if (!check(scene.meshes[0].primitives[0].material_index.has_value() &&
                   *scene.meshes[0].primitives[0].material_index == 0,
               "expected primitive material index 0")) {
        return false;
    }
    if (!check(scene.samplers.size() == 1, "expected one sampler")) {
        return false;
    }
    if (!check(scene.samplers[0].mag_filter == stellar::assets::TextureFilter::kNearest,
               "expected nearest sampler mag filter")) {
        return false;
    }
    if (!check(scene.samplers[0].min_filter ==
                   stellar::assets::TextureFilter::kNearestMipmapLinear,
               "expected mipmapped sampler min filter")) {
        return false;
    }
    if (!check(scene.samplers[0].wrap_s == stellar::assets::TextureWrapMode::kClampToEdge,
               "expected clamp sampler wrap_s")) {
        return false;
    }
    if (!check(scene.samplers[0].wrap_t == stellar::assets::TextureWrapMode::kMirroredRepeat,
               "expected mirrored sampler wrap_t")) {
        return false;
    }
    if (!check(scene.textures.size() == (expect_external_image ? 2u : 1u),
               "unexpected texture count")) {
        return false;
    }
    if (!check(scene.textures[0].image_index.has_value() &&
                   *scene.textures[0].image_index == (expect_external_image ? 1u : 0u),
               "expected texture source image mapping")) {
        return false;
    }
    if (!check(scene.textures[0].sampler_index.has_value() &&
                   *scene.textures[0].sampler_index == 0,
               "expected texture sampler mapping")) {
        return false;
    }
    if (!check(scene.materials[0].base_color_texture.has_value() &&
                   scene.materials[0].base_color_texture->texture_index == 0,
                "expected base color texture index 0")) {
        return false;
    }
    if (!check(scene.materials[0].base_color_texture->texcoord_set == 1,
               "expected base color texcoord set 1")) {
        return false;
    }
    if (!check(scene.materials[0].metallic_roughness_texture.has_value() &&
                   scene.materials[0].metallic_roughness_texture->texcoord_set == 2,
               "expected metallic roughness texcoord set 2")) {
        return false;
    }
    if (!check(scene.materials[0].normal_texture.has_value() &&
                    scene.materials[0].normal_texture->texcoord_set == 3,
                "expected normal texcoord set 3")) {
        return false;
    }
    if (!check(scene.materials[0].base_color_factor[0] > 0.249f &&
                   scene.materials[0].base_color_factor[3] < 0.801f,
               "expected base color factor import")) {
        return false;
    }
    if (!check(scene.materials[0].metallic_factor > 0.599f &&
                   scene.materials[0].roughness_factor < 0.401f,
               "expected metallic and roughness factor import")) {
        return false;
    }
    if (!check(scene.materials[0].alpha_mode == stellar::assets::AlphaMode::kMask,
               "expected alpha mask mode")) {
        return false;
    }
    if (!check(scene.materials[0].alpha_cutoff > 0.249f &&
                   scene.materials[0].alpha_cutoff < 0.251f,
               "expected alpha cutoff 0.25")) {
        return false;
    }
    if (!check(scene.materials[0].double_sided, "expected double-sided material")) {
        return false;
    }
    if (!check(scene.nodes[0].mesh_instances.size() == 1, "expected node mesh instance")) {
        return false;
    }
    if (!check(scene.nodes[0].mesh_instances[0].mesh_index == 0,
               "expected node mesh index 0")) {
        return false;
    }
    if (!check(scene.scenes[0].root_nodes.size() == 1 && scene.scenes[0].root_nodes[0] == 0,
               "expected root node 0")) {
        return false;
    }
    if (!check(scene.images.size() == (expect_external_image ? 2u : 1u),
               "unexpected image count")) {
        return false;
    }
    if (!check(scene.images[0].width == 1 && scene.images[0].height == 1,
               "expected 1x1 embedded image")) {
        return false;
    }
    if (expect_external_image) {
        if (!check(scene.images[1].width == 1 && scene.images[1].height == 1,
                   "expected 1x1 external image")) {
            return false;
        }
        if (!check(scene.images[1].source_uri.find("external.png") != std::string::npos,
                   "expected external image source uri")) {
            return false;
        }
    } else {
        if (!check(scene.images[0].source_uri.find("data:") == 0,
                   "expected data URI image source")) {
            return false;
        }
    }
    return true;
}

bool run_buffer_view_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "buffer_view";
    std::filesystem::create_directories(work_dir);

    const auto image_bytes = base64_decode(kPngBase64);
    const auto fixture = build_buffer_fixture(std::span<const std::uint8_t>(image_bytes.data(),
                                                                            image_bytes.size()));

    const auto external_image_path = work_dir / "external.png";
    write_binary_file(external_image_path,
                      std::span<const std::uint8_t>(image_bytes.data(), image_bytes.size()));

    const std::string buffer_uri = data_uri("application/octet-stream", fixture.bytes);
    const std::string image0_definition =
        "{ \"name\": \"embedded\", \"bufferView\": 1, \"mimeType\": \"image/png\" }";
    const std::string external_image_definition =
        "{ \"name\": \"external\", \"uri\": \"external.png\" }";

    const auto gltf_path = work_dir / "scene.gltf";
    write_text_file(gltf_path,
                    build_scene_json(buffer_uri, fixture.bytes.size(), fixture.image_offset,
                                     image_bytes.size(), image0_definition,
                                     external_image_definition, true));

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    return validate_scene(*scene, true);
}

bool run_data_uri_fixture(const std::filesystem::path& root) {
    const auto image_bytes = base64_decode(kPngBase64);
    const auto fixture = build_buffer_fixture(std::span<const std::uint8_t>(image_bytes.data(),
                                                                            image_bytes.size()));
    const auto work_dir = root / "data_uri";
    std::filesystem::create_directories(work_dir);

    const std::string buffer_uri = data_uri("application/octet-stream", fixture.bytes);
    const std::string image_uri = "data:image/png;base64," + std::string(kPngBase64);
    const std::string image0_definition =
        "{ \"name\": \"embedded\", \"uri\": \"" + image_uri + "\" }";

    const auto gltf_path = work_dir / "scene.gltf";
    write_text_file(gltf_path,
                    build_scene_json(buffer_uri, fixture.bytes.size(), fixture.image_offset,
                                     image_bytes.size(), image0_definition, {}, false));

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    return validate_scene(*scene, false);
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "stellar_import_gltf_regression";
    std::filesystem::create_directories(root);

    if (!run_buffer_view_fixture(root)) {
        return 1;
    }
    if (!run_data_uri_fixture(root)) {
        return 1;
    }

    return 0;
}
