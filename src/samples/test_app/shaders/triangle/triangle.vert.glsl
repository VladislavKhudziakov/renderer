#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_color;

layout (location = 0) out vec3 v_color;

const vec3 positions[] = vec3[](
    vec3(-0.5, -0.5, 0.0),
    vec3(0, 0.5, 0.0),
    vec3(0.5, -0.5, 0.0)
);

const vec3 colors[] = vec3[](
    vec3(1, 0.0, 0.0),
    vec3(0, 1, 0.0),
    vec3(0, 0.0, 1)
);

void main()
{
    gl_Position = vec4(a_position, 1.);
    v_color = a_color;
}