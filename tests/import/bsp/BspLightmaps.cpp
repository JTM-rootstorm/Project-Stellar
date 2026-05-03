#include "stellar/import/bsp/Loader.hpp"

#include "BspFixture.hpp"

#include <cassert>

namespace {

void valid_lighting_lump_creates_surface_lightmap_metadata() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, 0, 12);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.raw_lighting.size() == 12);
    assert(result->asset.geometry.lightmaps.size() == 1);
    assert(result->asset.geometry.images.size() == 1);
    assert(result->asset.geometry.lightmaps[0].size[0] == 2);
    assert(result->asset.geometry.lightmaps[0].size[1] == 2);
    assert(result->asset.geometry.materials.size() == 1);
    assert(result->asset.geometry.materials[0].lightmap_index == 0);
    const auto &primitive = result->asset.geometry.meshes[0].primitives[0];
    assert(primitive.vertices[0].uv1[0] == 0.0F);
    assert(primitive.vertices[0].uv1[1] == 0.0F);
    assert(primitive.vertices[2].uv1[0] == 1.0F);
    assert(primitive.vertices[2].uv1[1] == 1.0F);
}

void embedded_texture_and_lightmap_preserve_both_uv_sets() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(true, 0, 12);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "textured_lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.textures.size() == 1);
    assert(result->asset.geometry.lightmaps.size() == 1);
    assert(result->asset.geometry.materials.size() == 1);
    const auto &material = result->asset.geometry.materials[0];
    assert(material.texture_index == 0);
    assert(material.lightmap_index == 0);
    const auto &primitive = result->asset.geometry.meshes[0].primitives[0];
    assert(primitive.vertices[2].uv0[0] == 8.0F);
    assert(primitive.vertices[2].uv0[1] == 8.0F);
    assert(primitive.vertices[2].uv1[0] == 1.0F);
    assert(primitive.vertices[2].uv1[1] == 1.0F);
}

void multiple_faces_with_same_texture_get_distinct_lightmap_materials() {
    const auto bytes = stellar::tests::bsp_fixture::two_lit_faces_bsp();
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "two_lightmaps.bsp");
    assert(result);
    assert(result->asset.geometry.lightmaps.size() == 2);
    assert(result->asset.geometry.materials.size() == 2);
    assert(result->asset.geometry.materials[0].source_name ==
           result->asset.geometry.materials[1].source_name);
    assert(result->asset.geometry.materials[0].lightmap_index == 0);
    assert(result->asset.geometry.materials[1].lightmap_index == 1);
    assert(result->asset.geometry.lightmaps[0].style == 0);
    assert(result->asset.geometry.lightmaps[1].style == 2);
}

void invalid_light_offset_warns_and_falls_back_unlit() {
    const auto bytes = stellar::tests::bsp_fixture::single_face_bsp(false, 100, 3);
    const auto result = stellar::import::bsp::load_level_from_memory_with_report(
        bytes, "bad_lightmap.bsp");
    assert(result);
    assert(result->asset.geometry.raw_lighting.size() == 3);
    assert(result->asset.geometry.lightmaps.empty());
    assert(!result->asset.geometry.materials[0].lightmap_index.has_value());

    bool found_invalid_lighting = false;
    for (const auto &diagnostic : result->report.diagnostics) {
        found_invalid_lighting = found_invalid_lighting ||
                                 diagnostic.code ==
                                     stellar::import::bsp::DiagnosticCode::kInvalidLightingData;
    }
    assert(found_invalid_lighting);
}

} // namespace

int main() {
    valid_lighting_lump_creates_surface_lightmap_metadata();
    embedded_texture_and_lightmap_preserve_both_uv_sets();
    multiple_faces_with_same_texture_get_distinct_lightmap_materials();
    invalid_light_offset_warns_and_falls_back_unlit();
    return 0;
}
