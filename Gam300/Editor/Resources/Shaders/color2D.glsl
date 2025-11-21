#version 450 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 in_uv;
layout(location = 1) out vec2 out_uv;
uniform mat3 mat;

void main() {
    out_uv = vec2(in_uv.x, 1.0 - in_uv.y);
    vec3 res = mat * vec3(pos, 1.0);
    gl_Position = vec4(res.x, res.y, 0.0, 1.0);
}
==VERTEX==

#version 450 core

layout(location = 1) in vec2 uvs;
out vec4 FragColor;
uniform vec4 color;
uniform sampler2D texMap;
void main() {
    vec4 result = texture(texMap, uvs).rgba;
    FragColor = color * result;
}
==FRAGMENT==