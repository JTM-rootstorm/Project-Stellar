#include "stellar/import/bsp/Loader.hpp"

#include "../../../src/import/bsp/DeveloperTextures.hpp"
#include "BspFixture.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

void set_entities(std::vector<std::byte> &bytes, const std::string &entities) {
    const std::size_t entity_offset = bytes.size();
    bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(entities.data()),
                 reinterpret_cast<const std::byte *>(entities.data() + entities.size()));
    stellar::tests::bsp_fixture::set_lump(
        bytes, stellar::import::bsp::detail::LumpIndex::kEntities, entity_offset, entities.size());
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
    shifted_scaled_texinfo_produces_expected_uv0();
    missing_and_unsafe_wad_paths_are_diagnosed();
    return 0;
}
