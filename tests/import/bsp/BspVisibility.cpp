#include "stellar/assets/LevelAsset.hpp"
#include "stellar/assets/LevelVisibilityQueries.hpp"
#include "stellar/import/bsp/Loader.hpp"

#include "../../../src/import/bsp/BspBinary.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

using stellar::import::bsp::detail::LumpIndex;

template <typename T> void append(std::vector<std::byte>& bytes, T value) {
    const auto* raw = reinterpret_cast<const std::byte*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

void patch_i32(std::vector<std::byte>& bytes, std::size_t offset, std::int32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::size_t lump_header_offset(LumpIndex lump) {
    return 4 + static_cast<std::size_t>(lump) * 8;
}

void set_lump(std::vector<std::byte>& bytes, LumpIndex lump, std::size_t offset,
              std::size_t size) {
    patch_i32(bytes, lump_header_offset(lump), static_cast<std::int32_t>(offset));
    patch_i32(bytes, lump_header_offset(lump) + 4, static_cast<std::int32_t>(size));
}

void append_vec3(std::vector<std::byte>& bytes, float x, float y, float z) {
    append(bytes, x);
    append(bytes, y);
    append(bytes, z);
}

void append_plane(std::vector<std::byte>& bytes) {
    append_vec3(bytes, 0.0F, 0.0F, 1.0F);
    append(bytes, 0.0F);
    append<std::int32_t>(bytes, 0);
}

void append_texinfo(std::vector<std::byte>& bytes) {
    append(bytes, 1.0F);
    append(bytes, 0.0F);
    append(bytes, 0.0F);
    append(bytes, 0.0F);
    append(bytes, 0.0F);
    append(bytes, 1.0F);
    append(bytes, 0.0F);
    append(bytes, 0.0F);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
}

void append_face(std::vector<std::byte>& bytes, std::int32_t first_edge) {
    append<std::uint16_t>(bytes, 0);
    append<std::uint16_t>(bytes, 0);
    append<std::int32_t>(bytes, first_edge);
    append<std::uint16_t>(bytes, 4);
    append<std::uint16_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
    append<std::int32_t>(bytes, -1);
}

void append_model(std::vector<std::byte>& bytes) {
    append_vec3(bytes, 0.0F, 0.0F, 0.0F);
    append_vec3(bytes, 128.0F, 64.0F, 0.0F);
    append_vec3(bytes, 0.0F, 0.0F, 0.0F);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 2);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 3);
}

void append_leaf(std::vector<std::byte>& bytes, std::int32_t contents,
                 std::int32_t visibility_offset, std::array<std::int16_t, 3> mins,
                 std::array<std::int16_t, 3> maxs, std::uint16_t first_marksurface,
                 std::uint16_t marksurface_count) {
    append<std::int32_t>(bytes, contents);
    append<std::int32_t>(bytes, visibility_offset);
    for (const auto value : mins) {
        append<std::int16_t>(bytes, value);
    }
    for (const auto value : maxs) {
        append<std::int16_t>(bytes, value);
    }
    append<std::uint16_t>(bytes, first_marksurface);
    append<std::uint16_t>(bytes, marksurface_count);
    append<std::uint8_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
}

std::vector<std::byte> two_leaf_bsp(std::vector<std::uint8_t> visibility_bytes = {0x01U, 0x02U}) {
    std::vector<std::byte> bytes(4 + 15 * 8);
    patch_i32(bytes, 0, 29);

    const std::string entities = "{\n\"classname\" \"worldspawn\"\n}\n";
    const std::size_t entity_offset = bytes.size();
    bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(entities.data()),
                 reinterpret_cast<const std::byte*>(entities.data() + entities.size()));
    set_lump(bytes, LumpIndex::kEntities, entity_offset, entities.size());

    const std::size_t plane_offset = bytes.size();
    append_plane(bytes);
    set_lump(bytes, LumpIndex::kPlanes, plane_offset, 20);

    const std::size_t texture_offset = bytes.size();
    append<std::int32_t>(bytes, 1);
    append<std::int32_t>(bytes, 8);
    const char name[16] = {'s', 't', 'o', 'n', 'e', 0};
    bytes.insert(bytes.end(), reinterpret_cast<const std::byte*>(name),
                 reinterpret_cast<const std::byte*>(name + 16));
    append<std::uint32_t>(bytes, 64);
    append<std::uint32_t>(bytes, 64);
    append<std::uint32_t>(bytes, 0);
    append<std::uint32_t>(bytes, 0);
    append<std::uint32_t>(bytes, 0);
    append<std::uint32_t>(bytes, 0);
    set_lump(bytes, LumpIndex::kTextures, texture_offset, bytes.size() - texture_offset);

    const std::size_t vertex_offset = bytes.size();
    append_vec3(bytes, 0.0F, 0.0F, 0.0F);
    append_vec3(bytes, 64.0F, 0.0F, 0.0F);
    append_vec3(bytes, 64.0F, 64.0F, 0.0F);
    append_vec3(bytes, 0.0F, 64.0F, 0.0F);
    append_vec3(bytes, 65.0F, 0.0F, 0.0F);
    append_vec3(bytes, 128.0F, 0.0F, 0.0F);
    append_vec3(bytes, 128.0F, 64.0F, 0.0F);
    append_vec3(bytes, 65.0F, 64.0F, 0.0F);
    set_lump(bytes, LumpIndex::kVertices, vertex_offset, 8 * 12);

    const std::size_t texinfo_offset = bytes.size();
    append_texinfo(bytes);
    set_lump(bytes, LumpIndex::kTexinfo, texinfo_offset, 40);

    const std::size_t face_offset = bytes.size();
    append_face(bytes, 100);
    append_face(bytes, 0);
    append_face(bytes, 4);
    set_lump(bytes, LumpIndex::kFaces, face_offset, 3 * 20);

    const std::size_t edge_offset = bytes.size();
    for (std::uint16_t first = 0; first < 8; ++first) {
        const std::uint16_t second = first % 4 == 3 ? static_cast<std::uint16_t>(first - 3)
                                                    : static_cast<std::uint16_t>(first + 1);
        append<std::uint16_t>(bytes, first);
        append<std::uint16_t>(bytes, second);
    }
    set_lump(bytes, LumpIndex::kEdges, edge_offset, 8 * 4);

    const std::size_t surfedge_offset = bytes.size();
    for (std::int32_t edge = 0; edge < 8; ++edge) {
        append<std::int32_t>(bytes, edge);
    }
    set_lump(bytes, LumpIndex::kSurfedges, surfedge_offset, 8 * 4);

    if (!visibility_bytes.empty()) {
        const std::size_t visibility_offset = bytes.size();
        for (const auto value : visibility_bytes) {
            append<std::uint8_t>(bytes, value);
        }
        set_lump(bytes, LumpIndex::kVisibility, visibility_offset, visibility_bytes.size());
    }

    const std::size_t marksurface_offset = bytes.size();
    append<std::uint16_t>(bytes, 1);
    append<std::uint16_t>(bytes, 0);
    append<std::uint16_t>(bytes, 2);
    set_lump(bytes, LumpIndex::kMarksurfaces, marksurface_offset, 3 * 2);

    const std::size_t leaf_offset = bytes.size();
    append_leaf(bytes, 0, 0, {0, 0, -16}, {64, 64, 16}, 0, 2);
    append_leaf(bytes, 1, 1, {65, 0, -16}, {128, 64, 16}, 2, 1);
    set_lump(bytes, LumpIndex::kLeaves, leaf_offset, 2 * 28);

    const std::size_t model_offset = bytes.size();
    append_model(bytes);
    set_lump(bytes, LumpIndex::kModels, model_offset, 64);
    return bytes;
}

void two_leaf_pvs_fixture_imports_and_queries_visibility() {
    auto result = stellar::import::bsp::load_level_from_memory(two_leaf_bsp(), "two_leaf.bsp");
    assert(result);
    assert(result->visibility.available);
    assert(result->visibility.leaves.size() == 2);
    assert(result->visibility.leaves[0].contents == 0);
    assert(result->visibility.leaves[1].contents == 1);
    assert(result->visibility.leaves[0].compressed_pvs_offset == 0);
    assert(result->visibility.leaves[1].compressed_pvs_offset == 1);

    const auto first_leaf = stellar::assets::find_level_leaf_for_point(result->visibility,
                                                                       {32.0F, 32.0F, 0.0F});
    assert(first_leaf == 0);
    const auto visible = stellar::assets::visible_level_surfaces_from_leaf(*result, *first_leaf);
    assert(visible.size() == 2);
    assert(visible[0]);
    assert(!visible[1]);
}

void marksurfaces_map_to_level_surface_indices_after_skipped_faces() {
    auto result = stellar::import::bsp::load_level_from_memory_with_report(two_leaf_bsp(),
                                                                           "marksurfaces.bsp");
    assert(result);
    assert(result->asset.geometry.surfaces.size() == 2);
    assert(result->asset.visibility.leaves[0].surface_indices.size() == 1);
    assert(result->asset.visibility.leaves[0].surface_indices[0] == 0);
    assert(result->asset.visibility.leaves[1].surface_indices.size() == 1);
    assert(result->asset.visibility.leaves[1].surface_indices[0] == 1);

    bool found_skipped_face_diagnostic = false;
    for (const auto& diagnostic : result->report.diagnostics) {
        found_skipped_face_diagnostic = found_skipped_face_diagnostic ||
                                        diagnostic.code == stellar::import::bsp::DiagnosticCode::kInvalidVisibilityData;
    }
    assert(found_skipped_face_diagnostic);
}

void zero_run_decompression_hides_later_surfaces() {
    stellar::assets::LevelAsset level{};
    level.geometry.surfaces.resize(9);
    level.visibility.available = true;
    level.visibility.cluster_count = 9;
    level.visibility.compressed_pvs = {std::byte{0x01U}, std::byte{0x00U}, std::byte{0x01U}};
    level.visibility.leaves.resize(9);
    level.visibility.leaves[0].compressed_pvs_offset = 0;
    level.visibility.leaves[0].surface_indices = {0};
    level.visibility.leaves[8].surface_indices = {8};

    const auto visible = stellar::assets::visible_level_surfaces_from_leaf(level, 0);
    assert(visible.size() == 9);
    assert(visible[0]);
    assert(!visible[8]);
}

void truncated_pvs_is_diagnosed_and_falls_back() {
    auto result = stellar::import::bsp::load_level_from_memory_with_report(two_leaf_bsp({0x00U}),
                                                                           "truncated.bsp");
    assert(result);
    assert(!result->asset.visibility.available);

    bool found_invalid_visibility = false;
    for (const auto& diagnostic : result->report.diagnostics) {
        found_invalid_visibility = found_invalid_visibility ||
                                   diagnostic.code == stellar::import::bsp::DiagnosticCode::kInvalidVisibilityData;
    }
    assert(found_invalid_visibility);

    auto visible = stellar::assets::visible_level_surfaces_from_leaf(result->asset, 0);
    assert(visible.size() == 2);
    assert(visible[0]);
    assert(visible[1]);
}

void missing_pvs_falls_back_to_all_surfaces() {
    auto result = stellar::import::bsp::load_level_from_memory(two_leaf_bsp({}), "missing_pvs.bsp");
    assert(result);
    assert(!result->visibility.available);
    assert(result->visibility.leaves.size() == 2);

    const auto visible = stellar::assets::visible_level_surfaces_from_leaf(*result, 0);
    assert(visible.size() == 2);
    assert(visible[0]);
    assert(visible[1]);
}

} // namespace

int main() {
    two_leaf_pvs_fixture_imports_and_queries_visibility();
    marksurfaces_map_to_level_surface_indices_after_skipped_faces();
    zero_run_decompression_hides_later_surfaces();
    truncated_pvs_is_diagnosed_and_falls_back();
    missing_pvs_falls_back_to_all_surfaces();
    return 0;
}
