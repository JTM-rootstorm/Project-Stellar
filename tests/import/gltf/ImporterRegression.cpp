#include <bit>
#include <array>
#include <cmath>
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

void append_vec2(std::vector<std::uint8_t>& out, float x, float y) {
    append_f32_le(out, x);
    append_f32_le(out, y);
}

void append_vec3(std::vector<std::uint8_t>& out, float x, float y, float z) {
    append_f32_le(out, x);
    append_f32_le(out, y);
    append_f32_le(out, z);
}

void append_vec4(std::vector<std::uint8_t>& out, float x, float y, float z, float w) {
    append_f32_le(out, x);
    append_f32_le(out, y);
    append_f32_le(out, z);
    append_f32_le(out, w);
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

void write_glb_file(const std::filesystem::path& path, std::string_view json,
                    std::span<const std::uint8_t> bin = {}) {
    std::vector<std::uint8_t> json_chunk(json.begin(), json.end());
    while (json_chunk.size() % 4 != 0) {
        json_chunk.push_back(0x20);
    }

    std::vector<std::uint8_t> bin_chunk(bin.begin(), bin.end());
    while (bin_chunk.size() % 4 != 0) {
        bin_chunk.push_back(0);
    }

    const std::uint32_t total_length = static_cast<std::uint32_t>(
        12 + 8 + json_chunk.size() + (bin_chunk.empty() ? 0 : 8 + bin_chunk.size()));

    std::vector<std::uint8_t> glb;
    glb.reserve(total_length);
    append_u32_le(glb, 0x46546C67);
    append_u32_le(glb, 2);
    append_u32_le(glb, total_length);
    append_u32_le(glb, static_cast<std::uint32_t>(json_chunk.size()));
    append_u32_le(glb, 0x4E4F534A);
    glb.insert(glb.end(), json_chunk.begin(), json_chunk.end());
    if (!bin_chunk.empty()) {
        append_u32_le(glb, static_cast<std::uint32_t>(bin_chunk.size()));
        append_u32_le(glb, 0x004E4942);
        glb.insert(glb.end(), bin_chunk.begin(), bin_chunk.end());
    }

    write_binary_file(path, glb);
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
    const int data_texture_index = include_external_image ? 1 : 0;
    json += "  \"materials\": [{ \"name\": \"material0\", \"alphaMode\": \"MASK\", "
            "\"alphaCutoff\": 0.25, \"doubleSided\": true, "
            "\"emissiveFactor\": [0.1, 0.2, 0.3], \"pbrMetallicRoughness\": { "
            "\"baseColorFactor\": [0.25, 0.5, 0.75, 0.8], "
            "\"metallicFactor\": 0.6, \"roughnessFactor\": 0.4, "
            "\"baseColorTexture\": { \"index\": 0, \"texCoord\": 1 }, "
            "\"metallicRoughnessTexture\": { \"index\": ";
    json += std::to_string(data_texture_index);
    json += ", \"texCoord\": 2 } }, \"normalTexture\": { \"index\": ";
    json += std::to_string(data_texture_index);
    json += ", \"texCoord\": 3, \"scale\": 0.65 }, \"occlusionTexture\": { \"index\": ";
    json += std::to_string(data_texture_index);
    json += ", \"texCoord\": 4, \"strength\": 0.7 }, "
            "\"emissiveTexture\": { \"index\": 0, \"texCoord\": 5 } }],\n";
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

bool check_near(float actual, float expected, std::string_view message) {
    return check(std::fabs(actual - expected) < 0.0001f, message);
}

bool check_vec2(const std::array<float, 2>& actual, const std::array<float, 2>& expected,
                std::string_view message) {
    return check_near(actual[0], expected[0], message) &&
           check_near(actual[1], expected[1], message);
}

bool check_vec3(const std::array<float, 3>& actual, const std::array<float, 3>& expected,
                std::string_view message) {
    return check_near(actual[0], expected[0], message) &&
           check_near(actual[1], expected[1], message) &&
           check_near(actual[2], expected[2], message);
}

bool check_vec4(const std::array<float, 4>& actual, const std::array<float, 4>& expected,
                std::string_view message) {
    return check_near(actual[0], expected[0], message) &&
           check_near(actual[1], expected[1], message) &&
           check_near(actual[2], expected[2], message) &&
           check_near(actual[3], expected[3], message);
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
    if (!check(!scene.meshes[0].primitives[0].has_tangents,
               "expected no tangents when TANGENT is absent")) {
        return false;
    }
    if (!check_vec3(scene.meshes[0].primitives[0].bounds_min, {0.0f, 0.0f, 0.0f},
                    "expected imported primitive bounds minimum")) {
        return false;
    }
    if (!check_vec3(scene.meshes[0].primitives[0].bounds_max, {1.0f, 1.0f, 0.0f},
                    "expected imported primitive bounds maximum")) {
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
    if (expect_external_image) {
        if (!check(scene.textures[1].image_index.has_value() &&
                        *scene.textures[1].image_index == 0,
                    "expected second texture to map back to embedded image 0")) {
            return false;
        }
        if (!check(scene.textures[0].color_space == stellar::assets::TextureColorSpace::kSrgb,
                   "expected base/emissive texture to use sRGB color space")) {
            return false;
        }
        if (!check(scene.textures[1].color_space == stellar::assets::TextureColorSpace::kLinear,
                   "expected data texture to use linear color space")) {
            return false;
        }
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
    if (!check_near(scene.materials[0].normal_texture->scale, 0.65f,
                    "expected normal texture scale")) {
        return false;
    }
    if (!check(scene.materials[0].occlusion_texture.has_value() &&
                   scene.materials[0].occlusion_texture->texcoord_set == 4,
               "expected occlusion texture texcoord set 4")) {
        return false;
    }
    if (!check_near(scene.materials[0].occlusion_strength, 0.7f,
                    "expected occlusion strength")) {
        return false;
    }
    if (!check(scene.materials[0].emissive_texture.has_value() &&
                   scene.materials[0].emissive_texture->texcoord_set == 5,
               "expected emissive texture texcoord set 5")) {
        return false;
    }
    if (!check_vec3(scene.materials[0].emissive_factor, {0.1f, 0.2f, 0.3f},
                    "expected emissive factor")) {
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

bool run_material_variants_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "material_variants";
    std::filesystem::create_directories(work_dir);

    const auto gltf_path = work_dir / "materials.gltf";
    write_text_file(gltf_path,
                    "{\n"
                    "  \"asset\": { \"version\": \"2.0\" },\n"
                    "  \"materials\": [\n"
                    "    { \"name\": \"opaque\" },\n"
                    "    { \"name\": \"mask\", \"alphaMode\": \"MASK\", "
                    "\"alphaCutoff\": 0.35 },\n"
                    "    { \"name\": \"blend\", \"alphaMode\": \"BLEND\", "
                    "\"pbrMetallicRoughness\": { \"baseColorFactor\": "
                    "[1.0, 0.5, 0.25, 0.4] } },\n"
                    "    { \"name\": \"double_sided\", \"doubleSided\": true }\n"
                    "  ]\n"
                    "}\n");

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    if (!check(scene->materials.size() == 4, "expected four representative materials")) {
        return false;
    }
    if (!check(scene->materials[1].alpha_mode == stellar::assets::AlphaMode::kMask,
               "expected alpha-mask material")) {
        return false;
    }
    if (!check(scene->materials[1].alpha_cutoff > 0.349f &&
                   scene->materials[1].alpha_cutoff < 0.351f,
               "expected alpha-mask cutoff")) {
        return false;
    }
    if (!check(scene->materials[2].alpha_mode == stellar::assets::AlphaMode::kBlend,
               "expected alpha-blend material")) {
        return false;
    }
    if (!check(scene->materials[2].base_color_factor[3] > 0.399f &&
                   scene->materials[2].base_color_factor[3] < 0.401f,
               "expected alpha-blend base color alpha")) {
        return false;
    }
    if (!check(scene->materials[3].double_sided, "expected double-sided material")) {
        return false;
    }

    return true;
}

bool run_tangent_and_bounds_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "tangent_bounds";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bytes;
    const std::size_t positions_offset = bytes.size();
    append_vec3(bytes, -2.0f, 0.0f, 1.0f);
    append_vec3(bytes, 3.0f, 4.0f, -1.0f);
    append_vec3(bytes, 0.0f, -5.0f, 2.0f);

    const std::size_t normals_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);

    const std::size_t uv0_offset = bytes.size();
    append_vec2(bytes, 0.0f, 0.0f);
    append_vec2(bytes, 1.0f, 0.0f);
    append_vec2(bytes, 0.0f, 1.0f);

    const std::size_t tangents_offset = bytes.size();
    append_vec4(bytes, 1.0f, 0.0f, 0.0f, 1.0f);
    append_vec4(bytes, 0.0f, 1.0f, 0.0f, -1.0f);
    append_vec4(bytes, 0.0f, 0.0f, 1.0f, 1.0f);

    const std::size_t indices_offset = bytes.size();
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 1);
    append_u16_le(bytes, 2);

    const auto gltf_path = work_dir / "tangent_bounds.gltf";
    const std::string buffer_uri = data_uri("application/octet-stream", bytes);
    write_text_file(gltf_path,
                    "{\n"
                    "  \"asset\": { \"version\": \"2.0\" },\n"
                    "  \"buffers\": [{ \"byteLength\": " +
                        std::to_string(bytes.size()) + ", \"uri\": \"" + buffer_uri +
                        "\" }],\n"
                    "  \"bufferViews\": [\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(positions_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(normals_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(uv0_offset) + ", \"byteLength\": 24 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(tangents_offset) + ", \"byteLength\": 48 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(indices_offset) + ", \"byteLength\": 6 }\n"
                    "  ],\n"
                    "  \"accessors\": [\n"
                    "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\", \"min\": [-2, -5, -1], \"max\": [3, 4, 2] },\n"
                    "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC2\" },\n"
                    "    { \"bufferView\": 3, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC4\" },\n"
                    "    { \"bufferView\": 4, \"componentType\": 5123, \"count\": 3, "
                    "\"type\": \"SCALAR\" }\n"
                    "  ],\n"
                    "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { "
                    "\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2, \"TANGENT\": 3 }, "
                    "\"indices\": 4, \"mode\": 4 }] }],\n"
                    "  \"nodes\": [{ \"mesh\": 0 }],\n"
                    "  \"scenes\": [{ \"nodes\": [0] }],\n"
                    "  \"scene\": 0\n"
                    "}\n");

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    const auto& primitive = scene->meshes[0].primitives[0];
    return check(primitive.has_tangents, "expected imported tangents") &&
           check_vec4(primitive.vertices[0].tangent, {1.0f, 0.0f, 0.0f, 1.0f},
                      "expected first tangent") &&
           check_vec4(primitive.vertices[1].tangent, {0.0f, 1.0f, 0.0f, -1.0f},
                      "expected second tangent handedness") &&
           check_vec4(primitive.vertices[2].tangent, {0.0f, 0.0f, 1.0f, 1.0f},
                      "expected third tangent") &&
           check_vec3(primitive.bounds_min, {-2.0f, -5.0f, -1.0f},
                      "expected non-default bounds minimum") &&
           check_vec3(primitive.bounds_max, {3.0f, 4.0f, 2.0f},
                      "expected non-default bounds maximum");
}

bool expect_load_failure(const std::filesystem::path& path, std::string_view json,
                         std::string_view fixture_name) {
    write_text_file(path, json);
    auto scene = stellar::import::gltf::load_scene(path.string());
    if (scene) {
        std::cerr << fixture_name << " unexpectedly loaded\n";
        return false;
    }
    return true;
}

bool run_attribute_validation_failures_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "attribute_failures";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bytes;
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 1.0f, 0.0f, 0.0f);
    append_vec3(bytes, 0.0f, 1.0f, 0.0f);
    const std::string buffer_uri = data_uri("application/octet-stream", bytes);
    const std::string prefix =
        "{\n"
        "  \"asset\": { \"version\": \"2.0\" },\n"
        "  \"buffers\": [{ \"byteLength\": " +
        std::to_string(bytes.size()) + ", \"uri\": \"" + buffer_uri + "\" }],\n"
        "  \"bufferViews\": [{ \"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36 }],\n"
        "  \"accessors\": [";
    const std::string suffix =
        "],\n"
        "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { ";

    const std::string scene_suffix =
        " }, \"mode\": 4 }] }],\n"
        "  \"nodes\": [{ \"mesh\": 0 }],\n"
        "  \"scenes\": [{ \"nodes\": [0] }],\n"
        "  \"scene\": 0\n"
        "}\n";

    const auto missing_position = prefix +
                                  "{ \"bufferView\": 0, \"componentType\": 5126, "
                                  "\"count\": 3, \"type\": \"VEC3\" }" +
                                  suffix + "\"NORMAL\": 0" + scene_suffix;
    const auto invalid_position = prefix +
                                  "{ \"bufferView\": 0, \"componentType\": 5126, "
                                  "\"count\": 3, \"type\": \"VEC2\" }" +
                                  suffix + "\"POSITION\": 0" + scene_suffix;
    const auto invalid_tangent = prefix +
                                 "{ \"bufferView\": 0, \"componentType\": 5126, "
                                 "\"count\": 3, \"type\": \"VEC3\" }," 
                                 "{ \"bufferView\": 0, \"componentType\": 5126, "
                                 "\"count\": 3, \"type\": \"VEC3\" }" +
                                 suffix + "\"POSITION\": 0, \"TANGENT\": 1" + scene_suffix;

    return expect_load_failure(work_dir / "missing_position.gltf", missing_position,
                               "missing POSITION") &&
           expect_load_failure(work_dir / "invalid_position.gltf", invalid_position,
                               "invalid POSITION") &&
           expect_load_failure(work_dir / "invalid_tangent.gltf", invalid_tangent,
                               "invalid TANGENT");
}

bool run_uv1_vertex_color_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "uv1_vertex_color";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bytes;
    const std::size_t positions_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 1.0f, 0.0f, 0.0f);
    append_vec3(bytes, 0.0f, 1.0f, 0.0f);
    const std::size_t uv1_offset = bytes.size();
    append_vec2(bytes, 0.25f, 0.75f);
    append_vec2(bytes, 0.5f, 0.625f);
    append_vec2(bytes, 0.875f, 0.125f);
    const std::size_t color_offset = bytes.size();
    append_vec4(bytes, 1.0f, 0.25f, 0.0f, 0.75f);
    append_vec4(bytes, 0.0f, 1.0f, 0.25f, 0.5f);
    append_vec4(bytes, 0.25f, 0.0f, 1.0f, 0.25f);

    const auto gltf_path = work_dir / "uv1_color.gltf";
    const std::string buffer_uri = data_uri("application/octet-stream", bytes);
    write_text_file(gltf_path,
                    "{\n"
                    "  \"asset\": { \"version\": \"2.0\" },\n"
                    "  \"buffers\": [{ \"byteLength\": " +
                        std::to_string(bytes.size()) + ", \"uri\": \"" + buffer_uri +
                        "\" }],\n"
                    "  \"bufferViews\": [\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(positions_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(uv1_offset) + ", \"byteLength\": 24 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(color_offset) + ", \"byteLength\": 48 }\n"
                    "  ],\n"
                    "  \"accessors\": [\n"
                    "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC2\" },\n"
                    "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC4\" }\n"
                    "  ],\n"
                    "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { "
                    "\"POSITION\": 0, \"TEXCOORD_1\": 1, \"COLOR_0\": 2 }, "
                    "\"mode\": 4, \"extras\": { \"ignored\": true } }] }],\n"
                    "  \"nodes\": [{ \"mesh\": 0 }],\n"
                    "  \"scenes\": [{ \"nodes\": [0] }],\n"
                    "  \"scene\": 0,\n"
                    "  \"extras\": { \"authoringTool\": \"phase2a\" }\n"
                    "}\n");

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    const auto& primitive = scene->meshes[0].primitives[0];
    return check(primitive.vertices.size() == 3, "expected three vertices with UV1/color") &&
           check(primitive.has_colors, "expected imported vertex colors") &&
           check_vec2(primitive.vertices[0].uv1, {0.25f, 0.75f}, "expected first UV1") &&
           check_vec2(primitive.vertices[1].uv1, {0.5f, 0.625f}, "expected second UV1") &&
           check_vec2(primitive.vertices[2].uv1, {0.875f, 0.125f}, "expected third UV1") &&
           check_vec4(primitive.vertices[0].color, {1.0f, 0.25f, 0.0f, 0.75f},
                      "expected first vertex color") &&
           check_vec4(primitive.vertices[1].color, {0.0f, 1.0f, 0.25f, 0.5f},
                      "expected second vertex color") &&
           check_vec4(primitive.vertices[2].color, {0.25f, 0.0f, 1.0f, 0.25f},
                      "expected third vertex color") &&
           check(!primitive.has_tangents, "expected no tangents from unsupported attributes") &&
           check_vec3(primitive.bounds_max, {1.0f, 1.0f, 0.0f},
                       "expected core mesh import with UV1/color attributes");
}

bool run_glb_bin_chunk_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "glb_bin_chunk";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bin;
    const std::size_t positions_offset = bin.size();
    append_vec3(bin, 0.0f, 0.0f, 0.0f);
    append_vec3(bin, 2.0f, 0.0f, 0.0f);
    append_vec3(bin, 0.0f, 3.0f, 0.0f);
    const std::size_t indices_offset = bin.size();
    append_u16_le(bin, 0);
    append_u16_le(bin, 1);
    append_u16_le(bin, 2);

    const std::string json =
        "{\n"
        "  \"asset\": { \"version\": \"2.0\" },\n"
        "  \"buffers\": [{ \"byteLength\": " +
        std::to_string(bin.size()) +
        " }],\n"
        "  \"bufferViews\": [\n"
        "    { \"buffer\": 0, \"byteOffset\": " +
        std::to_string(positions_offset) +
        ", \"byteLength\": 36 },\n"
        "    { \"buffer\": 0, \"byteOffset\": " +
        std::to_string(indices_offset) +
        ", \"byteLength\": 6 }\n"
        "  ],\n"
        "  \"accessors\": [\n"
        "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
        "\"type\": \"VEC3\", \"min\": [0, 0, 0], \"max\": [2, 3, 0] },\n"
        "    { \"bufferView\": 1, \"componentType\": 5123, \"count\": 3, "
        "\"type\": \"SCALAR\" }\n"
        "  ],\n"
        "  \"materials\": [{ \"name\": \"glb-material\", \"pbrMetallicRoughness\": { "
        "\"baseColorFactor\": [0.4, 0.5, 0.6, 1.0] } }],\n"
        "  \"meshes\": [{ \"name\": \"glb-mesh\", \"primitives\": [{ \"attributes\": { "
        "\"POSITION\": 0 }, \"indices\": 1, \"material\": 0, \"mode\": 4 }] }],\n"
        "  \"nodes\": [{ \"name\": \"glb-node\", \"mesh\": 0 }],\n"
        "  \"scenes\": [{ \"name\": \"glb-scene\", \"nodes\": [0] }],\n"
        "  \"scene\": 0\n"
        "}\n";

    const auto glb_path = work_dir / "triangle_bin.glb";
    write_glb_file(glb_path, json, bin);

    auto scene = stellar::import::gltf::load_scene(glb_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    const auto& primitive = scene->meshes[0].primitives[0];
    return check(scene->meshes.size() == 1, "expected one GLB mesh") &&
           check(scene->materials.size() == 1, "expected one GLB material") &&
           check(primitive.vertices.size() == 3, "expected three GLB vertices") &&
           check(primitive.indices.size() == 3, "expected three GLB indices") &&
           check(primitive.indices[0] == 0 && primitive.indices[1] == 1 &&
                     primitive.indices[2] == 2,
                 "expected GLB index data") &&
           check_vec3(primitive.vertices[1].position, {2.0f, 0.0f, 0.0f},
                      "expected GLB vertex data") &&
           check_vec3(primitive.bounds_max, {2.0f, 3.0f, 0.0f},
                      "expected GLB bounds") &&
           check(primitive.material_index.has_value() && *primitive.material_index == 0,
                 "expected GLB material index") &&
           check(scene->default_scene_index.has_value() && *scene->default_scene_index == 0,
                 "expected GLB default scene");
}

bool run_generated_tangent_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "generated_tangent";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bytes;
    const std::size_t positions_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 1.0f, 0.0f, 0.0f);
    append_vec3(bytes, 0.0f, 1.0f, 0.0f);
    const std::size_t normals_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    const std::size_t uv0_offset = bytes.size();
    append_vec2(bytes, 0.0f, 0.0f);
    append_vec2(bytes, 1.0f, 0.0f);
    append_vec2(bytes, 0.0f, 1.0f);
    const std::size_t indices_offset = bytes.size();
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 1);
    append_u16_le(bytes, 2);
    const auto image_bytes = base64_decode(kPngBase64);
    while (bytes.size() % 4 != 0) {
        bytes.push_back(0);
    }
    const std::size_t image_offset = bytes.size();
    bytes.insert(bytes.end(), image_bytes.begin(), image_bytes.end());

    const auto gltf_path = work_dir / "generated_tangent.gltf";
    const std::string buffer_uri = data_uri("application/octet-stream", bytes);
    write_text_file(gltf_path,
                    "{\n"
                    "  \"asset\": { \"version\": \"2.0\" },\n"
                    "  \"buffers\": [{ \"byteLength\": " +
                        std::to_string(bytes.size()) + ", \"uri\": \"" + buffer_uri +
                        "\" }],\n"
                    "  \"bufferViews\": [\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(positions_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(normals_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(uv0_offset) + ", \"byteLength\": 24 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(indices_offset) + ", \"byteLength\": 6 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(image_offset) + ", \"byteLength\": " +
                        std::to_string(image_bytes.size()) + " }\n"
                    "  ],\n"
                    "  \"accessors\": [\n"
                    "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC2\" },\n"
                    "    { \"bufferView\": 3, \"componentType\": 5123, \"count\": 3, "
                    "\"type\": \"SCALAR\" }\n"
                    "  ],\n"
                    "  \"images\": [{ \"bufferView\": 4, \"mimeType\": \"image/png\" }],\n"
                    "  \"textures\": [{ \"source\": 0 }],\n"
                    "  \"materials\": [{ \"normalTexture\": { \"index\": 0 } }],\n"
                    "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { "
                    "\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2 }, "
                    "\"indices\": 3, \"material\": 0, \"mode\": 4 }] }],\n"
                    "  \"nodes\": [{ \"mesh\": 0 }],\n"
                    "  \"scenes\": [{ \"nodes\": [0] }],\n"
                    "  \"scene\": 0\n"
                    "}\n");

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    const auto& primitive = scene->meshes[0].primitives[0];
    return check(primitive.has_tangents, "expected generated tangents") &&
           check_vec4(primitive.vertices[0].tangent, {1.0f, 0.0f, 0.0f, 1.0f},
                      "expected generated tangent for first vertex") &&
           check_vec4(primitive.vertices[1].tangent, {1.0f, 0.0f, 0.0f, 1.0f},
                      "expected generated tangent for second vertex") &&
           check_vec4(primitive.vertices[2].tangent, {1.0f, 0.0f, 0.0f, 1.0f},
                      "expected generated tangent for third vertex");
}

bool run_degenerate_tangent_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "degenerate_tangent";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bytes;
    const std::size_t positions_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 1.0f, 0.0f, 0.0f);
    append_vec3(bytes, 0.0f, 1.0f, 0.0f);
    const std::size_t normals_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    append_vec3(bytes, 0.0f, 0.0f, 1.0f);
    const std::size_t uv0_offset = bytes.size();
    append_vec2(bytes, 0.0f, 0.0f);
    append_vec2(bytes, 0.0f, 0.0f);
    append_vec2(bytes, 0.0f, 0.0f);
    const auto image_bytes = base64_decode(kPngBase64);
    while (bytes.size() % 4 != 0) {
        bytes.push_back(0);
    }
    const std::size_t image_offset = bytes.size();
    bytes.insert(bytes.end(), image_bytes.begin(), image_bytes.end());

    const auto gltf_path = work_dir / "degenerate_tangent.gltf";
    const std::string buffer_uri = data_uri("application/octet-stream", bytes);
    write_text_file(gltf_path,
                    "{\n"
                    "  \"asset\": { \"version\": \"2.0\" },\n"
                    "  \"buffers\": [{ \"byteLength\": " +
                        std::to_string(bytes.size()) + ", \"uri\": \"" + buffer_uri +
                        "\" }],\n"
                    "  \"bufferViews\": [\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(positions_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(normals_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(uv0_offset) + ", \"byteLength\": 24 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(image_offset) + ", \"byteLength\": " +
                        std::to_string(image_bytes.size()) + " }\n"
                    "  ],\n"
                    "  \"accessors\": [\n"
                    "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 1, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC2\" }\n"
                    "  ],\n"
                    "  \"images\": [{ \"bufferView\": 3, \"mimeType\": \"image/png\" }],\n"
                    "  \"textures\": [{ \"source\": 0 }],\n"
                    "  \"materials\": [{ \"normalTexture\": { \"index\": 0 } }],\n"
                    "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { "
                    "\"POSITION\": 0, \"NORMAL\": 1, \"TEXCOORD_0\": 2 }, "
                    "\"material\": 0, \"mode\": 4 }] }],\n"
                    "  \"nodes\": [{ \"mesh\": 0 }],\n"
                    "  \"scenes\": [{ \"nodes\": [0] }],\n"
                    "  \"scene\": 0\n"
                    "}\n");

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    return check(!scene->meshes[0].primitives[0].has_tangents,
                 "expected degenerate UVs to disable generated tangents");
}

bool run_skin_animation_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "skin_animation";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bytes;
    const std::size_t positions_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 1.0f, 0.0f, 0.0f);
    append_vec3(bytes, 0.0f, 1.0f, 0.0f);

    const std::size_t joints_offset = bytes.size();
    for (int i = 0; i < 3; ++i) {
        append_u16_le(bytes, 0);
        append_u16_le(bytes, 1);
        append_u16_le(bytes, 0);
        append_u16_le(bytes, 0);
    }

    const std::size_t weights_offset = bytes.size();
    append_vec4(bytes, 0.25f, 0.75f, 0.0f, 0.0f);
    append_vec4(bytes, 2.0f, 2.0f, 0.0f, 0.0f);
    append_vec4(bytes, 1.0f, 0.0f, 0.0f, 0.0f);

    const std::size_t inverse_bind_offset = bytes.size();
    for (int matrix = 0; matrix < 2; ++matrix) {
        append_vec4(bytes, matrix == 0 ? 1.0f : 2.0f, 0.0f, 0.0f, 0.0f);
        append_vec4(bytes, 0.0f, 1.0f, 0.0f, 0.0f);
        append_vec4(bytes, 0.0f, 0.0f, 1.0f, 0.0f);
        append_vec4(bytes, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    const std::size_t time_offset = bytes.size();
    append_f32_le(bytes, 0.0f);
    append_f32_le(bytes, 1.0f);

    const std::size_t translation_offset = bytes.size();
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 0.0f, 2.0f, 0.0f);

    const std::size_t rotation_time_offset = bytes.size();
    append_f32_le(bytes, 0.0f);
    append_f32_le(bytes, 1.0f);

    const std::size_t rotation_offset = bytes.size();
    append_vec4(bytes, 0.0f, 0.0f, 0.0f, 1.0f);
    append_vec4(bytes, 0.0f, 0.0f, 1.0f, 0.0f);

    const auto gltf_path = work_dir / "skin_animation.gltf";
    const std::string buffer_uri = data_uri("application/octet-stream", bytes);
    write_text_file(gltf_path,
                    "{\n"
                    "  \"asset\": { \"version\": \"2.0\" },\n"
                    "  \"buffers\": [{ \"byteLength\": " +
                        std::to_string(bytes.size()) + ", \"uri\": \"" + buffer_uri +
                        "\" }],\n"
                    "  \"bufferViews\": [\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(positions_offset) + ", \"byteLength\": 36 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(joints_offset) + ", \"byteLength\": 24 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(weights_offset) + ", \"byteLength\": 48 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(inverse_bind_offset) + ", \"byteLength\": 128 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(time_offset) + ", \"byteLength\": 8 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(translation_offset) + ", \"byteLength\": 24 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(rotation_time_offset) + ", \"byteLength\": 8 },\n"
                    "    { \"buffer\": 0, \"byteOffset\": " +
                        std::to_string(rotation_offset) + ", \"byteLength\": 32 }\n"
                    "  ],\n"
                    "  \"accessors\": [\n"
                    "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 1, \"componentType\": 5123, \"count\": 3, "
                    "\"type\": \"VEC4\" },\n"
                    "    { \"bufferView\": 2, \"componentType\": 5126, \"count\": 3, "
                    "\"type\": \"VEC4\" },\n"
                    "    { \"bufferView\": 3, \"componentType\": 5126, \"count\": 2, "
                    "\"type\": \"MAT4\" },\n"
                    "    { \"bufferView\": 4, \"componentType\": 5126, \"count\": 2, "
                    "\"type\": \"SCALAR\" },\n"
                    "    { \"bufferView\": 5, \"componentType\": 5126, \"count\": 2, "
                    "\"type\": \"VEC3\" },\n"
                    "    { \"bufferView\": 6, \"componentType\": 5126, \"count\": 2, "
                    "\"type\": \"SCALAR\" },\n"
                    "    { \"bufferView\": 7, \"componentType\": 5126, \"count\": 2, "
                    "\"type\": \"VEC4\" }\n"
                    "  ],\n"
                    "  \"skins\": [{ \"name\": \"skin0\", \"joints\": [1, 2], "
                    "\"skeleton\": 0, \"inverseBindMatrices\": 3 }],\n"
                    "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { "
                    "\"POSITION\": 0, \"JOINTS_0\": 1, \"WEIGHTS_0\": 2 }, "
                    "\"mode\": 4 }] }],\n"
                    "  \"nodes\": [\n"
                    "    { \"name\": \"root\", \"children\": [1, 2, 3] },\n"
                    "    { \"name\": \"joint0\" },\n"
                    "    { \"name\": \"joint1\" },\n"
                    "    { \"name\": \"skinnedMesh\", \"mesh\": 0, \"skin\": 0 }\n"
                    "  ],\n"
                    "  \"animations\": [{\n"
                    "    \"name\": \"anim0\",\n"
                    "    \"samplers\": [\n"
                    "      { \"input\": 4, \"output\": 5, \"interpolation\": \"LINEAR\" },\n"
                    "      { \"input\": 6, \"output\": 7, \"interpolation\": \"STEP\" }\n"
                    "    ],\n"
                    "    \"channels\": [\n"
                    "      { \"sampler\": 0, \"target\": { \"node\": 1, "
                    "\"path\": \"translation\" } },\n"
                    "      { \"sampler\": 1, \"target\": { \"node\": 2, "
                    "\"path\": \"rotation\" } }\n"
                    "    ]\n"
                    "  }],\n"
                    "  \"scenes\": [{ \"nodes\": [0] }],\n"
                    "  \"scene\": 0\n"
                    "}\n");

    auto scene = stellar::import::gltf::load_scene(gltf_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    const auto& primitive = scene->meshes[0].primitives[0];
    const auto& skin = scene->skins[0];
    const auto& animation = scene->animations[0];
    return check(scene->skins.size() == 1, "expected one skin") &&
           check(scene->animations.size() == 1, "expected one animation") &&
           check(scene->nodes[3].skin_index.has_value() && *scene->nodes[3].skin_index == 0,
                 "expected node skin index") &&
           check(primitive.has_skinning, "expected skinned primitive") &&
           check(primitive.vertices[0].joints0[0] == 0 && primitive.vertices[0].joints0[1] == 1,
                 "expected first JOINTS_0 values") &&
           check_vec4(primitive.vertices[1].weights0, {0.5f, 0.5f, 0.0f, 0.0f},
                      "expected normalized WEIGHTS_0 values") &&
           check(skin.joints.size() == 2 && skin.joints[0] == 1 && skin.joints[1] == 2,
                 "expected skin joint node indices") &&
           check(skin.skeleton_root.has_value() && *skin.skeleton_root == 0,
                 "expected skin skeleton root") &&
           check(skin.inverse_bind_matrices.size() == 2,
                 "expected inverse bind matrix count") &&
           check_near(skin.inverse_bind_matrices[1][0], 2.0f,
                      "expected second inverse bind matrix value") &&
           check(animation.samplers.size() == 2, "expected animation samplers") &&
           check(animation.channels.size() == 2, "expected animation channels") &&
           check(animation.samplers[0].input_times.size() == 2,
                 "expected animation input times") &&
           check(animation.samplers[0].output_components == 3,
                 "expected translation output components") &&
           check_vec3({animation.samplers[0].output_values[3], animation.samplers[0].output_values[4],
                       animation.samplers[0].output_values[5]},
                      {0.0f, 2.0f, 0.0f}, "expected translation output values") &&
           check(animation.channels[0].target_node.has_value() &&
                     *animation.channels[0].target_node == 1,
                 "expected animation channel target node") &&
           check(animation.channels[0].target_path ==
                     stellar::assets::AnimationTargetPath::kTranslation,
                 "expected translation target path") &&
           check(animation.samplers[1].interpolation ==
                     stellar::assets::AnimationInterpolation::kStep,
                 "expected step interpolation");
}

bool run_skin_attribute_validation_failures_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "skin_attribute_failures";
    std::filesystem::create_directories(work_dir);

    std::vector<std::uint8_t> bytes;
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 1.0f, 0.0f, 0.0f);
    append_vec3(bytes, 0.0f, 1.0f, 0.0f);
    for (int i = 0; i < 3; ++i) {
        append_u16_le(bytes, 0);
        append_u16_le(bytes, 0);
        append_u16_le(bytes, 0);
        append_u16_le(bytes, 0);
    }

    const std::string buffer_uri = data_uri("application/octet-stream", bytes);
    const std::string prefix =
        "{\n"
        "  \"asset\": { \"version\": \"2.0\" },\n"
        "  \"buffers\": [{ \"byteLength\": " +
        std::to_string(bytes.size()) + ", \"uri\": \"" + buffer_uri + "\" }],\n"
        "  \"bufferViews\": [\n"
        "    { \"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36 },\n"
        "    { \"buffer\": 0, \"byteOffset\": 36, \"byteLength\": 24 }\n"
        "  ],\n"
        "  \"accessors\": [\n"
        "    { \"bufferView\": 0, \"componentType\": 5126, \"count\": 3, "
        "\"type\": \"VEC3\" },\n"
        "    { \"bufferView\": 1, \"componentType\": 5123, \"count\": 3, "
        "\"type\": \"VEC4\" }\n"
        "  ],\n"
        "  \"meshes\": [{ \"primitives\": [{ \"attributes\": { ";
    const std::string suffix =
        " }, \"mode\": 4 }] }],\n"
        "  \"nodes\": [{ \"mesh\": 0 }],\n"
        "  \"scenes\": [{ \"nodes\": [0] }],\n"
        "  \"scene\": 0\n"
        "}\n";

    const auto missing_weights = prefix + "\"POSITION\": 0, \"JOINTS_0\": 1" + suffix;
    const auto invalid_weights =
        prefix + "\"POSITION\": 0, \"JOINTS_0\": 1, \"WEIGHTS_0\": 1" + suffix;

    return expect_load_failure(work_dir / "missing_weights.gltf", missing_weights,
                               "missing WEIGHTS_0") &&
           expect_load_failure(work_dir / "invalid_weights.gltf", invalid_weights,
                               "invalid WEIGHTS_0");
}

bool run_glb_smoke_fixture(const std::filesystem::path& root) {
    const auto work_dir = root / "glb_smoke";
    std::filesystem::create_directories(work_dir);

    const auto glb_path = work_dir / "empty_scene.glb";
    write_glb_file(glb_path,
                   "{\n"
                   "  \"asset\": { \"version\": \"2.0\" },\n"
                   "  \"scenes\": [{ \"name\": \"empty\", \"nodes\": [] }],\n"
                   "  \"scene\": 0\n"
                   "}\n");

    auto scene = stellar::import::gltf::load_scene(glb_path.string());
    if (!scene) {
        std::cerr << "load_scene failed: " << scene.error().message << '\n';
        return false;
    }

    return check(scene->scenes.size() == 1, "expected one scene from generated glb") &&
           check(scene->default_scene_index.has_value() && *scene->default_scene_index == 0,
                 "expected glb default scene index 0");
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
    if (!run_material_variants_fixture(root)) {
        return 1;
    }
    if (!run_tangent_and_bounds_fixture(root)) {
        return 1;
    }
    if (!run_attribute_validation_failures_fixture(root)) {
        return 1;
    }
    if (!run_uv1_vertex_color_fixture(root)) {
        return 1;
    }
    if (!run_glb_bin_chunk_fixture(root)) {
        return 1;
    }
    if (!run_generated_tangent_fixture(root)) {
        return 1;
    }
    if (!run_degenerate_tangent_fixture(root)) {
        return 1;
    }
    if (!run_skin_animation_fixture(root)) {
        return 1;
    }
    if (!run_skin_attribute_validation_failures_fixture(root)) {
        return 1;
    }
    if (!run_glb_smoke_fixture(root)) {
        return 1;
    }

    return 0;
}
