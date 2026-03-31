#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location=0) in vec3 inlocation;

layout(location=0) flat out float outviewdepth;


void main(){
    vec4 viewlocation = view*model*vec4(inlocation,1); 
    
    outviewdepth = viewlocation.z;

    gl_Position = projection*viewlocation;

    float nearHeight = 2*zNear*tan(fovy/2);

    float scale = 900/nearHeight;

    float nearSize = rigidRadius * zNear/(-outviewdepth);

    gl_PointSize = 2*scale*nearSize;
  
} 