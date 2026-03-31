#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

// DiffuseParticle结构
layout(location=0) in vec3 inlocation;
layout(location=1) in vec3 invelocity;
layout(location=2) in float inmass;
layout(location=3) in float inbuoyancy;
layout(location=4) in float inlifetime;     // 关键：用于剔除死亡粒子
layout(location=5) in uint intype;

layout(location=0) flat out float outviewdepth;
layout(location=1) flat out uint outtype;

void main(){
    // 剔除死亡粒子（LifeTime <= 0）
    if (inlifetime <= 0.0) {
        gl_Position = vec4(0.0, 0.0, -100.0, 1.0);
        gl_PointSize = 0.0;
        return;
    }

    vec4 viewlocation = view*model*vec4(inlocation,1); 
    
    outviewdepth = viewlocation.z;
    outtype = intype;

    gl_Position = projection * viewlocation;

    float nearHeight = 2 * zNear * tan(fovy / 2);
    float scale = 1200 / nearHeight;
    float nearSize = particleRadius * zNear / (-outviewdepth);

    // 根据粒子类型调整大小
    float sizeScale = 1.0;
    if (intype == 1) {
        sizeScale = 0.6;  // Spray/Foam 较小
    } else if (intype == 2) {
        sizeScale = 1.0;  // Bubble 正常
    }

    gl_PointSize = 2.8 * scale * nearSize * sizeScale;
}
