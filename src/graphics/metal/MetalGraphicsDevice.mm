#include "stellar/graphics/metal/MetalGraphicsDevice.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

#include <cstdint>
#include <expected>
#include <string>
#include <unordered_set>
#include <utility>

namespace stellar::graphics::metal {
namespace {

[[nodiscard]] stellar::platform::Error error(std::string message) {
    return stellar::platform::Error(std::move(message));
}

[[nodiscard]] std::string sdl_error_message(std::string prefix) {
    const char* sdl_error = SDL_GetError();
    if (sdl_error != nullptr && sdl_error[0] != '\0') {
        prefix += ": ";
        prefix += sdl_error;
    }
    return prefix;
}

} // namespace

struct MetalGraphicsDevice::Impl {
    SDL_MetalView metal_view = nullptr;
    CAMetalLayer* layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;
    id<CAMetalDrawable> drawable = nil;
    id<MTLCommandBuffer> command_buffer = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
    std::unordered_set<std::uint64_t> meshes;
    std::unordered_set<std::uint64_t> textures;
    std::unordered_set<std::uint64_t> materials;
    std::uint64_t next_handle = 1;

    [[nodiscard]] std::uint64_t allocate_handle() noexcept {
        return next_handle++;
    }
};

MetalGraphicsDevice::MetalGraphicsDevice() : impl_(std::make_unique<Impl>()) {}

MetalGraphicsDevice::~MetalGraphicsDevice() noexcept {
    if (impl_ != nullptr) {
        end_frame();
        if (impl_->metal_view != nullptr) {
            SDL_Metal_DestroyView(impl_->metal_view);
            impl_->metal_view = nullptr;
        }
        impl_->layer = nil;
        impl_->command_queue = nil;
        impl_->device = nil;
    }
}

std::expected<void, stellar::platform::Error>
MetalGraphicsDevice::initialize(stellar::platform::Window& window) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    SDL_Window* sdl_window = window.native_handle();
    if (sdl_window == nullptr) {
        return std::unexpected(error("Metal initialization failed: SDL window is null"));
    }

    impl_->metal_view = SDL_Metal_CreateView(sdl_window);
    if (impl_->metal_view == nullptr) {
        return std::unexpected(error(sdl_error_message("Metal initialization failed to create SDL Metal view")));
    }

    void* raw_layer = SDL_Metal_GetLayer(impl_->metal_view);
    if (raw_layer == nullptr) {
        return std::unexpected(error(sdl_error_message("Metal initialization failed to get CAMetalLayer")));
    }
    impl_->layer = (__bridge CAMetalLayer*)raw_layer;

    impl_->device = MTLCreateSystemDefaultDevice();
    if (impl_->device == nil) {
        return std::unexpected(error("Metal initialization failed: no default MTLDevice is available"));
    }

    impl_->layer.device = impl_->device;
    impl_->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    impl_->layer.framebufferOnly = YES;

    int drawable_width = 0;
    int drawable_height = 0;
    SDL_Metal_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
    if (drawable_width > 0 && drawable_height > 0) {
        impl_->layer.drawableSize =
            CGSizeMake(static_cast<CGFloat>(drawable_width), static_cast<CGFloat>(drawable_height));
    }

    impl_->command_queue = [impl_->device newCommandQueue];
    if (impl_->command_queue == nil) {
        return std::unexpected(error("Metal initialization failed: could not create command queue"));
    }

    return {};
}

std::expected<MeshHandle, stellar::platform::Error>
MetalGraphicsDevice::create_mesh(const stellar::assets::MeshAsset& mesh) {
    (void)mesh;
    const std::uint64_t handle = impl_->allocate_handle();
    impl_->meshes.insert(handle);
    return MeshHandle{.value = handle};
}

std::expected<TextureHandle, stellar::platform::Error>
MetalGraphicsDevice::create_texture(const TextureUpload& texture) {
    (void)texture;
    const std::uint64_t handle = impl_->allocate_handle();
    impl_->textures.insert(handle);
    return TextureHandle{.value = handle};
}

std::expected<MaterialHandle, stellar::platform::Error>
MetalGraphicsDevice::create_material(const MaterialUpload& material) {
    (void)material;
    const std::uint64_t handle = impl_->allocate_handle();
    impl_->materials.insert(handle);
    return MaterialHandle{.value = handle};
}

void MetalGraphicsDevice::begin_frame(int width, int height) noexcept {
    if (impl_ == nullptr || impl_->layer == nil || impl_->command_queue == nil) {
        return;
    }
    if (width > 0 && height > 0) {
        impl_->layer.drawableSize =
            CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height));
    }

    impl_->drawable = [impl_->layer nextDrawable];
    if (impl_->drawable == nil) {
        return;
    }
    impl_->command_buffer = [impl_->command_queue commandBuffer];
    if (impl_->command_buffer == nil) {
        impl_->drawable = nil;
        return;
    }

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = impl_->drawable.texture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    impl_->encoder = [impl_->command_buffer renderCommandEncoderWithDescriptor:pass];
}

void MetalGraphicsDevice::draw_mesh(MeshHandle mesh,
                                    std::span<const MeshPrimitiveDrawCommand> commands,
                                    const MeshDrawTransforms& transforms) noexcept {
    (void)mesh;
    (void)commands;
    (void)transforms;
}

void MetalGraphicsDevice::end_frame() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->encoder != nil) {
        [impl_->encoder endEncoding];
        impl_->encoder = nil;
    }
    if (impl_->command_buffer != nil && impl_->drawable != nil) {
        [impl_->command_buffer presentDrawable:impl_->drawable];
        [impl_->command_buffer commit];
    }
    impl_->command_buffer = nil;
    impl_->drawable = nil;
}

void MetalGraphicsDevice::destroy_mesh(MeshHandle mesh) noexcept {
    if (impl_ != nullptr) {
        impl_->meshes.erase(mesh.value);
    }
}

void MetalGraphicsDevice::destroy_texture(TextureHandle texture) noexcept {
    if (impl_ != nullptr) {
        impl_->textures.erase(texture.value);
    }
}

void MetalGraphicsDevice::destroy_material(MaterialHandle material) noexcept {
    if (impl_ != nullptr) {
        impl_->materials.erase(material.value);
    }
}

} // namespace stellar::graphics::metal
