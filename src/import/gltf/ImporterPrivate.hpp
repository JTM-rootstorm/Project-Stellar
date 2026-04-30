#pragma once

#include "stellar/assets/SceneAsset.hpp"
#include "stellar/platform/Error.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <cgltf.h>

namespace stellar::import::gltf {

struct CgltfDataDeleter {
    void operator()(cgltf_data* data) const noexcept;
};

using CgltfDataPtr = std::unique_ptr<cgltf_data, CgltfDataDeleter>;

[[nodiscard]] std::expected<stellar::assets::SceneAsset, stellar::platform::Error>
load_scene_from_file(std::string_view path);

std::string duplicate_to_string(const char* value);
std::filesystem::path resolve_relative_path(const std::filesystem::path& base, const char* relative);

[[nodiscard]] std::expected<std::size_t, stellar::platform::Error>
checked_index(cgltf_size value, const char* error_message);
[[nodiscard]] std::unexpected<stellar::platform::Error> import_error(const char* message);
[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_required_extensions(const cgltf_data* data);
[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_float_accessor(const cgltf_accessor* accessor, cgltf_type type, cgltf_size expected_count,
                        const char* label);
[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_normalized_float_attribute_accessor(const cgltf_accessor* accessor, cgltf_type type,
                                             cgltf_size expected_count, const char* label);
[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_index_accessor(const cgltf_accessor* accessor);
[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_joints_accessor(const cgltf_accessor* accessor, cgltf_size expected_count);
[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_weights_accessor(const cgltf_accessor* accessor, cgltf_size expected_count);

[[nodiscard]] std::expected<std::array<float, 2>, stellar::platform::Error>
read_vec2(const cgltf_accessor* accessor, std::size_t index, const char* error_message);
[[nodiscard]] std::expected<std::array<float, 3>, stellar::platform::Error>
read_vec3(const cgltf_accessor* accessor, std::size_t index, const char* error_message);
[[nodiscard]] std::expected<std::array<float, 4>, stellar::platform::Error>
read_vec4(const cgltf_accessor* accessor, std::size_t index, const char* error_message);
[[nodiscard]] std::expected<std::array<std::uint16_t, 4>, stellar::platform::Error>
read_u16_vec4(const cgltf_accessor* accessor, std::size_t index, const char* error_message);
[[nodiscard]] std::expected<std::array<float, 16>, stellar::platform::Error>
read_mat4(const cgltf_accessor* accessor, std::size_t index, const char* error_message);

bool normalize_weights(std::array<float, 4>& weights) noexcept;
std::array<float, 16> identity_matrix() noexcept;
std::array<float, 16> node_matrix(const cgltf_node& node);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, stellar::platform::Error>
decode_data_uri(std::string_view uri);

[[nodiscard]] std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
load_image_from_file(const std::filesystem::path& path, const char* name);
[[nodiscard]] std::expected<stellar::assets::ImageAsset, stellar::platform::Error>
load_image_from_memory(std::span<const std::uint8_t> bytes, const char* name,
                       std::string source_uri, const char* error_message);

[[nodiscard]] std::expected<stellar::assets::MaterialAsset, stellar::platform::Error>
load_material(const cgltf_data* data, const cgltf_material* material);
[[nodiscard]] std::expected<stellar::assets::SamplerAsset, stellar::platform::Error>
load_sampler(const cgltf_sampler& sampler);
[[nodiscard]] std::expected<stellar::assets::TextureAsset, stellar::platform::Error>
load_texture(const cgltf_data* data, const cgltf_texture& texture);
void classify_texture_color_spaces(stellar::assets::SceneAsset& scene) noexcept;

[[nodiscard]] std::expected<stellar::assets::MeshAsset, stellar::platform::Error>
load_mesh(const cgltf_data* data, const cgltf_mesh& source_mesh);
[[nodiscard]] std::expected<stellar::assets::SkinAsset, stellar::platform::Error>
load_skin(const cgltf_data* data, const cgltf_skin& source_skin);
[[nodiscard]] std::expected<stellar::assets::AnimationAsset, stellar::platform::Error>
load_animation(const cgltf_data* data, const cgltf_animation& source_animation);
[[nodiscard]] std::expected<stellar::scene::Node, stellar::platform::Error>
load_node(const cgltf_data* data, const cgltf_node& node);

[[nodiscard]] std::expected<void, stellar::platform::Error>
validate_skinned_mesh_joint_indices(const stellar::assets::SceneAsset& scene);
[[nodiscard]] std::expected<stellar::assets::LevelCollisionAsset, stellar::platform::Error>
extract_level_collision(const stellar::assets::SceneAsset& scene);

} // namespace stellar::import::gltf
