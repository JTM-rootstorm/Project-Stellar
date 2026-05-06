#include "stellar/graphics/metal/MetalGraphicsDevice.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace stellar::graphics::metal {
namespace {

constexpr const char* kMetalShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct StaticVertex {
    packed_float3 position;
    packed_float3 normal;
    packed_float2 uv0;
    packed_float4 tangent;
    packed_float2 uv1;
    packed_float4 color;
};

struct DrawUniforms {
    float4x4 mvp;
    float4 base_color;
    uint has_base_color_texture;
    uint has_vertex_color;
};

struct VertexOut {
    float4 position [[position]];
    float2 uv0;
    float4 color;
};

vertex VertexOut stellar_vertex(const device StaticVertex* vertices [[buffer(0)]],
                                constant DrawUniforms& uniforms [[buffer(1)]],
                                uint vertex_id [[vertex_id]]) {
    const StaticVertex vtx = vertices[vertex_id];
    VertexOut out;
    out.position = uniforms.mvp * float4(vtx.position, 1.0);
    out.uv0 = float2(vtx.uv0);
    out.color = uniforms.has_vertex_color != 0 ? float4(vtx.color) : float4(1.0);
    return out;
}

fragment float4 stellar_fragment(VertexOut in [[stage_in]],
                                 constant DrawUniforms& uniforms [[buffer(1)]],
                                 texture2d<float> base_texture [[texture(0)]],
                                 sampler base_sampler [[sampler(0)]]) {
    float4 color = uniforms.base_color * in.color;
    if (uniforms.has_base_color_texture != 0) {
        color *= base_texture.sample(base_sampler, in.uv0);
    }
    if (color.a < 0.01) {
        discard_fragment();
    }
    return color;
}
)";

struct DrawUniforms {
    std::array<float, 16> mvp{};
    std::array<float, 4> base_color{1.0F, 1.0F, 1.0F, 1.0F};
    std::uint32_t has_base_color_texture = 0;
    std::uint32_t has_vertex_color = 0;
};

struct MeshPrimitiveRecord {
    id<MTLBuffer> vertex_buffer = nil;
    id<MTLBuffer> index_buffer = nil;
    NSUInteger index_count = 0;
    bool has_colors = false;
};

struct MeshRecord {
    std::vector<MeshPrimitiveRecord> primitives;
};

struct TextureRecord {
    id<MTLTexture> texture = nil;
};

struct MaterialRecord {
    MaterialUpload upload;
};

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

[[nodiscard]] MTLPixelFormat pixel_format_for(const TextureUpload& texture) noexcept {
    switch (texture.image.format) {
    case stellar::assets::ImageFormat::kR8G8B8A8:
        return texture.color_space == stellar::assets::TextureColorSpace::kSrgb
                   ? MTLPixelFormatRGBA8Unorm_sRGB
                   : MTLPixelFormatRGBA8Unorm;
    case stellar::assets::ImageFormat::kR8G8B8:
        return texture.color_space == stellar::assets::TextureColorSpace::kSrgb
                   ? MTLPixelFormatRGBA8Unorm_sRGB
                   : MTLPixelFormatRGBA8Unorm;
    case stellar::assets::ImageFormat::kUnknown:
        break;
    }
    return MTLPixelFormatInvalid;
}

[[nodiscard]] MTLSamplerMinMagFilter min_mag_filter(
    stellar::assets::TextureFilter filter) noexcept {
    switch (filter) {
    case stellar::assets::TextureFilter::kNearest:
    case stellar::assets::TextureFilter::kNearestMipmapNearest:
    case stellar::assets::TextureFilter::kNearestMipmapLinear:
        return MTLSamplerMinMagFilterNearest;
    case stellar::assets::TextureFilter::kLinear:
    case stellar::assets::TextureFilter::kLinearMipmapNearest:
    case stellar::assets::TextureFilter::kLinearMipmapLinear:
    default:
        return MTLSamplerMinMagFilterLinear;
    }
}

[[nodiscard]] MTLSamplerAddressMode address_mode(
    stellar::assets::TextureWrapMode mode) noexcept {
    switch (mode) {
    case stellar::assets::TextureWrapMode::kClampToEdge:
        return MTLSamplerAddressModeClampToEdge;
    case stellar::assets::TextureWrapMode::kMirroredRepeat:
        return MTLSamplerAddressModeMirrorRepeat;
    case stellar::assets::TextureWrapMode::kRepeat:
    default:
        return MTLSamplerAddressModeRepeat;
    }
}

[[nodiscard]] std::vector<std::uint8_t> rgba_pixels_for(const TextureUpload& texture) {
    if (texture.image.format == stellar::assets::ImageFormat::kR8G8B8A8) {
        return texture.image.pixels;
    }

    std::vector<std::uint8_t> pixels;
    pixels.reserve(texture.image.width * texture.image.height * 4);
    for (std::size_t index = 0; index + 2 < texture.image.pixels.size(); index += 3) {
        pixels.push_back(texture.image.pixels[index + 0]);
        pixels.push_back(texture.image.pixels[index + 1]);
        pixels.push_back(texture.image.pixels[index + 2]);
        pixels.push_back(255);
    }
    return pixels;
}

[[nodiscard]] bool env_flag_enabled(const char* name) noexcept {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    const std::string_view text(value);
    return text == "1" || text == "true" || text == "TRUE" || text == "on" ||
           text == "ON";
}

[[nodiscard]] std::size_t env_frame_limit(const char* name, std::size_t fallback) noexcept {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value) {
        return fallback;
    }
    return static_cast<std::size_t>(parsed);
}

} // namespace

struct MetalGraphicsDevice::Impl {
    SDL_Window* window = nullptr;
    SDL_MetalView metal_view = nullptr;
    CAMetalLayer* layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;
    id<MTLLibrary> library = nil;
    id<MTLRenderPipelineState> opaque_pipeline = nil;
    id<MTLRenderPipelineState> blend_pipeline = nil;
    id<MTLDepthStencilState> depth_state = nil;
    id<MTLTexture> depth_texture = nil;
    id<CAMetalDrawable> drawable = nil;
    id<MTLCommandBuffer> command_buffer = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
    std::unordered_map<std::uint64_t, MeshRecord> meshes;
    std::unordered_map<std::uint64_t, TextureRecord> textures;
    std::unordered_map<std::uint64_t, MaterialRecord> materials;
    std::uint64_t next_handle = 1;
    int depth_width = 0;
    int depth_height = 0;
    bool debug_render_enabled = false;
    std::size_t debug_render_frame_limit = 0;
    std::size_t debug_render_frame_index = 0;

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
    }
}

std::expected<void, stellar::platform::Error>
MetalGraphicsDevice::initialize(stellar::platform::Window& window) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->window = window.native_handle();
    if (impl_->window == nullptr) {
        return std::unexpected(error("Metal initialization failed: SDL window is null"));
    }

    impl_->metal_view = SDL_Metal_CreateView(impl_->window);
    if (impl_->metal_view == nullptr) {
        return std::unexpected(error(
            sdl_error_message("Metal initialization failed to create SDL Metal view")));
    }

    void* raw_layer = SDL_Metal_GetLayer(impl_->metal_view);
    if (raw_layer == nullptr) {
        return std::unexpected(error(
            sdl_error_message("Metal initialization failed to get CAMetalLayer")));
    }
    impl_->layer = (__bridge CAMetalLayer*)raw_layer;

    impl_->device = MTLCreateSystemDefaultDevice();
    if (impl_->device == nil) {
        return std::unexpected(
            error("Metal initialization failed: no default MTLDevice is available"));
    }

    impl_->layer.device = impl_->device;
    impl_->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    impl_->layer.framebufferOnly = YES;
    impl_->debug_render_enabled = env_flag_enabled("STELLAR_DEBUG_RENDER");
    impl_->debug_render_frame_limit =
        env_frame_limit("STELLAR_DEBUG_RENDER_FRAMES",
                        impl_->debug_render_enabled ? 3U : 0U);
    impl_->debug_render_frame_index = 0;

    NSError* library_error = nil;
    NSString* source = [NSString stringWithUTF8String:kMetalShaderSource];
    impl_->library = [impl_->device newLibraryWithSource:source options:nil error:&library_error];
    if (impl_->library == nil) {
        std::string message = "Metal initialization failed to compile shader library";
        if (library_error != nil && library_error.localizedDescription != nil) {
            message += ": ";
            message += [library_error.localizedDescription UTF8String];
        }
        return std::unexpected(error(std::move(message)));
    }

    MTLRenderPipelineDescriptor* opaque_descriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    opaque_descriptor.vertexFunction = [impl_->library newFunctionWithName:@"stellar_vertex"];
    opaque_descriptor.fragmentFunction = [impl_->library newFunctionWithName:@"stellar_fragment"];
    opaque_descriptor.colorAttachments[0].pixelFormat = impl_->layer.pixelFormat;
    opaque_descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

    NSError* pipeline_error = nil;
    impl_->opaque_pipeline =
        [impl_->device newRenderPipelineStateWithDescriptor:opaque_descriptor
                                                      error:&pipeline_error];
    if (impl_->opaque_pipeline == nil) {
        std::string message = "Metal initialization failed to create opaque pipeline";
        if (pipeline_error != nil && pipeline_error.localizedDescription != nil) {
            message += ": ";
            message += [pipeline_error.localizedDescription UTF8String];
        }
        return std::unexpected(error(std::move(message)));
    }

    MTLRenderPipelineDescriptor* blend_descriptor = [opaque_descriptor copy];
    blend_descriptor.colorAttachments[0].blendingEnabled = YES;
    blend_descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    blend_descriptor.colorAttachments[0].destinationRGBBlendFactor =
        MTLBlendFactorOneMinusSourceAlpha;
    blend_descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    blend_descriptor.colorAttachments[0].destinationAlphaBlendFactor =
        MTLBlendFactorOneMinusSourceAlpha;
    impl_->blend_pipeline =
        [impl_->device newRenderPipelineStateWithDescriptor:blend_descriptor
                                                      error:&pipeline_error];
    if (impl_->blend_pipeline == nil) {
        std::string message = "Metal initialization failed to create blend pipeline";
        if (pipeline_error != nil && pipeline_error.localizedDescription != nil) {
            message += ": ";
            message += [pipeline_error.localizedDescription UTF8String];
        }
        return std::unexpected(error(std::move(message)));
    }

    MTLDepthStencilDescriptor* depth_descriptor = [[MTLDepthStencilDescriptor alloc] init];
    depth_descriptor.depthCompareFunction = MTLCompareFunctionLess;
    depth_descriptor.depthWriteEnabled = YES;
    impl_->depth_state = [impl_->device newDepthStencilStateWithDescriptor:depth_descriptor];

    impl_->command_queue = [impl_->device newCommandQueue];
    if (impl_->command_queue == nil) {
        return std::unexpected(error("Metal initialization failed: could not create command queue"));
    }

    return {};
}

std::expected<MeshHandle, stellar::platform::Error>
MetalGraphicsDevice::create_mesh(const stellar::assets::MeshAsset& mesh) {
    if (impl_ == nullptr || impl_->device == nil) {
        return std::unexpected(error("Metal graphics device is not initialized"));
    }
    if (mesh.primitives.empty()) {
        return std::unexpected(error("Mesh has no primitives"));
    }

    MeshRecord record;
    record.primitives.reserve(mesh.primitives.size());
    for (const stellar::assets::MeshPrimitive& primitive : mesh.primitives) {
        if (primitive.topology != stellar::assets::PrimitiveTopology::kTriangles) {
            return std::unexpected(error("Unsupported mesh topology"));
        }
        if (primitive.vertices.empty() || primitive.indices.empty()) {
            return std::unexpected(error("Mesh primitive is empty"));
        }

        MeshPrimitiveRecord primitive_record;
        primitive_record.vertex_buffer =
            [impl_->device newBufferWithBytes:primitive.vertices.data()
                                       length:primitive.vertices.size() *
                                              sizeof(stellar::assets::StaticVertex)
                                      options:MTLResourceStorageModeShared];
        primitive_record.index_buffer =
            [impl_->device newBufferWithBytes:primitive.indices.data()
                                       length:primitive.indices.size() * sizeof(std::uint32_t)
                                      options:MTLResourceStorageModeShared];
        if (primitive_record.vertex_buffer == nil || primitive_record.index_buffer == nil) {
            return std::unexpected(error("Metal failed to allocate mesh buffers"));
        }
        primitive_record.index_count = static_cast<NSUInteger>(primitive.indices.size());
        primitive_record.has_colors = primitive.has_colors;
        record.primitives.push_back(std::move(primitive_record));
    }

    const std::uint64_t handle = impl_->allocate_handle();
    impl_->meshes.emplace(handle, std::move(record));
    return MeshHandle{.value = handle};
}

std::expected<TextureHandle, stellar::platform::Error>
MetalGraphicsDevice::create_texture(const TextureUpload& texture) {
    if (impl_ == nullptr || impl_->device == nil) {
        return std::unexpected(error("Metal graphics device is not initialized"));
    }
    if (texture.image.width == 0 || texture.image.height == 0 || texture.image.pixels.empty()) {
        return std::unexpected(error("Image asset is empty"));
    }

    const MTLPixelFormat pixel_format = pixel_format_for(texture);
    if (pixel_format == MTLPixelFormatInvalid) {
        return std::unexpected(error("Unsupported image format"));
    }

    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:pixel_format
                                     width:static_cast<NSUInteger>(texture.image.width)
                                    height:static_cast<NSUInteger>(texture.image.height)
                                 mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> metal_texture = [impl_->device newTextureWithDescriptor:descriptor];
    if (metal_texture == nil) {
        return std::unexpected(error("Metal failed to allocate texture"));
    }

    const std::vector<std::uint8_t> pixels = rgba_pixels_for(texture);
    const MTLRegion region = MTLRegionMake2D(0, 0, static_cast<NSUInteger>(texture.image.width),
                                             static_cast<NSUInteger>(texture.image.height));
    [metal_texture replaceRegion:region
                     mipmapLevel:0
                       withBytes:pixels.data()
                     bytesPerRow:static_cast<NSUInteger>(texture.image.width * 4)];

    const std::uint64_t handle = impl_->allocate_handle();
    impl_->textures.emplace(handle, TextureRecord{.texture = metal_texture});
    return TextureHandle{.value = handle};
}

std::expected<MaterialHandle, stellar::platform::Error>
MetalGraphicsDevice::create_material(const MaterialUpload& material) {
    if (impl_ == nullptr || impl_->device == nil) {
        return std::unexpected(error("Metal graphics device is not initialized"));
    }

    const std::uint64_t handle = impl_->allocate_handle();
    impl_->materials.emplace(handle, MaterialRecord{.upload = material});
    return MaterialHandle{.value = handle};
}

void MetalGraphicsDevice::begin_frame(int width, int height) noexcept {
    if (impl_ == nullptr || impl_->layer == nil || impl_->command_queue == nil) {
        return;
    }
    if (impl_->window != nullptr) {
        int drawable_width = width;
        int drawable_height = height;
        SDL_Metal_GetDrawableSize(impl_->window, &drawable_width, &drawable_height);
        width = drawable_width > 0 ? drawable_width : width;
        height = drawable_height > 0 ? drawable_height : height;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    impl_->layer.drawableSize =
        CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height));

    if (impl_->depth_texture == nil || impl_->depth_width != width ||
        impl_->depth_height != height) {
        MTLTextureDescriptor* depth_descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                         width:static_cast<NSUInteger>(width)
                                        height:static_cast<NSUInteger>(height)
                                     mipmapped:NO];
        depth_descriptor.usage = MTLTextureUsageRenderTarget;
        depth_descriptor.storageMode = MTLStorageModePrivate;
        impl_->depth_texture = [impl_->device newTextureWithDescriptor:depth_descriptor];
        impl_->depth_width = width;
        impl_->depth_height = height;
    }

    impl_->drawable = [impl_->layer nextDrawable];
    if (impl_->drawable == nil || impl_->depth_texture == nil) {
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
    pass.depthAttachment.texture = impl_->depth_texture;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    pass.depthAttachment.clearDepth = 1.0;

    impl_->encoder = [impl_->command_buffer renderCommandEncoderWithDescriptor:pass];
    if (impl_->encoder != nil) {
        [impl_->encoder setDepthStencilState:impl_->depth_state];
        const MTLViewport viewport{0.0,
                                   0.0,
                                   static_cast<double>(width),
                                   static_cast<double>(height),
                                   0.0,
                                   1.0};
        [impl_->encoder setViewport:viewport];
    }
    if (impl_->debug_render_enabled &&
        impl_->debug_render_frame_index < impl_->debug_render_frame_limit) {
        const CGSize drawable_size = impl_->layer.drawableSize;
        std::fprintf(stderr,
                     "[stellar][metal] frame=%zu drawable=%dx%d layer_drawable=%.0fx%.0f "
                     "depth=%dx%d viewport=%dx%d projection=metal_ndc_z_zero_to_one\n",
                     impl_->debug_render_frame_index, width, height,
                     static_cast<double>(drawable_size.width),
                     static_cast<double>(drawable_size.height), impl_->depth_width,
                     impl_->depth_height, width, height);
    }
    ++impl_->debug_render_frame_index;
}

void MetalGraphicsDevice::draw_mesh(MeshHandle mesh,
                                    std::span<const MeshPrimitiveDrawCommand> commands,
                                    const MeshDrawTransforms& transforms) noexcept {
    if (impl_ == nullptr || impl_->encoder == nil || impl_->opaque_pipeline == nil) {
        return;
    }
    const auto mesh_it = impl_->meshes.find(mesh.value);
    if (mesh_it == impl_->meshes.end()) {
        return;
    }

    for (const MeshPrimitiveDrawCommand& command : commands) {
        if (command.primitive_index >= mesh_it->second.primitives.size()) {
            continue;
        }
        const MeshPrimitiveRecord& primitive =
            mesh_it->second.primitives[command.primitive_index];

        const auto material_it = impl_->materials.find(command.material.value);
        const MaterialRecord* material =
            material_it != impl_->materials.end() ? &material_it->second : nullptr;
        const std::array<float, 4> fallback_color{0.85F, 0.9F, 1.0F, 1.0F};
        DrawUniforms uniforms;
        uniforms.mvp = transforms.mvp;
        uniforms.base_color = material != nullptr
                                  ? material->upload.material.base_color_factor
                                  : fallback_color;
        uniforms.has_vertex_color = primitive.has_colors ? 1U : 0U;

        id<MTLTexture> base_texture = nil;
        id<MTLSamplerState> sampler_state = nil;
        if (material != nullptr && material->upload.base_color_texture.has_value()) {
            const auto texture_it = impl_->textures.find(
                material->upload.base_color_texture->texture.value);
            if (texture_it != impl_->textures.end()) {
                base_texture = texture_it->second.texture;
                uniforms.has_base_color_texture = 1U;

                MTLSamplerDescriptor* sampler_descriptor = [[MTLSamplerDescriptor alloc] init];
                const stellar::assets::SamplerAsset& sampler =
                    material->upload.base_color_texture->sampler;
                sampler_descriptor.minFilter = min_mag_filter(sampler.min_filter);
                sampler_descriptor.magFilter = min_mag_filter(sampler.mag_filter);
                sampler_descriptor.sAddressMode = address_mode(sampler.wrap_s);
                sampler_descriptor.tAddressMode = address_mode(sampler.wrap_t);
                sampler_state = [impl_->device newSamplerStateWithDescriptor:sampler_descriptor];
            }
        }

        const bool alpha_blend =
            material != nullptr &&
            material->upload.material.alpha_mode == stellar::assets::AlphaMode::kBlend;
        [impl_->encoder setRenderPipelineState:alpha_blend ? impl_->blend_pipeline
                                                           : impl_->opaque_pipeline];
        [impl_->encoder setVertexBuffer:primitive.vertex_buffer offset:0 atIndex:0];
        [impl_->encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
        [impl_->encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
        if (base_texture != nil && sampler_state != nil) {
            [impl_->encoder setFragmentTexture:base_texture atIndex:0];
            [impl_->encoder setFragmentSamplerState:sampler_state atIndex:0];
        }
        [impl_->encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                   indexCount:primitive.index_count
                                    indexType:MTLIndexTypeUInt32
                                  indexBuffer:primitive.index_buffer
                            indexBufferOffset:0];
    }
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
