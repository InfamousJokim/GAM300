#version 450 core

layout (location = 0) in vec3 position;
layout (location = 5) in ivec4 joints;
layout (location = 6) in vec4 weights;

#define MAX_WEIGHTS 4
#define MAX_JOINTS 100

uniform mat4 modelMat;
uniform mat4 frustumMat; // proj * view
uniform mat4 jointsMat[MAX_JOINTS];
uniform bool hasJoints = false;
uniform uint uEntityID = 0u;  // default: no entity

flat out uint vEntityID;

void main() {
    mat4 transform = mat4(1.0);

    if(hasJoints)
    {
        transform = mat4(0.0);
        for(int i = 0; i < MAX_WEIGHTS && joints[i] > -1; i++)
        {
            transform += jointsMat[joints[i]] * weights[i];
        }
    }

    transform = modelMat * transform;
    gl_Position = frustumMat * transform * vec4(position, 1.0);

    // Pass entity ID flat (no interpolation) to fragment shader
    vEntityID = uEntityID;
}
==VERTEX==

#version 450 core

layout (location = 0) out uint outEntityID;

flat in uint vEntityID;

void main() {
    // Every fragment of this object gets the same entity ID
    outEntityID = vEntityID;

    // Optional: discard background/sky fragments if you want
    // if (fs_in.entityID == 0u) discard;
}
==FRAGMENT==
