#include "stellar/import/bsp/Loader.hpp"

#include "../../../src/import/bsp/DeveloperTextures.hpp"
#include "BspFixture.hpp"

#include <cassert>
#include <cstddef>
#include <string>

namespace {

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
    assert(grid->image.width == 12);
    assert(grid->image.height == 12);
    assert(grid->image.format == stellar::assets::ImageFormat::kR8G8B8A8);
    assert(grid->image.pixels.size() == 12U * 12U * 4U);
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
    assert(image.width == 64);
    assert(image.height == 64);
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
    assert(vertices[1].uv0[0] == 0.25F);

    for (const auto &diagnostic : result->report.diagnostics) {
        assert(diagnostic.code != stellar::import::bsp::DiagnosticCode::kMissingTexture);
    }
}

} // namespace

int main() {
    embedded_miptex_creates_image_and_texture();
    missing_external_texture_uses_fallback_and_warning();
    surface_material_resolves_to_preserved_source_name();
    developer_texture_names_are_known();
    external_developer_texture_alias_creates_material_binding();
    return 0;
}
