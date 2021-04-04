#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_color;

layout (binding = 0) uniform renderer_global {
    mat4 u_projectopn;
    mat4 u_view;
    mat4 u_model;
    mat4 u_mvp;
} global;

layout (location = 0) out vec3 v_color;

void main()
{
    gl_Position = vec4(mat3(global.u_mvp) * a_position, 1.);
    v_color = a_color;
}