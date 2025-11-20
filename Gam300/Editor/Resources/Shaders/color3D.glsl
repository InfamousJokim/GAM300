#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

uniform mat4 mat;

void main() {
    gl_Position = mat * vec4(position, 1.0);
}
==VERTEX==

#version 450 core

out vec4 FragColor;
uniform vec4 color;
void main() {
    FragColor = color;
}
==FRAGMENT==