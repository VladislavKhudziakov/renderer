#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;

layout (push_constant) uniform renderer_instance_data {
    vec4 color;
    mat4 transform;
} instance_data;

layout (set = 0, binding = 0) uniform renderer_global {
    mat4 u_projectopn;
    mat4 u_view_porj;
    mat4 u_view;
    mat4 u_model;
    mat4 u_mvp;
} global;

layout (location = 0) out vec3 v_normal;
layout (location = 1) out vec3 v_color;
layout (location = 2) out vec3 v_vertex;
layout (location = 3) out vec2 v_uv;

void main()
{
    gl_Position = global.u_mvp * instance_data.transform * vec4(a_position, 1.);
    gl_Position.y = 1. - gl_Position.y;
    v_normal = inverse(transpose(mat3(global.u_model * instance_data.transform))) * normalize(a_normal);
    v_vertex = vec3(global.u_model * vec4(a_position, 1));
    v_color = instance_data.color.rgb;
    v_uv = a_uv;
}