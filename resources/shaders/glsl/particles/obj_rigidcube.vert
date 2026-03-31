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

struct ShapeMatchingInfo {
    // 初始质心 & 当前质心
    vec3 CM0;
    vec3 CM;

    // 协方差矩阵
    vec3 Apq_col0;
    vec3 Apq_col1;
    vec3 Apq_col2;

    // 旋转矩阵
    vec3 Q_col0;
    vec3 Q_col1;
    vec3 Q_col2;

    // 伸缩矩阵
    vec3 S_col0;
    vec3 S_col1;
    vec3 S_col2;
};
layout(binding=1) readonly buffer ShapeMatchingBuffer{
    ShapeMatchingInfo smi;
};

void main() {
    mat3 R = mat3(
        smi.Q_col0,
        smi.Q_col1,
        smi.Q_col2
    );

    vec3 r = aPos - smi.CM0;
    vec3 new_pos = R * r + smi.CM;

    vec4 worldPos = vec4(new_pos, 1.0);
    FragPos = worldPos.xyz;
    Normal = R * aNormal; 
    
    gl_Position = projection * view * worldPos;
	TexCoord = aTex;
    Color = aColor;
}