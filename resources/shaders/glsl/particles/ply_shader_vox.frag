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

void main() {
    ivec3 idx = ivec3((FragPos - voxel.boxMin) / voxel.voxelSize);
    if (all(greaterThanEqual(idx, ivec3(0))) && all(lessThan(idx, voxel.resolution))) {
        uint flatIdx = uint(idx.y * (voxel.resolution.z * voxel.resolution.x) + idx.z * voxel.resolution.x + idx.x);
        atomicAdd(counts[flatIdx], 1);
    }
    outColor = vec4(Color.xyz, 1.0);
}