#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec4 o_FragColor;

layout (location = 0) in vec3 v_normal;
layout (location = 1) in vec3 v_color;
layout (location = 2) in vec3 v_vertex;
layout (location = 3) in vec2 v_uv;

layout (set = 0, binding = 1) uniform sampler2D s_tex_image;

void main()
{
    float diff = max(dot(v_normal, normalize(vec3(0, 1, -1))), 0);
    o_FragColor = vec4(texture(s_tex_image, v_uv).rgb * diff, 1.);
}