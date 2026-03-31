#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;
layout(location = 3) in vec4 aColor;

layout(location = 0) out vec3 oFragPos;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec2 oTexCoord;
layout(location = 3) out vec4 oColor;
layout(location = 4) out vec3 oViewPos;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    oFragPos = worldPos.xyz;
    oNormal = mat3(transpose(inverse(model))) * aNormal; 
    
    gl_Position = projection * view * worldPos;
    oTexCoord = aTex;
    oViewPos = camera_pos;
}