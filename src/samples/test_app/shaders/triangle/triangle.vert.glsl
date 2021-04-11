#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_color;
layout (location = 2) in vec3 a_uv;

layout (set = 0, binding = 0) uniform renderer_global {
    mat4 u_projectopn;
    mat4 u_view;
    mat4 u_model;
    mat4 u_mvp;
} global;

layout (location = 0) out vec3 v_color;
layout (location = 1) out vec2 v_uv;

void main()
{
    gl_Position = global.u_mvp * vec4(a_position, 1.);
    v_color = a_color;
    v_uv = a_uv.xy;
}