#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inUV;
layout (location = 2) in vec4 inNormal;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outView;
layout (location = 3) out vec3 outLight;

layout(binding=1) uniform UniformEnv{
    vec4 lightPos_;
    vec4 lightDir_;
};

void main() 
{
	outUV = inUV.xy;
	outNormal = inNormal.xyz;
	vec4 eyePos = view * model * vec4(inPos.xyz, 1.0); 
	gl_Position = projection * eyePos;	

    vec3 lPos = lightPos_.xyz;
    outLight = lPos - inPos.xyz;
    outView = -inPos.xyz;
}

