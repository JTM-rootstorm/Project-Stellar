#version 450

layout(location = 0) in vec3 frag_world_position;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv0;
layout(location = 3) in vec4 frag_tangent;
layout(location = 4) in vec2 frag_uv1;
layout(location = 5) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main() {
    vec3 normal = normalize(frag_normal);
    float lambert = max(dot(normal, normalize(vec3(0.25, 0.35, 0.9))), 0.0);
    vec3 lit_color = frag_color.rgb * (0.25 + lambert * 0.75);
    out_color = vec4(lit_color, frag_color.a);
}
