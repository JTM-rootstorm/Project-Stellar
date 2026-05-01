#pragma once

#include "../../../src/import/bsp/BspBinary.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace stellar::tests::bsp_fixture {

using stellar::import::bsp::detail::LumpIndex;

template <typename T> void append(std::vector<std::byte> &bytes, T value) {
    const auto *raw = reinterpret_cast<const std::byte *>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

inline void patch_i32(std::vector<std::byte> &bytes, std::size_t offset, std::int32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

inline std::size_t lump_header_offset(LumpIndex lump) {
    return 4 + static_cast<std::size_t>(lump) * 8;
}

inline void set_lump(std::vector<std::byte> &bytes, LumpIndex lump, std::size_t offset,
                     std::size_t size) {
    patch_i32(bytes, lump_header_offset(lump), static_cast<std::int32_t>(offset));
    patch_i32(bytes, lump_header_offset(lump) + 4, static_cast<std::int32_t>(size));
}

inline void append_vec3(std::vector<std::byte> &bytes, float x, float y, float z) {
    append(bytes, x);
    append(bytes, y);
    append(bytes, z);
}

inline void append_basic_lumps(std::vector<std::byte> &bytes, std::int32_t light_offset = -1) {
    const std::string entities = "{\n\"classname\" \"worldspawn\"\n}\n";
    const std::size_t entity_offset = bytes.size();
    bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(entities.data()),
                 reinterpret_cast<const std::byte *>(entities.data() + entities.size()));
    set_lump(bytes, LumpIndex::kEntities, entity_offset, entities.size());

    const std::size_t plane_offset = bytes.size();
    append_vec3(bytes, 0.0F, 0.0F, 1.0F);
    append(bytes, 0.0F);
    append<std::int32_t>(bytes, 0);
    set_lump(bytes, LumpIndex::kPlanes, plane_offset, 20);

    const std::size_t vertex_offset = bytes.size();
    append_vec3(bytes, 0.0F, 0.0F, 0.0F);
    append_vec3(bytes, 16.0F, 0.0F, 0.0F);
    append_vec3(bytes, 16.0F, 16.0F, 0.0F);
    append_vec3(bytes, 0.0F, 16.0F, 0.0F);
    set_lump(bytes, LumpIndex::kVertices, vertex_offset, 4 * 12);

    const std::size_t texinfo_offset = bytes.size();
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
    set_lump(bytes, LumpIndex::kTexinfo, texinfo_offset, 40);

    const std::size_t face_offset = bytes.size();
    append<std::uint16_t>(bytes, 0);
    append<std::uint16_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::uint16_t>(bytes, 4);
    append<std::uint16_t>(bytes, 0);
    append<std::uint8_t>(bytes, 0);
    append<std::uint8_t>(bytes, 255);
    append<std::uint8_t>(bytes, 255);
    append<std::uint8_t>(bytes, 255);
    append<std::int32_t>(bytes, light_offset);
    set_lump(bytes, LumpIndex::kFaces, face_offset, 20);

    const std::size_t edge_offset = bytes.size();
    append<std::uint16_t>(bytes, 0);
    append<std::uint16_t>(bytes, 1);
    append<std::uint16_t>(bytes, 1);
    append<std::uint16_t>(bytes, 2);
    append<std::uint16_t>(bytes, 2);
    append<std::uint16_t>(bytes, 3);
    append<std::uint16_t>(bytes, 3);
    append<std::uint16_t>(bytes, 0);
    set_lump(bytes, LumpIndex::kEdges, edge_offset, 16);

    const std::size_t surfedge_offset = bytes.size();
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 1);
    append<std::int32_t>(bytes, 2);
    append<std::int32_t>(bytes, 3);
    set_lump(bytes, LumpIndex::kSurfedges, surfedge_offset, 16);

    const std::size_t model_offset = bytes.size();
    append_vec3(bytes, 0.0F, 0.0F, 0.0F);
    append_vec3(bytes, 16.0F, 16.0F, 0.0F);
    append_vec3(bytes, 0.0F, 0.0F, 0.0F);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 0);
    append<std::int32_t>(bytes, 1);
    set_lump(bytes, LumpIndex::kModels, model_offset, 64);
}

inline std::vector<std::byte> single_face_bsp(bool embedded_texture,
                                              std::int32_t light_offset = -1,
                                              std::size_t lighting_bytes = 0,
                                              std::string texture_name = "stone") {
    std::vector<std::byte> bytes(4 + 15 * 8);
    patch_i32(bytes, 0, 29);

    const std::size_t texture_offset = bytes.size();
    append<std::int32_t>(bytes, 1);
    append<std::int32_t>(bytes, 8);
    std::array<char, 16> name{};
    const std::size_t copy_size = std::min(texture_name.size(), name.size());
    std::memcpy(name.data(), texture_name.data(), copy_size);
    bytes.insert(bytes.end(), reinterpret_cast<const std::byte *>(name.data()),
                 reinterpret_cast<const std::byte *>(name.data() + name.size()));
    append<std::uint32_t>(bytes, 2);
    append<std::uint32_t>(bytes, 2);
    append<std::uint32_t>(bytes, embedded_texture ? 40U : 0U);
    append<std::uint32_t>(bytes, 0U);
    append<std::uint32_t>(bytes, 0U);
    append<std::uint32_t>(bytes, 0U);
    if (embedded_texture) {
        append<std::uint8_t>(bytes, 1U);
        append<std::uint8_t>(bytes, 2U);
        append<std::uint8_t>(bytes, 3U);
        append<std::uint8_t>(bytes, 4U);
    }
    set_lump(bytes, LumpIndex::kTextures, texture_offset, bytes.size() - texture_offset);

    append_basic_lumps(bytes, light_offset);

    if (lighting_bytes > 0) {
        const std::size_t lighting_offset = bytes.size();
        for (std::size_t i = 0; i < lighting_bytes; ++i) {
            append<std::uint8_t>(bytes, static_cast<std::uint8_t>(i & 0xFFU));
        }
        set_lump(bytes, LumpIndex::kLighting, lighting_offset, lighting_bytes);
    }
    return bytes;
}

} // namespace stellar::tests::bsp_fixture
