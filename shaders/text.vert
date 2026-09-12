#version 410 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 a_tex_coord;


out vec2 uv;

uniform mat4 proj;
uniform mat4 view;

void main() 
{
    //vec2 p = pos.xy;

    //float depth = (abs(row - cursor_pos.y));

    //float scale = 1.0 / (1.0 + depth * 0.2);

    // scale around the cursor
    //p.xy = cursor_pos + (p.xy - cursor_pos) * scale;
    
    gl_Position = proj * view * vec4(pos, 0.0, 1.0);
    uv = a_tex_coord;

}
