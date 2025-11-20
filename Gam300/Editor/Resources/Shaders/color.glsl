#version 450 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 in_uv;

layout(location = 1) out vec2 out_uv;
void main() {
    out_uv = in_uv;
    gl_Position = vec4(pos, 0.0, 1.0);
}
==VERTEX==

#version 450 core

out vec4 FragColor;
uniform vec4 color;
void main() {
    FragColor = color;
}
==FRAGMENT==