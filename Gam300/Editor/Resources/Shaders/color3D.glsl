#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

uniform mat4 mat;
uniform mat4 proj;
layout (location = 2) out vec2 o_uv;

void main() {
    o_uv = vec2(uv.x, 1.0 - uv.y);
    gl_Position = proj * mat * vec4(position, 1.0);
}
==VERTEX==

#version 450 core

layout (location = 2) in vec2 uvs;
out vec4 FragColor;
uniform vec4 color;
uniform sampler2D texMap;
void main() {
    vec4 result = texture(texMap, uvs).rgba;
    FragColor = color * result;
}
==FRAGMENT==