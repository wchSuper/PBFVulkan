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

// Billboard 模式：粒子数据（顶点属性）
layout(location=0) in vec3 inlocation;
layout(location=1) in vec3 inAnisotropicMatrix0;
layout(location=2) in vec3 inAnisotropicMatrix1;
layout(location=3) in vec3 inAnisotropicMatrix2;

// 输出到 Fragment Shader
layout(location=0) flat out float outviewdepth;
layout(location=1) flat out mat3 outCovMatrix;    // 占用 location 1, 2, 3
layout(location=4) flat out vec3 outParticleCenter;
layout(location=5) flat out float outRmsRadius;
layout(location=6) out vec2 outTexCoord;           // Billboard纹理坐标

mat3 safeInverse3x3(mat3 m) {
    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
              - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
              + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (abs(det) < 1e-6) {
        float meanDiag = max((m[0][0] + m[1][1] + m[2][2]) / 3.0, 1e-12);
        float lam = 0.05 * meanDiag;
        m[0][0] += lam;
        m[1][1] += lam;
        m[2][2] += lam;
        det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
            - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
            + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        if (abs(det) < 1e-8) {
            vec3 diag = vec3(m[0][0], m[1][1], m[2][2]);
            float md = max((diag.x + diag.y + diag.z) / 3.0, 1e-12);
            diag = max(diag, vec3(0.01 * md));
            return mat3(
                1.0/(diag.x), 0.0, 0.0,
                0.0, 1.0/(diag.y), 0.0,
                0.0, 0.0, 1.0/(diag.z)
            );
        }
    }
    float invDet = 1.0 / det;
    mat3 inv;
    inv[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    inv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
    inv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
    inv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
    inv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    inv[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;
    inv[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    inv[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
    inv[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;
    return inv;
}

void main() {
    if (uint(gl_InstanceIndex) >= numParticles) {
        outviewdepth = 0.0;
        outParticleCenter = vec3(0.0);
        outRmsRadius = 0.0;
        outTexCoord = vec2(0.0);
        outCovMatrix = mat3(1.0);
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
    // Billboard 模式：从顶点属性读取
    vec3 particlePos = inlocation;
    mat3 covWorld = mat3(
        inAnisotropicMatrix0,
        inAnisotropicMatrix1,
        inAnisotropicMatrix2
    );
    
    // 计算粒子中心的视图空间位置
    vec4 centerViewPos = view * model * vec4(particlePos, 1.0);
    outviewdepth = centerViewPos.z;
    outParticleCenter = centerViewPos.xyz;
    
    // 将协方差矩阵从世界坐标变换到视图坐标：C_view = (V*M) C_world (V*M)^T
    mat3 M3 = mat3(model);
    mat3 V3 = mat3(view);
    mat3 T = V3 * M3;
    mat3 covView = T * covWorld * transpose(T);
    
    // 基于视图空间RMS半径的全局尺度归一，避免异常值
    float trace = covView[0][0] + covView[1][1] + covView[2][2];
    float rmsRadius = sqrt(max(trace / 3.0, 1e-12));
    float rmsMin = particleRadius * 0.6;
    float rmsMax = particleRadius * 2.4;
    float rmsTarget = clamp(rmsRadius, rmsMin, rmsMax);
    float s = rmsTarget / max(rmsRadius, 1e-12);
    covView *= (s * s);
    rmsRadius = rmsTarget;
    
    outRmsRadius = rmsRadius;
    outCovMatrix = safeInverse3x3(covView);

    // 计算椭球在XY平面的投影半轴
    mat2 A = mat2(outCovMatrix[0][0], outCovMatrix[0][1], outCovMatrix[1][0], outCovMatrix[1][1]);
    float tr2 = A[0][0] + A[1][1];
    float det2 = A[0][0] * A[1][1] - A[0][1] * A[1][0];
    float disc2 = sqrt(max(tr2 * tr2 - 4.0 * det2, 0.0));
    float lambda_max = 0.5 * (tr2 + disc2);
    float lambda_min = 0.5 * (tr2 - disc2);
    float a_max = inversesqrt(max(lambda_min, 1e-12));
    float a_min = inversesqrt(max(lambda_max, 1e-12));
    
    // 计算主轴方向（特征向量）
    vec2 eigenvec;
    if (abs(A[0][1]) > 1e-6) {
        float eval = lambda_max;
        eigenvec = normalize(vec2(A[0][1], eval - A[0][0]));
    } else {
        eigenvec = vec2(1.0, 0.0);
    }
    
    // Billboard 模式：生成4个顶点的quad
    // 使用 gl_VertexIndex 确定当前顶点位置（每个instance绘制4个顶点）
    int vertexID = gl_VertexIndex;
    vec2 quadOffsets[4] = vec2[](
        vec2(-1.0, -1.0),  // 左下 (vertex 0)
        vec2( 1.0, -1.0),  // 右下 (vertex 1)
        vec2(-1.0,  1.0),  // 左上 (vertex 2)
        vec2( 1.0,  1.0)   // 右上 (vertex 3)
    );
    vec2 offset = quadOffsets[vertexID];
    
    // 纹理坐标
    outTexCoord = offset * 0.5 + 0.5;  // 从[-1,1]映射到[0,1]
    
    // 基于椭球主轴构建视图空间的billboard偏移
    vec2 right = eigenvec * a_max;      // 长轴方向
    vec2 up = vec2(-eigenvec.y, eigenvec.x) * a_min;  // 短轴方向（垂直于长轴）
    
    // 应用缩放因子使billboard足够大
    float scaleFactor = 2.8;
    vec2 billboardOffset = (right * offset.x + up * offset.y) * scaleFactor;
    
    // 在视图空间应用偏移
    vec3 vertexViewPos = centerViewPos.xyz + vec3(billboardOffset, 0.0);
    
    // 投影到裁剪空间
    gl_Position = projection * vec4(vertexViewPos, 1.0);
}
