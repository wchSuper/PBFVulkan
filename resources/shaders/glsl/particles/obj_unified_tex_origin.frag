#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec4 Color;
layout(location = 4) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

layout(binding=1) uniform sampler2D ObjTexture;

void main() {
    vec3 color = textureLod(ObjTexture, TexCoord, 2.0).rgb * 1.0;
    outColor = vec4(color, 1.0);
}