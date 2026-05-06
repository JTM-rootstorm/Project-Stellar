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
    float4x4 model;
    float4 normal_column0;
    float4 normal_column1;
    float4 normal_column2;
    float4 base_color;
    float4 emissive_factor;
    float4 global_light_color;
    float4 camera_world_position;
    float4 key_light_direction;
    float4 texture_transform0[6];
    float4 texture_transform1[6];
    float metallic_factor;
    float roughness_factor;
    float normal_scale;
    float normal_light_strength;
    float specular_factor;
    float specular_power;
    float occlusion_strength;
    float lightmap_intensity;
    float lightmap_min;
    float global_light_intensity;
    float alpha_cutoff;
    uint has_base_color_texture;
    uint has_lightmap_texture;
    uint has_normal_texture;
    uint has_metallic_roughness_texture;
    uint has_occlusion_texture;
    uint has_emissive_texture;
    uint has_specular_texture;
    uint has_vertex_color;
    uint has_camera_world_position;
    uint base_color_texcoord_set;
    uint lightmap_texcoord_set;
    uint normal_texcoord_set;
    uint metallic_roughness_texcoord_set;
    uint occlusion_texcoord_set;
    uint emissive_texcoord_set;
    uint specular_texcoord_set;
    uint alpha_mode;
    uint unlit;
};

struct VertexOut {
    float4 position [[position]];
    float2 uv0;
    float2 uv1;
    float3 normal;
    float4 tangent;
    float4 color;
    float3 world_position;
};

vertex VertexOut stellar_vertex(const device StaticVertex* vertices [[buffer(0)]],
                                constant DrawUniforms& uniforms [[buffer(1)]],
                                uint vertex_id [[vertex_id]]) {
    const StaticVertex vtx = vertices[vertex_id];
    const float4 world_position = uniforms.model * float4(vtx.position, 1.0);
    const float3x3 normal_matrix = float3x3(uniforms.normal_column0.xyz,
                                           uniforms.normal_column1.xyz,
                                           uniforms.normal_column2.xyz);
    VertexOut out;
    out.position = uniforms.mvp * float4(vtx.position, 1.0);
    out.uv0 = float2(vtx.uv0);
    out.uv1 = float2(vtx.uv1);
    out.normal = normalize(normal_matrix * float3(vtx.normal));
    out.tangent = float4((float3x3(uniforms.model[0].xyz,
                                   uniforms.model[1].xyz,
                                   uniforms.model[2].xyz) * float3(vtx.tangent.xyz)),
                         vtx.tangent.w);
    out.color = uniforms.has_vertex_color != 0 ? float4(vtx.color) : float4(1.0);
    out.world_position = world_position.xyz;
    return out;
}

float2 uv_for_set(VertexOut in, uint texcoord_set) {
    return texcoord_set == 1 ? in.uv1 : in.uv0;
}

float2 transformed_uv_for_slot(VertexOut in,
                               constant DrawUniforms& uniforms,
                               uint texcoord_set,
                               uint slot) {
    float2 uv = uv_for_set(in, texcoord_set);
    if (uniforms.texture_transform0[slot].w == 0.0) {
        return uv;
    }

    const float2 scaled = uv * uniforms.texture_transform1[slot].xy;
    const float c = cos(uniforms.texture_transform0[slot].z);
    const float s = sin(uniforms.texture_transform0[slot].z);
    return uniforms.texture_transform0[slot].xy +
        float2(c * scaled.x - s * scaled.y, s * scaled.x + c * scaled.y);
}

fragment float4 stellar_fragment(VertexOut in [[stage_in]],
                                 constant DrawUniforms& uniforms [[buffer(1)]],
                                 texture2d<float> base_texture [[texture(0)]],
                                 texture2d<float> normal_texture [[texture(1)]],
                                 texture2d<float> metallic_roughness_texture [[texture(2)]],
                                 texture2d<float> occlusion_texture [[texture(3)]],
                                 texture2d<float> emissive_texture [[texture(4)]],
                                 texture2d<float> lightmap_texture [[texture(5)]],
                                 texture2d<float> specular_texture [[texture(6)]],
                                 sampler base_sampler [[sampler(0)]],
                                 sampler normal_sampler [[sampler(1)]],
                                 sampler metallic_roughness_sampler [[sampler(2)]],
                                 sampler occlusion_sampler [[sampler(3)]],
                                 sampler emissive_sampler [[sampler(4)]],
                                 sampler lightmap_sampler [[sampler(5)]],
                                 sampler specular_sampler [[sampler(6)]]) {
    float4 color = uniforms.base_color * in.color;
    if (uniforms.has_base_color_texture != 0) {
        color *= base_texture.sample(
            base_sampler,
            transformed_uv_for_slot(in, uniforms, uniforms.base_color_texcoord_set, 0));
    }
    if (uniforms.alpha_mode == 1 && color.a < uniforms.alpha_cutoff) {
        discard_fragment();
    }

    float metallic = uniforms.metallic_factor;
    float roughness = uniforms.roughness_factor;
    if (uniforms.has_metallic_roughness_texture != 0) {
        const float4 metallic_roughness = metallic_roughness_texture.sample(
            metallic_roughness_sampler,
            transformed_uv_for_slot(in, uniforms,
                                    uniforms.metallic_roughness_texcoord_set, 2));
        roughness *= metallic_roughness.g;
        metallic *= metallic_roughness.b;
    }

    float3 normal = normalize(in.normal);
    if (uniforms.has_normal_texture != 0) {
        const float3 tangent = normalize(in.tangent.xyz - normal * dot(in.tangent.xyz, normal));
        const float3 bitangent = normalize(cross(normal, tangent) * in.tangent.w);
        const float3x3 tbn = float3x3(tangent, bitangent, normal);
        float3 sampled_normal = normal_texture.sample(
            normal_sampler,
            transformed_uv_for_slot(in, uniforms, uniforms.normal_texcoord_set, 1)).xyz *
            2.0 - 1.0;
        sampled_normal.xy *= uniforms.normal_scale;
        normal = normalize(tbn * sampled_normal);
    }

    float3 emissive = uniforms.emissive_factor.xyz;
    if (uniforms.has_emissive_texture != 0) {
        emissive *= emissive_texture.sample(
            emissive_sampler,
            transformed_uv_for_slot(in, uniforms, uniforms.emissive_texcoord_set, 4)).rgb;
    }

    const float3 light_dir = normalize(uniforms.key_light_direction.xyz);
    const float detail_diffuse =
        max(dot(normal, light_dir), 0.0) * uniforms.normal_light_strength;
    float occlusion = 1.0;
    if (uniforms.has_occlusion_texture != 0) {
        const float sample = occlusion_texture.sample(
            occlusion_sampler,
            transformed_uv_for_slot(in, uniforms, uniforms.occlusion_texcoord_set, 3)).r;
        occlusion = mix(1.0, sample, uniforms.occlusion_strength);
    }
    const float specular_map = uniforms.has_specular_texture != 0
        ? specular_texture.sample(
              specular_sampler,
              transformed_uv_for_slot(in, uniforms, uniforms.specular_texcoord_set, 5)).r
        : 1.0;
    const float3 view_delta = uniforms.camera_world_position.xyz - in.world_position;
    const float3 view_dir =
        uniforms.has_camera_world_position != 0 && dot(view_delta, view_delta) > 0.000001
            ? normalize(view_delta)
            : normalize(float3(0.0, -1.0, 0.25));
    const float3 half_dir = normalize(light_dir + view_dir);
    const float effective_power = mix(max(uniforms.specular_power, 1.0), 4.0,
                                      clamp(roughness, 0.0, 1.0));
    const float specular = pow(max(dot(normal, half_dir), 0.0), effective_power) *
        uniforms.specular_factor * specular_map;

    if (uniforms.has_lightmap_texture != 0) {
        const float3 lightmap = lightmap_texture.sample(
            lightmap_sampler, uv_for_set(in, uniforms.lightmap_texcoord_set)).rgb;
        const float3 lighting = lightmap * uniforms.lightmap_intensity +
            uniforms.global_light_color.xyz * uniforms.global_light_intensity +
            float3(detail_diffuse);
        return float4(color.rgb * max(lighting, float3(uniforms.lightmap_min)) +
                          float3(specular) + emissive,
                      color.a);
    }
    if (uniforms.unlit != 0) {
        return float4(color.rgb + emissive, color.a);
    }

    const float diffuse = max(dot(normal, light_dir), 0.0);
    const float perceptual_roughness = clamp(roughness, 0.04, 1.0);
    const float metal_attenuation = mix(1.0, 0.45, clamp(metallic, 0.0, 1.0));
    const float lit = 0.18 + diffuse * metal_attenuation *
        mix(1.0, 0.65, perceptual_roughness);
    return float4(color.rgb * (lit + detail_diffuse) * occlusion +
                      float3(specular) + emissive,
                  color.a);
}
)";

struct DrawUniforms {
    std::array<float, 16> mvp{};
    std::array<float, 16> model{};
    std::array<float, 4> normal_column0{1.0F, 0.0F, 0.0F, 0.0F};
    std::array<float, 4> normal_column1{0.0F, 1.0F, 0.0F, 0.0F};
    std::array<float, 4> normal_column2{0.0F, 0.0F, 1.0F, 0.0F};
    std::array<float, 4> base_color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> emissive_factor{0.0F, 0.0F, 0.0F, 0.0F};
    std::array<float, 4> global_light_color{0.0F, 0.0F, 0.0F, 0.0F};
    std::array<float, 4> camera_world_position{0.0F, 0.0F, 0.0F, 0.0F};
    std::array<float, 4> key_light_direction{0.35F, 0.8F, 0.45F, 0.0F};
    std::array<std::array<float, 4>, 6> texture_transform0{};
    std::array<std::array<float, 4>, 6> texture_transform1{
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
        std::array<float, 4>{1.0F, 1.0F, 0.0F, 0.0F},
    };
    float metallic_factor = 0.0F;
    float roughness_factor = 1.0F;
    float normal_scale = 1.0F;
    float normal_light_strength = 0.0F;
    float specular_factor = 0.0F;
    float specular_power = 32.0F;
    float occlusion_strength = 1.0F;
    float lightmap_intensity = 1.0F;
    float lightmap_min = 0.0F;
    float global_light_intensity = 0.0F;
    float alpha_cutoff = 0.5F;
    std::uint32_t has_base_color_texture = 0;
    std::uint32_t has_lightmap_texture = 0;
    std::uint32_t has_normal_texture = 0;
    std::uint32_t has_metallic_roughness_texture = 0;
    std::uint32_t has_occlusion_texture = 0;
    std::uint32_t has_emissive_texture = 0;
    std::uint32_t has_specular_texture = 0;
    std::uint32_t has_vertex_color = 0;
    std::uint32_t has_camera_world_position = 0;
    std::uint32_t base_color_texcoord_set = 0;
    std::uint32_t lightmap_texcoord_set = 1;
    std::uint32_t normal_texcoord_set = 0;
    std::uint32_t metallic_roughness_texcoord_set = 0;
    std::uint32_t occlusion_texcoord_set = 0;
    std::uint32_t emissive_texcoord_set = 0;
    std::uint32_t specular_texcoord_set = 0;
    std::uint32_t alpha_mode = 0;
    std::uint32_t unlit = 0;
};

struct MeshPrimitiveRecord {
    id<MTLBuffer> vertex_buffer = nil;
    id<MTLBuffer> index_buffer = nil;
    NSUInteger index_count = 0;
    bool has_tangents = false;
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

[[nodiscard]] std::uint32_t alpha_mode(stellar::assets::AlphaMode mode) noexcept {
    switch (mode) {
    case stellar::assets::AlphaMode::kMask:
        return 1U;
    case stellar::assets::AlphaMode::kBlend:
        return 2U;
    case stellar::assets::AlphaMode::kOpaque:
    default:
        return 0U;
    }
}

[[nodiscard]] std::array<float, 4>
transform0_for(const std::optional<MaterialTextureBinding>& binding) noexcept {
    if (!binding.has_value() || !binding->transform.enabled) {
        return {0.0F, 0.0F, 0.0F, 0.0F};
    }
    return {binding->transform.offset[0], binding->transform.offset[1],
            binding->transform.rotation, 1.0F};
}

[[nodiscard]] std::array<float, 4>
transform1_for(const std::optional<MaterialTextureBinding>& binding) noexcept {
    if (!binding.has_value() || !binding->transform.enabled) {
        return {1.0F, 1.0F, 0.0F, 0.0F};
    }
    return {binding->transform.scale[0], binding->transform.scale[1], 0.0F, 0.0F};
}

[[nodiscard]] std::size_t sampler_key(const stellar::assets::SamplerAsset& sampler) noexcept {
    std::size_t key = static_cast<std::size_t>(sampler.min_filter);
    key = key * 31U + static_cast<std::size_t>(sampler.mag_filter);
    key = key * 31U + static_cast<std::size_t>(sampler.wrap_s);
    key = key * 31U + static_cast<std::size_t>(sampler.wrap_t);
    return key;
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
    std::unordered_map<std::size_t, id<MTLSamplerState>> samplers;
    std::uint64_t next_handle = 1;
    int depth_width = 0;
    int depth_height = 0;
    bool debug_render_enabled = false;
    std::size_t debug_render_frame_limit = 0;
    std::size_t debug_render_frame_index = 0;
    std::size_t debug_material_logs = 0;

    [[nodiscard]] std::uint64_t allocate_handle() noexcept {
        return next_handle++;
    }
};

MetalGraphicsDevice::MetalGraphicsDevice() : impl_(std::make_unique<Impl>()) {}

const char* metal_shader_source() noexcept {
    return kMetalShaderSource;
}

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
    NSString* source = [NSString stringWithUTF8String:metal_shader_source()];
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
        primitive_record.has_tangents = primitive.has_tangents;
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
        uniforms.model = transforms.world;
        uniforms.normal_column0 = {transforms.normal[0], transforms.normal[1],
                                   transforms.normal[2], 0.0F};
        uniforms.normal_column1 = {transforms.normal[3], transforms.normal[4],
                                   transforms.normal[5], 0.0F};
        uniforms.normal_column2 = {transforms.normal[6], transforms.normal[7],
                                   transforms.normal[8], 0.0F};
        uniforms.base_color = material != nullptr
                                  ? material->upload.material.base_color_factor
                                  : fallback_color;
        uniforms.has_vertex_color = primitive.has_colors ? 1U : 0U;
        uniforms.camera_world_position = {transforms.camera_world_position[0],
                                          transforms.camera_world_position[1],
                                          transforms.camera_world_position[2], 0.0F};
        uniforms.has_camera_world_position =
            transforms.has_camera_world_position ? 1U : 0U;

        auto bind_texture =
            [this](const std::optional<MaterialTextureBinding>& binding,
                   NSUInteger index) noexcept -> bool {
            if (!binding.has_value() || binding->texcoord_set > 1) {
                return false;
            }
            const auto texture_it = impl_->textures.find(binding->texture.value);
            if (texture_it == impl_->textures.end()) {
                return false;
            }
            const std::size_t key = sampler_key(binding->sampler);
            id<MTLSamplerState> sampler_state = nil;
            const auto sampler_it = impl_->samplers.find(key);
            if (sampler_it != impl_->samplers.end()) {
                sampler_state = sampler_it->second;
            } else {
                MTLSamplerDescriptor* sampler_descriptor = [[MTLSamplerDescriptor alloc] init];
                sampler_descriptor.minFilter = min_mag_filter(binding->sampler.min_filter);
                sampler_descriptor.magFilter = min_mag_filter(binding->sampler.mag_filter);
                sampler_descriptor.sAddressMode = address_mode(binding->sampler.wrap_s);
                sampler_descriptor.tAddressMode = address_mode(binding->sampler.wrap_t);
                sampler_state = [impl_->device newSamplerStateWithDescriptor:sampler_descriptor];
                if (sampler_state == nil) {
                    return false;
                }
                impl_->samplers.emplace(key, sampler_state);
            }
            [impl_->encoder setFragmentTexture:texture_it->second.texture atIndex:index];
            [impl_->encoder setFragmentSamplerState:sampler_state atIndex:index];
            return true;
        };

        if (material != nullptr) {
            const MaterialUpload& upload = material->upload;
            uniforms.has_base_color_texture =
                bind_texture(upload.base_color_texture, 0) ? 1U : 0U;
            uniforms.has_lightmap_texture =
                bind_texture(upload.lightmap_texture, 5) ? 1U : 0U;
            uniforms.has_normal_texture =
                primitive.has_tangents && bind_texture(upload.normal_texture, 1) ? 1U : 0U;
            uniforms.has_metallic_roughness_texture =
                bind_texture(upload.metallic_roughness_texture, 2) ? 1U : 0U;
            uniforms.has_occlusion_texture =
                bind_texture(upload.occlusion_texture, 3) ? 1U : 0U;
            uniforms.has_emissive_texture =
                bind_texture(upload.emissive_texture, 4) ? 1U : 0U;
            uniforms.has_specular_texture =
                bind_texture(upload.specular_texture, 6) ? 1U : 0U;
            uniforms.base_color_texcoord_set =
                uniforms.has_base_color_texture != 0U && upload.base_color_texture.has_value()
                    ? upload.base_color_texture->texcoord_set
                    : 0U;
            uniforms.lightmap_texcoord_set =
                uniforms.has_lightmap_texture != 0U && upload.lightmap_texture.has_value()
                    ? upload.lightmap_texture->texcoord_set
                    : 1U;
            uniforms.normal_texcoord_set =
                uniforms.has_normal_texture != 0U && upload.normal_texture.has_value()
                    ? upload.normal_texture->texcoord_set
                    : 0U;
            uniforms.metallic_roughness_texcoord_set =
                uniforms.has_metallic_roughness_texture != 0U &&
                        upload.metallic_roughness_texture.has_value()
                    ? upload.metallic_roughness_texture->texcoord_set
                    : 0U;
            uniforms.occlusion_texcoord_set =
                uniforms.has_occlusion_texture != 0U && upload.occlusion_texture.has_value()
                    ? upload.occlusion_texture->texcoord_set
                    : 0U;
            uniforms.emissive_texcoord_set =
                uniforms.has_emissive_texture != 0U && upload.emissive_texture.has_value()
                    ? upload.emissive_texture->texcoord_set
                    : 0U;
            uniforms.specular_texcoord_set =
                uniforms.has_specular_texture != 0U && upload.specular_texture.has_value()
                    ? upload.specular_texture->texcoord_set
                    : 0U;
            uniforms.metallic_factor = upload.material.metallic_factor;
            uniforms.roughness_factor = upload.material.roughness_factor;
            uniforms.normal_scale = upload.material.normal_scale;
            uniforms.normal_light_strength = upload.material.normal_light_strength;
            uniforms.specular_factor = upload.material.specular_factor;
            uniforms.specular_power = upload.material.specular_power;
            uniforms.occlusion_strength = upload.material.occlusion_strength;
            uniforms.lightmap_intensity = upload.lightmap_multiplier;
            uniforms.lightmap_min = upload.lightmap_min;
            uniforms.global_light_color = {upload.global_light_color[0],
                                           upload.global_light_color[1],
                                           upload.global_light_color[2], 0.0F};
            uniforms.global_light_intensity = upload.global_light_intensity;
            uniforms.emissive_factor = {upload.material.emissive_factor[0],
                                        upload.material.emissive_factor[1],
                                        upload.material.emissive_factor[2], 0.0F};
            uniforms.alpha_cutoff = upload.material.alpha_cutoff;
            uniforms.alpha_mode = alpha_mode(upload.material.alpha_mode);
            uniforms.unlit = upload.material.unlit ? 1U : 0U;
            uniforms.texture_transform0 = {
                transform0_for(upload.base_color_texture),
                transform0_for(upload.normal_texture),
                transform0_for(upload.metallic_roughness_texture),
                transform0_for(upload.occlusion_texture),
                transform0_for(upload.emissive_texture),
                transform0_for(upload.specular_texture),
            };
            uniforms.texture_transform1 = {
                transform1_for(upload.base_color_texture),
                transform1_for(upload.normal_texture),
                transform1_for(upload.metallic_roughness_texture),
                transform1_for(upload.occlusion_texture),
                transform1_for(upload.emissive_texture),
                transform1_for(upload.specular_texture),
            };
        }

        const bool alpha_blend =
            material != nullptr &&
            material->upload.material.alpha_mode == stellar::assets::AlphaMode::kBlend;
        [impl_->encoder setRenderPipelineState:alpha_blend ? impl_->blend_pipeline
                                                           : impl_->opaque_pipeline];
        const bool double_sided =
            material != nullptr && material->upload.material.double_sided;
        [impl_->encoder setFrontFacingWinding:MTLWindingCounterClockwise];
        [impl_->encoder setCullMode:double_sided ? MTLCullModeNone : MTLCullModeBack];
        [impl_->encoder setVertexBuffer:primitive.vertex_buffer offset:0 atIndex:0];
        [impl_->encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
        [impl_->encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
        if (impl_->debug_render_enabled && impl_->debug_material_logs < 8U &&
            material != nullptr) {
            std::fprintf(stderr,
                         "[stellar][metal] material name=\"%s\" base=%u lightmap=%u "
                         "normal=%u mr=%u occlusion=%u emissive=%u specular=%u "
                         "alpha=%u double_sided=%u unlit=%u\n",
                         material->upload.material.name.c_str(),
                         uniforms.has_base_color_texture, uniforms.has_lightmap_texture,
                         uniforms.has_normal_texture,
                         uniforms.has_metallic_roughness_texture,
                         uniforms.has_occlusion_texture, uniforms.has_emissive_texture,
                         uniforms.has_specular_texture, uniforms.alpha_mode,
                         double_sided ? 1U : 0U, uniforms.unlit);
            ++impl_->debug_material_logs;
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
