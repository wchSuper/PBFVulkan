#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 TexCoord;
layout(location = 3) out vec4 Color;

layout(binding=1) readonly buffer EllipsoidBufferInfo{
    mat4 model;
    vec3 center;
    vec3 radii;
    vec3 accumulatedForce;        
    vec3 accumulatedTorque;       
    vec3 velocity;
    vec3 angularVelocity;
    vec4 orientation;
} ellipsoid;

void main() {
    vec4 worldPos = ellipsoid.model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(ellipsoid.model))) * aNormal; 
    
    gl_Position = projection * view * worldPos;
    Color = aColor;
}