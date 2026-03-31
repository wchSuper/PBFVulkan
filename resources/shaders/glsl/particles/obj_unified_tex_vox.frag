#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec4 Color;
layout(location = 4) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform VoxelUniformData {
    vec3 boxMin;
    ivec3 resolution;
    float voxelSize;
} voxel;

layout(binding = 2) buffer VoxelBuffer {
    uint counts[];
};

layout(binding=3) uniform sampler2D ObjTexture;

void main() {
    // 计算体素索引
    if (FragPos.x - voxel.boxMin.x < 0 || FragPos.y - voxel.boxMin.y < 0 || FragPos.z - voxel.boxMin.z < 0) return;
    ivec3 idx = ivec3((FragPos - voxel.boxMin) / voxel.voxelSize);
    if (all(greaterThanEqual(idx, ivec3(0))) && all(lessThan(idx, voxel.resolution))) {
        uint flatIdx = uint(idx.y * (voxel.resolution.z * voxel.resolution.x) + idx.z * voxel.resolution.x + idx.x);
        atomicAdd(counts[flatIdx], 1);
    }
    
    vec3 albedo = texture(ObjTexture, TexCoord).rgb;

    // 获取光照参数
    vec3 lightPos = lights[0].position;
    vec3 lightColor = lights[0].color * lights[0].intensity;

    // Ambient
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * albedo;
	outColor = vec4(result, 1.0);
}