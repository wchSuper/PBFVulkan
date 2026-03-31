#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;   
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord; 



void main() {
    // fragNormal = inNormal;
    fragNormal = mat3(transpose(inverse(model))) * inNormal;

    vec4 viewlocation = view*model*vec4(inPosition,1); 
    
    gl_Position = projection*view*model*vec4(inPosition,1);
    
    fragTexCoord = inTexCoord;
}