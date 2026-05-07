#version 450

layout(set = 0, binding = 0) uniform sampler2D base_color_texture;
layout(set = 0, binding = 1) uniform sampler2D normal_texture;
layout(set = 0, binding = 2) uniform sampler2D metallic_roughness_texture;
layout(set = 0, binding = 3) uniform sampler2D occlusion_texture;
layout(set = 0, binding = 4) uniform sampler2D emissive_texture;
layout(set = 0, binding = 5) uniform sampler2D lightmap_texture;
layout(set = 0, binding = 6) uniform sampler2D specular_texture;

layout(std140, set = 0, binding = 7) uniform DrawUniformBlock {
    mat4 mvp;
    mat4 world;
    vec4 normal_column0;
    vec4 normal_column1;
    vec4 normal_column2;
    vec4 base_color;
    vec4 emissive_factor;
    vec4 global_light_color;
    vec4 camera_world_position;
    vec4 key_light_direction;
    vec4 texture_transform0[6];
    vec4 texture_transform1[6];
    vec4 material_factors0;
    vec4 material_factors1;
    vec4 material_factors2;
    uvec4 material_flags0;
    uvec4 material_flags1;
    uvec4 material_flags2;
    uvec4 material_flags3;
    uvec4 material_flags4;
} uniforms;

layout(location = 0) in vec3 frag_normal;
layout(location = 1) in vec2 frag_uv0;
layout(location = 2) in vec4 frag_tangent;
layout(location = 3) in vec2 frag_uv1;
layout(location = 4) in vec4 frag_color;
layout(location = 5) in vec3 frag_world_position;

layout(location = 0) out vec4 out_color;

vec2 uv_for_set(uint texcoord_set) {
    return texcoord_set == 1u ? frag_uv1 : frag_uv0;
}

vec2 transformed_uv_for_slot(uint texcoord_set, uint slot) {
    vec2 uv = uv_for_set(texcoord_set);
    if (uniforms.texture_transform0[slot].w == 0.0) {
        return uv;
    }

    vec2 scaled = uv * uniforms.texture_transform1[slot].xy;
    float c = cos(uniforms.texture_transform0[slot].z);
    float s = sin(uniforms.texture_transform0[slot].z);
    return uniforms.texture_transform0[slot].xy + vec2(
        c * scaled.x - s * scaled.y,
        s * scaled.x + c * scaled.y);
}

void main() {
    bool has_base_color = uniforms.material_flags0.x != 0u;
    bool has_lightmap = uniforms.material_flags0.y != 0u;
    bool has_normal = uniforms.material_flags0.z != 0u;
    bool has_metallic_roughness = uniforms.material_flags0.w != 0u;
    bool has_occlusion = uniforms.material_flags1.x != 0u;
    bool has_emissive = uniforms.material_flags1.y != 0u;
    bool has_specular = uniforms.material_flags1.z != 0u;
    bool has_camera_world_position = uniforms.material_flags2.x != 0u;
    uint alpha_mode = uniforms.material_flags4.x;
    bool unlit = uniforms.material_flags4.y != 0u;

    vec4 color = uniforms.base_color * frag_color;
    if (has_base_color) {
        color *= texture(base_color_texture,
                         transformed_uv_for_slot(uniforms.material_flags2.y, 0u));
    }
    if (alpha_mode == 1u && color.a < uniforms.material_factors2.z) {
        discard;
    }

    float metallic = uniforms.material_factors0.x;
    float roughness = uniforms.material_factors0.y;
    if (has_metallic_roughness) {
        vec4 metallic_roughness = texture(
            metallic_roughness_texture,
            transformed_uv_for_slot(uniforms.material_flags3.x, 2u));
        roughness *= metallic_roughness.g;
        metallic *= metallic_roughness.b;
    }

    vec3 normal = normalize(frag_normal);
    if (has_normal) {
        vec3 tangent = normalize(frag_tangent.xyz - normal * dot(frag_tangent.xyz, normal));
        vec3 bitangent = normalize(cross(normal, tangent) * frag_tangent.w);
        mat3 tbn = mat3(tangent, bitangent, normal);
        vec3 sampled_normal = texture(
            normal_texture, transformed_uv_for_slot(uniforms.material_flags2.w, 1u)).xyz *
            2.0 - 1.0;
        sampled_normal.xy *= uniforms.material_factors0.z;
        normal = normalize(tbn * sampled_normal);
    }

    vec3 emissive = uniforms.emissive_factor.rgb;
    if (has_emissive) {
        emissive *= texture(
            emissive_texture,
            transformed_uv_for_slot(uniforms.material_flags3.z, 4u)).rgb;
    }

    vec3 light_dir = normalize(uniforms.key_light_direction.xyz);
    float detail_diffuse = max(dot(normal, light_dir), 0.0) *
                           uniforms.material_factors0.w;
    float occlusion = 1.0;
    if (has_occlusion) {
        float occlusion_sample = texture(
            occlusion_texture,
            transformed_uv_for_slot(uniforms.material_flags3.y, 3u)).r;
        occlusion = mix(1.0, occlusion_sample, uniforms.material_factors1.z);
    }
    float specular_map = has_specular
        ? texture(specular_texture,
                  transformed_uv_for_slot(uniforms.material_flags3.w, 5u)).r
        : 1.0;
    vec3 view_delta = uniforms.camera_world_position.xyz - frag_world_position;
    vec3 view_dir = has_camera_world_position && dot(view_delta, view_delta) > 0.000001
        ? normalize(view_delta)
        : normalize(vec3(0.0, -1.0, 0.25));
    vec3 half_dir = normalize(light_dir + view_dir);
    float effective_power = mix(max(uniforms.material_factors1.y, 1.0), 4.0,
                                clamp(roughness, 0.0, 1.0));
    float specular = pow(max(dot(normal, half_dir), 0.0), effective_power) *
                     uniforms.material_factors1.x * specular_map;

    if (has_lightmap) {
        vec3 lightmap = texture(lightmap_texture, uv_for_set(uniforms.material_flags2.z)).rgb;
        vec3 lighting = lightmap * uniforms.material_factors1.w +
                        uniforms.global_light_color.rgb * uniforms.material_factors2.y +
                        vec3(detail_diffuse);
        out_color = vec4(color.rgb * max(lighting, vec3(uniforms.material_factors2.x)) +
                             vec3(specular) + emissive,
                         color.a);
        return;
    }
    if (unlit) {
        out_color = vec4(color.rgb + emissive, color.a);
        return;
    }

    float diffuse = max(dot(normal, light_dir), 0.0);
    float perceptual_roughness = clamp(roughness, 0.04, 1.0);
    float metal_attenuation = mix(1.0, 0.45, clamp(metallic, 0.0, 1.0));
    float lit = 0.18 + diffuse * metal_attenuation *
                mix(1.0, 0.65, perceptual_roughness);
    out_color = vec4(color.rgb * (lit + detail_diffuse) * occlusion +
                         vec3(specular) + emissive,
                     color.a);
}
