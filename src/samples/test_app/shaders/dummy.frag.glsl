#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec4 o_FragColor;

layout (set = 1, binding = 0) uniform sampler2D s_diffuse;

layout (location = 0) in vec3 v_normal;
layout (location = 1) in vec2 v_uv;

void main()
{
    o_FragColor = vec4(texture(s_diffuse, v_uv).rgb, 1.);
}