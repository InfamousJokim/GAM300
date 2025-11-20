#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 biTangent;
layout (location = 5) in ivec4 joints;
layout (location = 6) in vec4 weights;

#define MAX_WEIGHTS 4
#define MAX_JOINTS 100

out Vertex {
    vec3 position;
    vec3 normal;
    mat3 TBN; //tangent, bitangent, normal
    vec2 uv;
} vertex;
layout (location = 1) out vec3 viewPos;
layout (location = 2) out vec4 fragPosLight;

uniform mat4 modelMat;
uniform mat4 frustumMat; // proj * view

uniform mat4 jointsMat[MAX_JOINTS];
uniform bool hasJoints = false;

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

    vertex.uv = vec2(uv.x, 1.0 - uv.y); //flip vertically due to opengl rendering logic
    transform = modelMat * transform;
    vertex.normal = mat3(transform) * normal;
    vertex.position = (transform * vec4(position, 1.0)).xyz;
    gl_Position = frustumMat * transform * vec4(position, 1.0);
    vertex.TBN = mat3(transform) * mat3(tangent, biTangent, normal);
}
==VERTEX==

#version 450 core

in Vertex {
    vec3 position;
    vec3 normal;
    mat3 TBN;
    vec2 uv;
} vertex;

struct Material {
    vec3 emissive;
    vec3 albedo;
    float roughness;
    float metallic;
    float occlusion;

    sampler2D occlusionMap;
    sampler2D roughnessMap;
    sampler2D emissiveMap;
    sampler2D metallicMap;
    sampler2D albedoMap;
    sampler2D normalMap;

    bool isOcclusionMap;
    bool isRoughnessMap;
    bool isEmissiveMap;
    bool isMetallicMap;
    bool isAlbedoMap;
    bool isNormalMap;
};
uniform Material material;

const float PI = 3.14159265358979323846;
const int MAX_LIGHTS = 10; //this number has to be redefined here due to glsl constrains
layout (location=0) out vec4 out_fragment;
layout(location=1) out vec4 out_brightness; //for bloom
const vec3 BLOOM_THRESHOLD = vec3(0.2126, 0.7152, 0.0722) ;

struct PointLight {
    vec3 position;
    vec3 radiance;
    float intensity;
};
uniform PointLight pointLights[MAX_LIGHTS];
uniform int noPointLight;

struct DirectionalLight {
    vec3 radiance;
    vec3 dir;
    float intensity;
};
uniform DirectionalLight dirLights[MAX_LIGHTS];
uniform int noDirLight;

struct SpotLight {
    vec3 dir;
    vec3 position;
    vec3 radiance;
    float intensity;
    float fallOff;
    float cutOff;
};
uniform SpotLight spotLights[MAX_LIGHTS];
uniform int noSpotLight;

uniform vec3 viewPos;

uniform bool isDebugMode;
uniform bool showNormalTexture;

// shadow mapping
uniform mat4 u_lightSpace;
uniform sampler2D u_depthMap;
float ComputeShadow()
{
  vec4 pos = u_lightSpace * vec4(vertex.position, 1.0);
  vec3 uvs = (pos.xyz / pos.w) * 0.5 + 0.5;
  float depth = texture(u_depthMap, uvs.xy).r;

  return pos.z > depth ? 1.0 : 0.0;
}

//this effect influences the appearance of surfaces
// for example, higher reflectivity at grazing angles than dielectrics
// f0 stands for the base fresnel distributivity
vec3 FresnelSchlick(float cosTheta, vec3 f0);
//approximate Fresnel factor for the microfacet distribution model
// for the material's reflectivity based on viewing angle
float GeometrySchlickGGX(float nDotV, float roughness);
//computes visibility term that accounts for self-shadowing 
// and masking effects due to microfacet distribution
float GeometrySmithGGX(float nDotV, float nDotL, float roughness);
//Trowbridge-Reitz calculates the probability distribution 
// of surface normals based on surface roughness
float DistributionGGX(vec3 N, vec3 H, float roughness);
vec3 ComputePointLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic);
vec3 ComputeDirLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic);
vec3 ComputeSpotLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic);

vec3 ComputeMapOrMatV3(bool isMap, sampler2D map, vec3 mat) {
    vec3 res = mat;
    if (isMap) {
        res = texture(map, vertex.uv).rgb;
    }
    return res;
}
float ComputeMapOrMatF(bool isMap, sampler2D map, float mat) {
    float res = mat;
    if (isMap) {
        res = texture(map, vertex.uv).r;
    }
    return res;
}

const int bayer64[64] = int[64](0,  32,  8,  40, 2, 34, 10, 42,
                                48, 16, 56, 25, 50, 18, 58, 26,
                                12, 44, 4, 36, 14, 46, 6, 38,
                                60, 28, 52, 20, 62, 30, 54, 22,
                                3, 35, 11, 43, 1, 33, 8, 41,
                                51, 19, 59, 27, 49, 17, 57, 25,
                                15, 47, 7, 39, 13, 45, 5, 37,
                                63, 31, 55, 23, 61, 29, 53, 21
                                );

uniform float ditherThreshold;

void main() {
    if (isDebugMode) {
        //                                       green                strength
        out_fragment = vec4(mix(material.albedo, vec3(0.0, 1.0, 0.0), 0.75), 1.0);
        return;
    }
    if (showNormalTexture){
        out_fragment = vec4(normalize(vertex.normal) * 0.5 + 0.5, 1.0); //normal map colors
        return;
    }
    vec3 V = normalize(viewPos - vertex.position);

    //material or texture maps
    vec3 N = normalize(vertex.normal);
    if (material.isNormalMap) {
        N = 2.0 * texture(material.normalMap, vertex.uv).rgb - 1.0;
        N = normalize(vertex.TBN * N);
    }
    vec3 albedo = ComputeMapOrMatV3(material.isAlbedoMap, material.albedoMap, material.albedo);
    float roughness = ComputeMapOrMatF(material.isRoughnessMap, material.roughnessMap, material.roughness);
    float metallic = ComputeMapOrMatF(material.isMetallicMap, material.metallicMap, material.metallic);
    vec3 emissive = ComputeMapOrMatV3(material.isEmissiveMap, material.emissiveMap, material.emissive);
    float occlusion = ComputeMapOrMatF(material.isOcclusionMap, material.occlusionMap, material.occlusion);

    //fresnel reflectivity
    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    //lights
    vec3 color = ComputePointLights(N, V, f0, albedo, roughness, metallic) + 
                ComputeDirLights(N, V, f0, albedo, roughness, metallic) + 
                ComputeSpotLights(N, V, f0, albedo, roughness, metallic);
    
    //shadows, occ and em
    color = (color * occlusion) + emissive;
    color *= 1.0 - ComputeShadow();

    if (dot(color,BLOOM_THRESHOLD)>1.0) {
        out_brightness=vec4(color,1.0);
    }
    else {
        out_brightness=vec4(0.0,0.0,0.0,1.0);
    }

    //simulate low bit depth
    float colorDepth = 32.0;
    vec3 quanColor = floor(color * colorDepth) / colorDepth; //5bits per channel, 32 levels

    //dither logic
    int col = int(mod(gl_FragCoord.x, 8));
    int row = int(mod(gl_FragCoord.y, 8));
    float threshold = float(bayer64[col + 8 * row]) / 64.0;
    if (length(color - quanColor) <= threshold * ditherThreshold) color = quanColor;

    out_fragment = vec4(color, 1.0);
    
}

vec3 FresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float GeometrySchlickGGX(float nDotV, float roughness) {
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    float denom = nDotV * (1.0 - k) + k;
    return nDotV / denom;
}
float GeometrySmithGGX(float nDotV, float nDotL, float roughness) {
    float ggx1 = GeometrySchlickGGX(nDotV, roughness);
    float ggx2 = GeometrySchlickGGX(nDotL, roughness);
    return ggx1 * ggx2;
}
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float aSq = (roughness * roughness) * (roughness * roughness);
    float nDotH = max(dot(N, H), 0.0);
    float nDotHSq = nDotH * nDotH;
    float denom = nDotHSq * (aSq - 1.0) + 1.0;
    return aSq / (PI * denom * denom);
}
vec3 ComputePointLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic) {
    vec3 result = vec3(0.0); 

    for (int i = 0; i < noPointLight; ++i) {
        //break condition
        if (i >= MAX_LIGHTS) {
            break;
        }

        vec3 L = normalize(pointLights[i].position - vertex.position);
        vec3 H = normalize(L + V);
        float nDotL = max(dot(N, L), 0.0);
        float nDotV = max(dot(N, V), 0.0);

        //Cook-Torrance (BRDF)
        float NDF = DistributionGGX(N, H, roughness);
        vec3 FS = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), f0);
        float GS = GeometrySmithGGX(nDotV, nDotL, roughness);

        vec3 diffuse = (vec3(1.0) - FS) * (1.0 - metallic) * albedo / PI;
        vec3 specular = (NDF * GS * FS) / max(4.0 * nDotV * nDotL, 0.0001);
        float dist = length(pointLights[i].position - vertex.position);
        float attenuation = pointLights[i].intensity / (dist * dist);

        result += (diffuse + specular) * pointLights[i].radiance * attenuation * nDotL;
    }

    return result;
}
vec3 ComputeDirLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic) {
    vec3 result = vec3(0.0);

    for (int i = 0; i < noDirLight; ++i) {
        //break condition
        if (i >= MAX_LIGHTS) {
            break;
        }

        vec3 L = -normalize(dirLights[i].dir);
        float nDotL = max(dot(N, L), 0.0);
        float nDotV = max(dot(N, V), 0.0);
        vec3 H = normalize(L + V);

        //BRDF
        float NDF = DistributionGGX(N, H, roughness);
        vec3 FS = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), f0);
        float GS = GeometrySmithGGX(nDotV, nDotL, roughness);

        vec3 diffuse = (vec3(1.0) - FS) * (1.0 - metallic) * albedo / PI;
        vec3 specular = (NDF * GS * FS) / max(4.0 * nDotV * nDotL, 0.0001);

        result += (diffuse + specular) * dirLights[i].radiance * dirLights[i].intensity * nDotL;
    }

    return result;
}
vec3 ComputeSpotLights(vec3 N, vec3 V, vec3 f0, vec3 albedo, float roughness, float metallic) {
    vec3 result = vec3(0.0);

    for (int i = 0; i < noSpotLight; ++i) {
        //break condition
        if (i >= MAX_LIGHTS) {
            break;
        }

        vec3 L = normalize(spotLights[i].position - vertex.position);
        float nDotL = max(dot(N, L), 0.0);
        float nDotV = max(dot(N, V), 0.0);
        vec3 H = normalize(L + V);

        //BRDF
        float NDF = DistributionGGX(N, H, roughness);
        vec3 FS = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), f0);
        float GS = GeometrySmithGGX(nDotV, nDotL, roughness);

        vec3 diffuse = (vec3(1.0) - FS) * (1.0 - metallic) * albedo / PI;
        vec3 specular = (NDF * GS * FS) / max(4.0 * nDotV * nDotL, 0.0001);

        //compute spot
        float theta = dot(L, normalize(-spotLights[i].dir));
        float epsilon = max(spotLights[i].fallOff - spotLights[i].cutOff, 0.0001);
        float spotFactor = clamp((theta - spotLights[i].cutOff) / epsilon, 0.0, 1.0);

        float dist = length(spotLights[i].position - vertex.position);
        float attenuation = spotLights[i].intensity / (dist * dist);

        result += (diffuse + specular) * 
                    spotLights[i].radiance * 
                    spotLights[i].intensity * 
                    attenuation * 
                    nDotL
                    * spotFactor;
    }

    return result;
}
==FRAGMENT==
