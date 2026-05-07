#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv0;
layout(location = 3) in vec4 in_tangent;
layout(location = 4) in vec2 in_uv1;
layout(location = 5) in vec4 in_color;

layout(push_constant) uniform DrawConstants {
    mat4 mvp;
    mat4 world;
    mat3 normal_matrix;
    vec3 camera_world_position;
    uint has_camera_world_position;
} draw_constants;

layout(location = 0) out vec3 frag_world_position;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec2 frag_uv0;
layout(location = 3) out vec4 frag_tangent;
layout(location = 4) out vec2 frag_uv1;
layout(location = 5) out vec4 frag_color;

void main() {
    vec4 world_position = draw_constants.world * vec4(in_position, 1.0);
    frag_world_position = world_position.xyz;
    frag_normal = normalize(draw_constants.normal_matrix * in_normal);
    frag_uv0 = in_uv0;
    frag_tangent = in_tangent;
    frag_uv1 = in_uv1;
    frag_color = in_color;
    gl_Position = draw_constants.mvp * vec4(in_position, 1.0);
}
