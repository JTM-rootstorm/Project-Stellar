#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "stellar/graphics/GraphicsDevice.hpp"

namespace stellar::graphics::testing {

struct RecordedPrimitiveDraw {
    std::size_t primitive_index = 0;
    MaterialHandle material;
    std::vector<std::array<float, 16>> skin_joint_matrices;
};

struct RecordedDrawCall {
    MeshHandle mesh;
    std::vector<RecordedPrimitiveDraw> commands;
    MeshDrawTransforms transforms;
};

class RecordingGraphicsDevice final : public GraphicsDevice {
public:
    std::expected<void, stellar::platform::Error>
    initialize(stellar::platform::Window& /*window*/) override {
        initialized = true;
        return {};
    }

    std::expected<MeshHandle, stellar::platform::Error>
    create_mesh(const stellar::assets::MeshAsset& mesh) override {
        uploaded_meshes.push_back(mesh);
        MeshHandle handle{next_handle_++};
        mesh_handles.push_back(handle);
        return handle;
    }

    std::expected<TextureHandle, stellar::platform::Error>
    create_texture(const TextureUpload& texture) override {
        uploaded_textures.push_back(texture);
        TextureHandle handle{next_handle_++};
        texture_handles.push_back(handle);
        return handle;
    }

    std::expected<MaterialHandle, stellar::platform::Error>
    create_material(const MaterialUpload& material) override {
        uploaded_materials.push_back(material);
        MaterialHandle handle{next_handle_++};
        material_handles.push_back(handle);
        return handle;
    }

    void begin_frame(int width, int height) noexcept override {
        began_frames.push_back({width, height});
    }

    void draw_mesh(MeshHandle mesh,
                   std::span<const MeshPrimitiveDrawCommand> commands,
                   const MeshDrawTransforms& transforms) noexcept override {
        RecordedDrawCall call;
        call.mesh = mesh;
        call.transforms = transforms;
        call.commands.reserve(commands.size());
        for (const auto& command : commands) {
            RecordedPrimitiveDraw recorded;
            recorded.primitive_index = command.primitive_index;
            recorded.material = command.material;
            recorded.skin_joint_matrices.assign(command.skin_joint_matrices.begin(),
                                                command.skin_joint_matrices.end());
            call.commands.push_back(std::move(recorded));
        }
        draw_calls.push_back(std::move(call));
    }

    void end_frame() noexcept override {
        ++ended_frame_count;
    }

    void destroy_mesh(MeshHandle mesh) noexcept override {
        destroyed_meshes.push_back(mesh);
    }

    void destroy_texture(TextureHandle texture) noexcept override {
        destroyed_textures.push_back(texture);
    }

    void destroy_material(MaterialHandle material) noexcept override {
        destroyed_materials.push_back(material);
    }

    [[nodiscard]] const RecordedPrimitiveDraw& primitive_draw(std::size_t draw_index) const {
        return draw_calls[draw_index].commands[0];
    }

    bool initialized = false;
    std::vector<std::array<int, 2>> began_frames;
    int ended_frame_count = 0;
    std::vector<stellar::assets::MeshAsset> uploaded_meshes;
    std::vector<TextureUpload> uploaded_textures;
    std::vector<MaterialUpload> uploaded_materials;
    std::vector<MeshHandle> mesh_handles;
    std::vector<TextureHandle> texture_handles;
    std::vector<MaterialHandle> material_handles;
    std::vector<RecordedDrawCall> draw_calls;
    std::vector<MeshHandle> destroyed_meshes;
    std::vector<TextureHandle> destroyed_textures;
    std::vector<MaterialHandle> destroyed_materials;

private:
    std::uint64_t next_handle_ = 1;
};

} // namespace stellar::graphics::testing
