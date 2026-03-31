#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(binding=1) uniform UniformBoxInfoObject {
    vec2 clampX;
    vec2 clampY;
    vec2 clampZ;
    vec2 clampX_still;
    vec2 clampY_still;
    vec2 clampZ_still;
};

layout(location=0) out vec2 outTexcoord;

// 顶点数组（32个顶点，4个平面每个8个）
vec4 boxCornerPositions[32] = {
    // 左面（X=0）
    vec4(clampX_still[0], clampY_still[0], clampZ_still[0], 1), // 0: 内底近
    vec4(clampX_still[0], clampY_still[1], clampZ_still[0], 1), // 1: 内顶近
    vec4(clampX_still[0], clampY_still[0], clampZ_still[1], 1), // 2: 内底远
    vec4(clampX_still[0], clampY_still[1], clampZ_still[1], 1), // 3: 内顶远
    vec4(clampX_still[0] - particleRadius * 4, clampY_still[0], clampZ_still[0], 1), // 4: 外底近
    vec4(clampX_still[0] - particleRadius * 4, clampY_still[1], clampZ_still[0], 1), // 5: 外顶近
    vec4(clampX_still[0] - particleRadius * 4, clampY_still[0], clampZ_still[1], 1), // 6: 外底远
    vec4(clampX_still[0] - particleRadius * 4, clampY_still[1], clampZ_still[1], 1), // 7: 外顶远
    // 右面（X=1）
    vec4(clampX_still[1], clampY_still[0], clampZ_still[0], 1), // 8: 内底近
    vec4(clampX_still[1], clampY_still[1], clampZ_still[0], 1), // 9: 内顶近
    vec4(clampX_still[1], clampY_still[0], clampZ_still[1], 1), // 10: 内底远
    vec4(clampX_still[1], clampY_still[1], clampZ_still[1], 1), // 11: 内顶远
    vec4(clampX_still[1] + particleRadius * 4, clampY_still[0], clampZ_still[0], 1), // 12: 外底近
    vec4(clampX_still[1] + particleRadius * 4, clampY_still[1], clampZ_still[0], 1), // 13: 外顶近
    vec4(clampX_still[1] + particleRadius * 4, clampY_still[0], clampZ_still[1], 1), // 14: 外底远
    vec4(clampX_still[1] + particleRadius * 4, clampY_still[1], clampZ_still[1], 1), // 15: 外顶远
    // 底面（Y=0）
    vec4(clampX_still[0], clampY_still[0], clampZ_still[0], 1), // 16: 内左近
    vec4(clampX_still[1], clampY_still[0], clampZ_still[0], 1), // 17: 内右近
    vec4(clampX_still[0], clampY_still[0], clampZ_still[1], 1), // 18: 内左远
    vec4(clampX_still[1], clampY_still[0], clampZ_still[1], 1), // 19: 内右远
    vec4(clampX_still[0], clampY_still[0] - particleRadius * 4, clampZ_still[0], 1), // 20: 外左近
    vec4(clampX_still[1], clampY_still[0] - particleRadius * 4, clampZ_still[0], 1), // 21: 外右近
    vec4(clampX_still[0], clampY_still[0] - particleRadius * 4, clampZ_still[1], 1), // 22: 外左远
    vec4(clampX_still[1], clampY_still[0] - particleRadius * 4, clampZ_still[1], 1), // 23: 外右远
    // 前面（Z=0）
    vec4(clampX_still[0], clampY_still[0], clampZ_still[0], 1), // 24: 内左底
    vec4(clampX_still[1], clampY_still[0], clampZ_still[0], 1), // 25: 内右底
    vec4(clampX_still[0], clampY_still[1], clampZ_still[0], 1), // 26: 内左顶
    vec4(clampX_still[1], clampY_still[1], clampZ_still[0], 1), // 27: 内右顶
    vec4(clampX_still[0], clampY_still[0], clampZ_still[0] - particleRadius * 4, 1), // 28: 外左底
    vec4(clampX_still[1], clampY_still[0], clampZ_still[0] - particleRadius * 4, 1), // 29: 外右底
    vec4(clampX_still[0], clampY_still[1], clampZ_still[0] - particleRadius * 4, 1), // 30: 外左顶
    vec4(clampX_still[1], clampY_still[1], clampZ_still[0] - particleRadius * 4, 1), // 31: 外右顶
};

// 索引数组（144个索引，4个平面各36个）
uint hardCoded_indexbuffer[144] = {
    // 左面（X=0）
    0, 2, 3,  0, 3, 1,      // 内表面 (法向量 -X)
    4, 7, 6,  4, 5, 7,      // 外表面 (法向量 +X)
    0, 4, 2,  2, 4, 6,      // 底侧面 (法向量 -Y)
    1, 3, 5,  3, 7, 5,      // 顶侧面 (法向量 +Y)
    0, 1, 4,  1, 5, 4,      // 近侧面 (法向量 -Z)
    2, 6, 3,  3, 6, 7,      // 远侧面 (法向量 +Z)
    // 右面（X=1）
    8, 11, 10,  8, 9, 11,   // 内表面 (法向量 +X)
    12, 14, 15,  12, 15, 13, // 外表面 (法向量 -X)
    8, 10, 12,  10, 14, 12,  // 底侧面 (法向量 -Y)
    9, 13, 11,  11, 13, 15,  // 顶侧面 (法向量 +Y)
    8, 12, 9,  9, 12, 13,    // 近侧面 (法向量 -Z)
    10, 11, 14,  11, 15, 14, // 远侧面 (法向量 +Z)
    // 底面（Y=0）
    16, 19, 18,  16, 17, 19, // 内表面 (法向量 -Y)
    20, 22, 23,  20, 23, 21, // 外表面 (法向量 +Y)
    16, 18, 20,  18, 22, 20, // 左侧面 (法向量 -X)
    17, 21, 19,  19, 21, 23, // 右侧面 (法向量 +X)
    16, 20, 17,  17, 20, 21, // 近侧面 (法向量 -Z)
    18, 19, 22,  19, 23, 22, // 远侧面 (法向量 +Z)
    // 前面（Z=0）
    24, 27, 26,  24, 25, 27, // 内表面 (法向量 -Z)
    28, 30, 31,  28, 31, 29, // 外表面 (法向量 +Z)
    24, 28, 26,  26, 28, 30, // 底侧面 (法向量 -Y)
    25, 29, 27,  27, 29, 31, // 顶侧面 (法向量 +Y)
    24, 25, 28,  25, 29, 28, // 左侧面 (法向量 -X)
    26, 27, 30,  27, 31, 30, // 右侧面 (法向量 +X)
};

// UV坐标（每个平面12个三角形，36个顶点）
vec2 hardCoded_UVs[144] = {
    // 左面（X=0）
    vec2(0,0), vec2(0,1), vec2(1,1),  vec2(0,0), vec2(1,1), vec2(1,0), // 内表面
    vec2(0,0), vec2(1,1), vec2(0,1),  vec2(0,0), vec2(1,0), vec2(1,1), // 外表面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 底侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(1,0), vec2(1,1), vec2(0,1), // 顶侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(1,0), vec2(1,1), vec2(0,1), // 近侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 远侧面
    // 右面（X=1）
    vec2(0,0), vec2(1,1), vec2(0,1),  vec2(0,0), vec2(1,0), vec2(1,1), // 内表面
    vec2(0,0), vec2(0,1), vec2(1,1),  vec2(0,0), vec2(1,1), vec2(1,0), // 外表面
    vec2(0,0), vec2(0,1), vec2(1,0),  vec2(0,1), vec2(1,1), vec2(1,0), // 底侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 顶侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 近侧面
    vec2(0,0), vec2(0,1), vec2(1,0),  vec2(0,1), vec2(1,1), vec2(1,0), // 远侧面
    // 底面（Y=0）
    vec2(0,0), vec2(1,1), vec2(0,1),  vec2(0,0), vec2(1,0), vec2(1,1), // 内表面
    vec2(0,0), vec2(0,1), vec2(1,1),  vec2(0,0), vec2(1,1), vec2(1,0), // 外表面
    vec2(0,0), vec2(0,1), vec2(1,0),  vec2(0,1), vec2(1,1), vec2(1,0), // 左侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 右侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 近侧面
    vec2(0,0), vec2(0,1), vec2(1,0),  vec2(0,1), vec2(1,1), vec2(1,0), // 远侧面
    // 前面（Z=0）
    vec2(0,0), vec2(1,1), vec2(0,1),  vec2(0,0), vec2(1,0), vec2(1,1), // 内表面
    vec2(0,0), vec2(0,1), vec2(1,1),  vec2(0,0), vec2(1,1), vec2(1,0), // 外表面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 底侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(0,1), vec2(1,0), vec2(1,1), // 顶侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(1,0), vec2(1,1), vec2(0,1), // 左侧面
    vec2(0,0), vec2(1,0), vec2(0,1),  vec2(1,0), vec2(1,1), vec2(0,1), // 右侧面
};

void main() {
    uint index = hardCoded_indexbuffer[gl_VertexIndex];
    vec4 Location = boxCornerPositions[index];
    gl_Position = projection * view * model * Location;
    outTexcoord = hardCoded_UVs[gl_VertexIndex];
}