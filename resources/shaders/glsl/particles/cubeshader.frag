#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location=0) out float outdepth;
layout(location=1) out float outthickness;

layout(location=0) flat in float inviewdepth;

void main(){
    float radius = rigidRadius;
    vec2 pos = gl_PointCoord - vec2(0.5);
    if(length(pos) > 0.5){
        discard;
        return;
    }
    float l = radius*2*length(pos);
    float viewdepth = inviewdepth + sqrt(radius*radius - l*l);
    
    vec4 temp = vec4(0,0,viewdepth,1);
    
    temp = projection*temp;

    outdepth = temp.z/temp.w;

    outthickness = 4*sqrt(radius*radius - l*l);
    
}