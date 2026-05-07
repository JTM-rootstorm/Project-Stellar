#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv0;
layout(location = 3) in vec4 in_tangent;
layout(location = 4) in vec2 in_uv1;
layout(location = 5) in vec4 in_color;

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

layout(location = 0) out vec3 frag_normal;
layout(location = 1) out vec2 frag_uv0;
layout(location = 2) out vec4 frag_tangent;
layout(location = 3) out vec2 frag_uv1;
layout(location = 4) out vec4 frag_color;
layout(location = 5) out vec3 frag_world_position;

void main() {
    vec4 world_position = uniforms.world * vec4(in_position, 1.0);
    mat3 normal_matrix = mat3(uniforms.normal_column0.xyz,
                              uniforms.normal_column1.xyz,
                              uniforms.normal_column2.xyz);
    frag_normal = normalize(normal_matrix * in_normal);
    frag_uv0 = in_uv0;
    frag_tangent = vec4(mat3(uniforms.world) * in_tangent.xyz, in_tangent.w);
    frag_uv1 = in_uv1;
    frag_color = uniforms.material_flags1.w != 0u ? in_color : vec4(1.0);
    frag_world_position = world_position.xyz;
    gl_Position = uniforms.mvp * vec4(in_position, 1.0);
}
