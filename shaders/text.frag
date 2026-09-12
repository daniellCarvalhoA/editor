#version 410 core

// flat in uint v_flags;
out vec4 color;

// in vec3 our_color;
in vec2 uv;

uniform sampler2D atlas;

void main() 
{
    // color = vec4(uv, 0.0, 1.0);
    float alpha = texture(atlas, uv).r;
    color = vec4(0.9, 0.9, 0.9, alpha);;
};
