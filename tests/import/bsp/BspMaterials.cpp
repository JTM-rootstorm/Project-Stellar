#include "stellar/import/bsp/Loader.hpp"

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

} // namespace

int main() {
    embedded_miptex_creates_image_and_texture();
    missing_external_texture_uses_fallback_and_warning();
    surface_material_resolves_to_preserved_source_name();
    return 0;
}
