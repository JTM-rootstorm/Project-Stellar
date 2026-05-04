#include "stellar/import/bsp/Loader.hpp"

#include "../../../src/import/bsp/DeveloperTextures.hpp"
#include "BspFixture.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace {

void assert_pixel(const stellar::assets::ImageAsset &image, std::uint32_t x,
                  std::uint32_t y, std::uint8_t r, std::uint8_t g,
                  std::uint8_t b) {
    const std::size_t offset = (static_cast<std::size_t>(y) * image.width + x) * 4U;
    assert(offset + 3U < image.pixels.size());
    assert(image.pixels[offset] == r);
    assert(image.pixels[offset + 1U] == g);
    assert(image.pixels[offset + 2U] == b);
    assert(image.pixels[offset + 3U] == 255U);
}

void set_entities(std::vector<std::byte> &bytes, const std::string &entities) {
    const std::size_t entity_offset = bytes.size();
    bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(entities.data()),
                 reinterpret_cast<const std::byte *>(entities.data() + entities.size()));
    stellar::tests::bsp_fixture::set_lump(
        bytes, stellar::import::bsp::detail::LumpIndex::kEntities, entity_offset, entities.size());
}

void write_text_file(const std::filesystem::path &path, const std::string &text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    assert(file);
    file << text;
}

void write_bmp(const std::filesystem::path &path,
               const std::vector<std::array<std::uint8_t, 4>> &pixels,
               std::int32_t width, std::int32_t height) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    assert(file);
    const std::uint32_t file_size = 14U + 40U + pixels.size() * 4U;
    const std::uint32_t pixel_offset = 14U + 40U;
    file.put('B');
    file.put('M');
    auto write_u16 = [&](std::uint16_t value) {
        file.put(static_cast<char>(value & 0xFFU));
        file.put(static_cast<char>((value >> 8U) & 0xFFU));
    };
    auto write_u32 = [&](std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            file.put(static_cast<char>((value >> shift) & 0xFFU));
        }
    };
    auto write_i32 = [&](std::int32_t value) {
        write_u32(static_cast<std::uint32_t>(value));
    };
    write_u32(file_size);
    write_u16(0);
    write_u16(0);
    write_u32(pixel_offset);
    write_u32(40);
    write_i32(width);
    write_i32(-height);
    write_u16(1);
    write_u16(32);
    write_u32(0);
    write_u32(static_cast<std::uint32_t>(pixels.size() * 4U));
    write_i32(2835);
    write_i32(2835);
    write_u32(0);
    write_u32(0);
    for (const auto &pixel : pixels) {
        file.put(static_cast<char>(pixel[2]));
        file.put(static_cast<char>(pixel[1]));
        file.put(static_cast<char>(pixel[0]));
        file.put(static_cast<char>(pixel[3]));
    }
}

std::filesystem::path reset_material_fixture_root(const std::string &name) {
    const auto root = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void embedded_miptex_creates_image_and_texture() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(true);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "embedded_texture.bsp");
    assert(result);
    assert(result->asset.geometry.images.size() == 1);
    assert(result->asset.geometry.textures.size() == 1);
    assert(result->asset.geometry.materials.size() == 1);
    const auto &image = result->asset.geometry.images[0];
    assert(image.name == "stone");
    assert(image.width == 2);
    assert(image.height == 2);
    assert(image.pixels.size() == 16);
    assert(image.pixels[0] == 1U);
    assert(image.pixels[4] == 2U);
    assert(result->asset.geometry.materials[0].source_name == "stone");
    assert(result->asset.geometry.materials[0].texture_index == 0);
}

void missing_external_texture_uses_fallback_and_warning() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "external_texture.bsp");
    assert(result);
    assert(result->asset.geometry.images.empty());
    assert(result->asset.geometry.textures.empty());
    assert(result->asset.geometry.materials.size() == 1);
    assert(result->asset.geometry.materials[0].name == "stone");
    assert(result->asset.geometry.materials[0].source_name == "stone");
    assert(!result->asset.geometry.materials[0].texture_index.has_value());

    bool found_missing_texture = false;
    for (const auto &diagnostic : result->report.diagnostics) {
        found_missing_texture = found_missing_texture ||
                                diagnostic.code ==
                                    stellar::import::bsp::DiagnosticCode::kMissingTexture;
    }
    assert(found_missing_texture);
}

void surface_material_resolves_to_preserved_source_name() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(true);
    const auto result = stellar::import::bsp::load_level_from_memory(bytes, "surface.bsp");
    assert(result);
    assert(result->geometry.surfaces.size() == 1);
    assert(result->geometry.surfaces[0].material_index.has_value());
    const std::size_t material_index = *result->geometry.surfaces[0].material_index;
    assert(material_index < result->geometry.materials.size());
    assert(result->geometry.materials[material_index].source_name == "stone");
}

void developer_texture_names_are_known() {
    const auto grid = stellar::import::bsp::detail::make_developer_texture(
        "stellar_dev_grid_12", "developer_names.bsp");
    assert(grid.has_value());
    assert(grid->image.name == "stellar_dev_grid_12");
    assert(grid->image.width == 128);
    assert(grid->image.height == 128);
    assert(grid->image.format == stellar::assets::ImageFormat::kR8G8B8A8);
    assert(grid->image.pixels.size() == 128U * 128U * 4U);
    assert(grid->sampler.mag_filter == stellar::assets::TextureFilter::kNearest);
    assert(grid->sampler.min_filter == stellar::assets::TextureFilter::kNearest);
    assert(grid->sampler.wrap_s == stellar::assets::TextureWrapMode::kRepeat);
    assert(grid->sampler.wrap_t == stellar::assets::TextureWrapMode::kRepeat);

    assert(stellar::import::bsp::detail::make_developer_texture(
               "stellar_dev_grid_16", "developer_names.bsp")
               .has_value());
    assert(stellar::import::bsp::detail::make_developer_texture(
               "stellar_dev_grid_32", "developer_names.bsp")
               .has_value());
    assert(stellar::import::bsp::detail::make_developer_texture(
               "stellar_dev_grid_64", "developer_names.bsp")
               .has_value());
    assert(stellar::import::bsp::detail::make_developer_texture(
               "stellar_dev_player_72", "developer_names.bsp")
               .has_value());
    assert(stellar::import::bsp::detail::make_developer_texture(
               "stellar_dev_wall_96", "developer_names.bsp")
               .has_value());
    const char *aliases[] = {"dev/grid_12", "dev/grid_16", "dev/grid_32",
                             "dev/grid_64", "dev/player_72", "dev/wall_96",
                             "dev_grid_12", "dev_grid_16", "dev_grid_32",
                             "dev_grid_64", "dev_player_72", "dev_wall_96",
                             "stellar_dev_grid_12", "stellar_dev_grid_16",
                             "stellar_dev_grid_32", "stellar_dev_grid_64",
                             "stellar_dev_player_72", "stellar_dev_wall_96"};
    for (const char *alias : aliases) {
        auto texture = stellar::import::bsp::detail::make_developer_texture(
            alias, "developer_names.bsp");
        assert(texture.has_value());
    }
    assert(!stellar::import::bsp::detail::make_developer_texture(
                 "unknown_missing", "developer_names.bsp")
                 .has_value());
}

void external_developer_texture_alias_creates_material_binding() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(
        false, -1, 0, "dev/grid_64");
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "developer_alias.bsp");
    assert(result);
    assert(result->asset.geometry.images.size() == 1);
    assert(result->asset.geometry.textures.size() == 1);
    assert(result->asset.geometry.samplers.size() == 1);
    assert(result->asset.geometry.materials.size() == 1);

    const auto &image = result->asset.geometry.images[0];
    assert(image.name == "stellar_dev_grid_64");
    assert(image.width == 128);
    assert(image.height == 128);
    assert(!image.pixels.empty());

    const auto &sampler = result->asset.geometry.samplers[0];
    assert(sampler.mag_filter == stellar::assets::TextureFilter::kNearest);
    assert(sampler.min_filter == stellar::assets::TextureFilter::kNearest);
    assert(sampler.wrap_s == stellar::assets::TextureWrapMode::kRepeat);
    assert(sampler.wrap_t == stellar::assets::TextureWrapMode::kRepeat);

    const auto &texture = result->asset.geometry.textures[0];
    assert(texture.image_index == 0);
    assert(texture.sampler_index == 0);
    assert(texture.color_space == stellar::assets::TextureColorSpace::kSrgb);

    const auto &material = result->asset.geometry.materials[0];
    assert(material.name == "dev/grid_64");
    assert(material.source_name == "dev/grid_64");
    assert(material.texture_index == 0);
    const auto &vertices = result->asset.geometry.meshes[0].primitives[0].vertices;
    assert(vertices[1].uv0[0] == 0.125F);

    for (const auto &diagnostic : result->report.diagnostics) {
        assert(diagnostic.code != stellar::import::bsp::DiagnosticCode::kMissingTexture);
    }
}

void external_wad_texture_resolves_from_safe_search_path() {
    auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, -1, 0, "dev_grid_32");
    set_entities(bytes, "{\n\"classname\" \"worldspawn\"\n\"wad\" \"stellar_dev.wad\"\n}\n"
                        "{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 36\"\n}\n");
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, (std::filesystem::current_path() / "wad_external.bsp").string());
    assert(result);
    assert(result->asset.geometry.images.size() == 1);
    assert(result->asset.geometry.textures.size() == 1);
    assert(result->asset.geometry.images[0].name == "dev_grid_32");
    assert(result->asset.geometry.images[0].width == 128);
    assert(result->asset.geometry.images[0].height == 128);
    assert(result->asset.geometry.materials[0].source_name == "dev_grid_32");
    for (const auto &diagnostic : result->report.diagnostics) {
        assert(diagnostic.message.find("rejected") == std::string::npos);
    }
}

void developer_wad_and_fallback_pixels_match() {
    const std::vector<std::string> names{"dev_grid_32", "dev_grid_64",
                                         "dev_wall_96"};
    for (const std::string &name : names) {
        auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, -1, 0,
                                                                 name);
        set_entities(bytes, "{\n\"classname\" \"worldspawn\"\n\"wad\" \"stellar_dev.wad\"\n}\n"
                            "{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 36\"\n}\n");
        const auto result = stellar::import::bsp::load_level_from_memory_with_report(
            bytes, (std::filesystem::current_path() / (name + ".bsp")).string());
        assert(result);
        assert(result->asset.geometry.images.size() == 1);

        const auto fallback = stellar::import::bsp::detail::make_developer_texture(
            name, "fallback_match.bsp");
        assert(fallback.has_value());
        const auto &wad_image = result->asset.geometry.images[0];
        assert(wad_image.width == fallback->image.width);
        assert(wad_image.height == fallback->image.height);
        assert(wad_image.format == fallback->image.format);
        assert(wad_image.pixels == fallback->image.pixels);
    }
}

void semantic_developer_texture_sample_pixels_are_stable() {
    const auto wall = stellar::import::bsp::detail::make_developer_texture(
        "dev/wall_96", "semantic_samples.bsp");
    assert(wall.has_value());
    assert(wall->image.width == 128);
    assert(wall->image.height == 128);
    assert_pixel(wall->image, 24, 24, 255, 224, 128);
    assert_pixel(wall->image, 8, 48, 255, 224, 128);
    assert_pixel(wall->image, 8, 49, 255, 248, 220);
    assert_pixel(wall->image, 64, 96, 240, 64, 48);
    assert_pixel(wall->image, 49, 113, 96, 64, 32);

    const auto grid = stellar::import::bsp::detail::make_developer_texture(
        "dev/grid_64", "semantic_samples.bsp");
    assert(grid.has_value());
    assert(grid->image.width == 128);
    assert(grid->image.height == 128);
    assert_pixel(grid->image, 1, 48, 255, 224, 128);
    assert_pixel(grid->image, 64, 10, 255, 224, 128);
    assert_pixel(grid->image, 16, 24, 122, 148, 176);
    assert_pixel(grid->image, 24, 24, 72, 84, 96);
    assert_pixel(grid->image, 88, 88, 72, 84, 96);
}

void shifted_scaled_texinfo_produces_expected_uv0() {
    auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, -1, 0,
                                                             "dev/grid_32");
    const std::size_t texinfo_offset =
        stellar::tests::bsp_fixture::lump_header_offset(
            stellar::import::bsp::detail::LumpIndex::kTexinfo);
    std::int32_t texinfo_lump = 0;
    std::memcpy(&texinfo_lump, bytes.data() + texinfo_offset,
                sizeof(texinfo_lump));
    auto patch_float = [&](std::size_t offset, float value) {
        std::memcpy(bytes.data() + static_cast<std::size_t>(texinfo_lump) + offset,
                    &value, sizeof(value));
    };
    patch_float(0, 0.5F);
    patch_float(4, 0.0F);
    patch_float(8, 0.0F);
    patch_float(12, 8.0F);
    patch_float(16, 0.0F);
    patch_float(20, 0.25F);
    patch_float(24, 0.0F);
    patch_float(28, 16.0F);

    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "shifted_scaled_texinfo.bsp");
    assert(result);
    const auto &vertices =
        result->asset.geometry.meshes[0].primitives[0].vertices;
    assert(vertices[0].uv0[0] == 8.0F / 128.0F);
    assert(vertices[0].uv0[1] == 16.0F / 128.0F);
    assert(vertices[1].uv0[0] == 16.0F / 128.0F);
    assert(vertices[1].uv0[1] == 16.0F / 128.0F);
    assert(vertices[2].uv0[0] == 16.0F / 128.0F);
    assert(vertices[2].uv0[1] == 20.0F / 128.0F);
}

void sidecar_resolves_normal_and_specular_from_explicit_root() {
    const auto root = reset_material_fixture_root("stellar_snt_materials_positive");
    const auto material_dir = root / "dev";
    write_bmp(material_dir / "wall_96_normal.bmp",
              {{128, 128, 255, 255}, {128, 128, 255, 255},
               {128, 128, 255, 255}, {128, 128, 255, 255}},
              2, 2);
    write_bmp(material_dir / "wall_96_spec.bmp",
              {{255, 255, 255, 255}, {0, 0, 0, 255},
               {128, 128, 128, 255}, {64, 64, 64, 255}},
              2, 2);
    write_text_file(material_dir / "wall_96.stellar_material",
                    "version = 1\n"
                    "name = \"dev/wall_96\"\n"
                    "normal = \"wall_96_normal.bmp\"\n"
                    "normal_scale = 1.5\n"
                    "normal_light_strength = 0.25\n"
                    "specular = \"wall_96_spec.bmp\"\n"
                    "specular_factor = 0.35\n"
                    "specular_power = 48\n"
                    "roughness_factor = 0.75\n"
                    "double_sided = true\n"
                    "unlit = false\n");

    stellar::import::bsp::LoadOptions options;
    options.material_search_roots.push_back(root);
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(
        false, -1, 0, "dev/wall_96");
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "sidecar_positive.bsp", options);
    assert(result);
    assert(result->asset.geometry.materials.size() == 1);
    const auto &surface_material = result->asset.geometry.materials[0];
    assert(surface_material.resolved_material.has_value());
    const auto &material = *surface_material.resolved_material;
    assert(material.name == "dev/wall_96");
    assert(material.base_color_texture.has_value());
    assert(material.base_color_texture->texture_index == surface_material.texture_index);
    assert(material.normal_texture.has_value());
    assert(material.specular_texture.has_value());
    assert(material.normal_scale == 1.5F);
    assert(material.normal_light_strength == 0.25F);
    assert(material.specular_factor == 0.35F);
    assert(material.specular_power == 48.0F);
    assert(material.roughness_factor == 0.75F);
    assert(material.double_sided);
    assert(!material.unlit);
    assert(result->asset.geometry.textures[material.normal_texture->texture_index]
               .color_space == stellar::assets::TextureColorSpace::kLinear);
    assert(result->asset.geometry.textures[material.specular_texture->texture_index]
               .color_space == stellar::assets::TextureColorSpace::kLinear);
}

void invalid_sidecar_preserves_fallback_and_strict_fails() {
    const auto root = reset_material_fixture_root("stellar_snt_materials_invalid");
    const auto material_dir = root / "dev";
    write_text_file(material_dir / "grid_64.stellar_material",
                    "version = 1\n"
                    "normal = \"../escape.bmp\"\n");

    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(
        false, -1, 0, "dev/grid_64");
    stellar::import::bsp::LoadOptions options;
    options.material_search_roots.push_back(root);
    const auto non_strict =
        stellar::import::bsp::load_level_from_memory_with_report(
            bytes, "sidecar_invalid.bsp", options);
    assert(non_strict);
    assert(!non_strict->asset.geometry.materials[0].resolved_material.has_value());
    bool saw_unsafe_path = false;
    for (const auto &diagnostic : non_strict->report.diagnostics) {
        saw_unsafe_path = saw_unsafe_path ||
                          diagnostic.code ==
                              stellar::import::bsp::DiagnosticCode::
                                  kMaterialSidecarUnsafePath;
    }
    assert(saw_unsafe_path);

    options.strict_material_sidecars = true;
    const auto strict = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "sidecar_invalid_strict.bsp", options);
    assert(!strict);
}

void unknown_sidecar_key_warns_or_fails_strict() {
    const auto root = reset_material_fixture_root("stellar_snt_materials_unknown");
    write_text_file(root / "dev" / "grid_32.stellar_material",
                    "version = 1\n"
                    "name = \"dev/grid_32\"\n"
                    "mystery = true\n");
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(
        false, -1, 0, "dev/grid_32");
    stellar::import::bsp::LoadOptions options;
    options.material_search_roots.push_back(root);
    const auto non_strict =
        stellar::import::bsp::load_level_from_memory_with_report(
            bytes, "sidecar_unknown.bsp", options);
    assert(non_strict);
    assert(non_strict->asset.geometry.materials[0].resolved_material.has_value());
    bool saw_unknown_key = false;
    for (const auto &diagnostic : non_strict->report.diagnostics) {
        saw_unknown_key = saw_unknown_key ||
                          diagnostic.code ==
                              stellar::import::bsp::DiagnosticCode::
                                  kMaterialSidecarUnknownKey;
    }
    assert(saw_unknown_key);

    options.strict_material_sidecars = true;
    const auto strict = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "sidecar_unknown_strict.bsp", options);
    assert(!strict);
}

void bsp_faces_generate_tangents_from_texinfo() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(
        false, -1, 0, "dev/grid_64");
    const auto result = stellar::import::bsp::load_level_from_memory(
        bytes, "tangent_valid.bsp");
    assert(result);
    const auto &primitive = result->geometry.meshes[0].primitives[0];
    assert(primitive.has_tangents);
    const auto &vertex = primitive.vertices[0];
    const auto &normal = vertex.normal;
    const auto &tangent = vertex.tangent;
    const float tangent_length =
        std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1] +
                  tangent[2] * tangent[2]);
    assert(std::abs(tangent_length - 1.0F) < 0.0001F);
    const float dot = normal[0] * tangent[0] + normal[1] * tangent[1] +
                      normal[2] * tangent[2];
    assert(std::abs(dot) < 0.0001F);
    assert(tangent[3] == 1.0F || tangent[3] == -1.0F);
}

void degenerate_texinfo_skips_tangents() {
    auto bytes = stellar::tests::bsp_fixture::single_face_bsp(
        false, -1, 0, "dev/grid_64");
    const std::size_t texinfo_offset =
        stellar::tests::bsp_fixture::lump_header_offset(
            stellar::import::bsp::detail::LumpIndex::kTexinfo);
    std::int32_t texinfo_lump = 0;
    std::memcpy(&texinfo_lump, bytes.data() + texinfo_offset,
                sizeof(texinfo_lump));
    const float zero = 0.0F;
    std::memcpy(bytes.data() + static_cast<std::size_t>(texinfo_lump), &zero,
                sizeof(zero));

    const auto result = stellar::import::bsp::load_level_from_memory(
        bytes, "tangent_degenerate.bsp");
    assert(result);
    assert(!result->geometry.meshes[0].primitives[0].has_tangents);
}

void missing_and_unsafe_wad_paths_are_diagnosed() {
    auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, -1, 0, "stone");
    set_entities(bytes, "{\n\"classname\" \"worldspawn\"\n\"wad\" \"../escape.wad;/tmp/local.wad;missing.wad\"\n}\n"
                        "{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 36\"\n}\n");
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, (std::filesystem::current_path() / "wad_bad.bsp").string());
    assert(result);
    bool saw_parent_escape = false;
    bool saw_absolute = false;
    bool saw_attempt = false;
    for (const auto &diagnostic : result->report.diagnostics) {
        saw_parent_escape = saw_parent_escape || diagnostic.message.find("parent escape") != std::string::npos;
        saw_absolute = saw_absolute || diagnostic.message.find("rejected absolute WAD path") != std::string::npos;
        saw_attempt = saw_attempt || diagnostic.message.find("tried WAD path") != std::string::npos;
    }
    assert(saw_parent_escape);
    assert(saw_absolute);
    assert(saw_attempt);
}

} // namespace

int main() {
    embedded_miptex_creates_image_and_texture();
    missing_external_texture_uses_fallback_and_warning();
    surface_material_resolves_to_preserved_source_name();
    developer_texture_names_are_known();
    external_developer_texture_alias_creates_material_binding();
    external_wad_texture_resolves_from_safe_search_path();
    developer_wad_and_fallback_pixels_match();
    semantic_developer_texture_sample_pixels_are_stable();
    shifted_scaled_texinfo_produces_expected_uv0();
    sidecar_resolves_normal_and_specular_from_explicit_root();
    invalid_sidecar_preserves_fallback_and_strict_fails();
    unknown_sidecar_key_warns_or_fails_strict();
    bsp_faces_generate_tangents_from_texinfo();
    degenerate_texinfo_skips_tangents();
    missing_and_unsafe_wad_paths_are_diagnosed();
    return 0;
}
