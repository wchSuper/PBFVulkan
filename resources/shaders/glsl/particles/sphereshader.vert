#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTex;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outViewDir;


void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    
    // 从视图矩阵的逆矩阵中提取相机世界坐标
    vec3 cameraPos = inverse(view)[3].xyz;
    
    outNormal = mat3(transpose(inverse(model))) * aNormal;
    
    // 正确计算从表面指向相机的向量
    outViewDir = cameraPos - worldPos.xyz;
    
    gl_Position = projection * view * worldPos;
}