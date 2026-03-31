#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(std140, binding=1) uniform SimulateObj{
    float dt;
    float accumulated_t;
    float restDensity;
    float sphRadius;

    float coffPoly6;
    float coffSpiky;
    float coffGradSpiky;

    float scorrK;
    float scorrN;
    float scorrQ;

    float spaceSize;
    float fluidParticleRadius;
    float cubeParticleRadius;
    float rigidParticleRadius;

    uint workGroupCount;
    uint gridCount;
    uint testData1;
    uint numParticles;
    uint maxParticles;
};

layout(location=0) in vec3 inlocation;

layout(location=0) flat out float outviewdepth;


void main(){
    if (uint(gl_VertexIndex) >= numParticles) {
        outviewdepth = 0.0;
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        return;
    }
    vec4 viewlocation = view*model*vec4(inlocation,1); 
    
    outviewdepth = viewlocation.z;

    gl_Position = projection * viewlocation;

    float nearHeight = 2 * zNear * tan(fovy / 2);

    float scale = 1200 / nearHeight;

    float nearSize = particleRadius * zNear / (-outviewdepth);

    gl_PointSize = 2.8 * scale * nearSize;
  
} 