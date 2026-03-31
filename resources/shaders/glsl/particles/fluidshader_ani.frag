#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location=0) out float outdepth;
layout(location=1) out float outthickness;

layout(location=0) flat in float inviewdepth;
layout(location=1) flat in mat3 outCovMatrix;    // 占用 location 1, 2, 3
layout(location=4) flat in vec3 outParticleCenter;
layout(location=5) flat in float outRmsRadius;
layout(location=6) in vec2 inTexCoord;           // Billboard纹理坐标

// 计算椭球体方程值
float computeEllipsoidValue(vec3 point, mat3 invMatrix) {
    float value = 0.0;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            value += point[i] * invMatrix[i][j] * point[j];
        }
    }
    return value;
}

void main() {
    // 获取视距
    float viewDistance = -inviewdepth;
    
    // 获取逆协方差矩阵与RMS半径（视图空间）
    mat3 invMatrix = outCovMatrix;
    float rmsRadius = outRmsRadius;
    
    // Billboard 模式：使用纹理坐标
    vec2 posN = inTexCoord - vec2(0.5);  // 从[0,1]映射到[-0.5,0.5]
    
    // 椭球阈值
    float ellipsoidThreshold = 1.0;
    
    // 计算视图空间中的XY坐标（基于椭球投影）
    // 重新计算椭球在XY平面的半轴
    mat2 A = mat2(invMatrix[0][0], invMatrix[0][1], invMatrix[1][0], invMatrix[1][1]);
    float tr2 = A[0][0] + A[1][1];
    float det2 = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    float disc2 = sqrt(max(tr2 * tr2 - 4.0 * det2, 0.0));
    float lambda_max = 0.5 * (tr2 + disc2);
    float lambda_min = 0.5 * (tr2 - disc2);
    float a_max = inversesqrt(max(lambda_min, 1e-12));
    float a_min = inversesqrt(max(lambda_max, 1e-12));
    
    // 计算主轴方向
    vec2 eigenvec;
    if (abs(A[0][1]) > 1e-6) {
        float eval = lambda_max;
        eigenvec = normalize(vec2(A[0][1], eval - A[0][0]));
    } else {
        eigenvec = vec2(1.0, 0.0);
    }
    
    // 基于椭球主轴缩放纹理坐标
    vec2 right = eigenvec * a_max;
    vec2 up = vec2(-eigenvec.y, eigenvec.x) * a_min;
    float scaleFactor = 2.8;
    vec2 xyOffset = (right * posN.x * 2.0 + up * posN.y * 2.0) * scaleFactor;
    
    vec3 point = vec3(xyOffset, 0.0);
    
    // 计算椭球体方程值
    float ellipsoidValXY = computeEllipsoidValue(point, invMatrix);
    
    // 如果XY平面上超出椭球，丢弃片段
    if (ellipsoidValXY > ellipsoidThreshold) {
        discard;
    }
    
    // 计算椭球体表面z值
    // 求解椭球方程 ellipsoidValXY + z相关项 = ellipsoidThreshold
    float a = invMatrix[2][2];
    float b = 2.0 * (invMatrix[0][2] * point.x + invMatrix[1][2] * point.y);
    float c = ellipsoidValXY - ellipsoidThreshold;
    
    float z;
    float discriminant = 0.0;
    
    if (a <= 0.0 || abs(a) < 1e-6) {
        // 退化情况
        float xyFactor = 1.0 - min(ellipsoidValXY / ellipsoidThreshold, 0.99);
        float zScale = sqrt(max(invMatrix[0][0], invMatrix[1][1]) / max(1e-6, invMatrix[2][2]));
        z = rmsRadius * sqrt(max(0.0, xyFactor)) * zScale;
    }
    else {
        // 求解二次方程 az^2 + bz + c = 0
        discriminant = b*b - 4.0*a*c;
        
        if (discriminant < 0.0) {
            float xyFactor = 1.0 - min(ellipsoidValXY / ellipsoidThreshold, 0.99);
            float zScale = sqrt(max(invMatrix[0][0], invMatrix[1][1]) / max(1e-6, invMatrix[2][2]));
            z = rmsRadius * sqrt(max(0.0, xyFactor)) * zScale;
        } 
        else {
            // 取较大解（向观察者方向）
            z = (-b + sqrt(discriminant)) / (2.0*a);
            
            // 检查结果合理性
            if (z <= 0.0 || z > 5.0 * rmsRadius || isnan(z) || isinf(z)) {
                float xyFactor = 1.0 - min(ellipsoidValXY / ellipsoidThreshold, 0.99);
                float zScale = sqrt(max(invMatrix[0][0], invMatrix[1][1]) / max(1e-6, invMatrix[2][2]));
                z = rmsRadius * sqrt(max(0.0, xyFactor)) * zScale;
            }
        }
    }
    
    // 计算视图空间深度
    float viewdepth = inviewdepth + z;
    
    // 投影到裁剪空间
    vec4 temp = projection * vec4(0.0, 0.0, viewdepth, 1.0);
    outdepth = temp.z / temp.w;
    
    // 计算厚度
    float backZ = 0.0;
    if (a > 0.0 && discriminant >= 0.0) {
        backZ = (-b - sqrt(discriminant)) / (2.0*a);
        if (backZ > 0.0 || isnan(backZ) || isinf(backZ)) {
            backZ = -z * 0.8;
        }
    } else {
        backZ = -z * 0.8;
    }
    
    float thickness = abs(z - backZ);
    
    // 近距离提高厚度
    if (viewDistance < rmsRadius * 10.0) {
        float distanceFactor = 1.0 + 0.4 * (1.0 - viewDistance / (rmsRadius * 10.0));
        thickness *= distanceFactor;
    }
    
    // 限制厚度范围
    float minThickness = rmsRadius * 0.8;
    float maxThickness = 4.0 * rmsRadius;
    
    outthickness = clamp(thickness, minThickness, maxThickness);
}
