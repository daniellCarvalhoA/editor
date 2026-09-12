#version 330 core

layout(location = 0) in vec2 pos;

uniform mat4 proj;
uniform mat4 view;

precision highp float;
void main()
{
    gl_Position = proj * view * vec4(pos, 0.0, 1.0);
}
