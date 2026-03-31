struct Particle{
    vec3 Location;
    vec3 Velocity;
    vec3 DeltaLocation;
    float Lambda;
    float Density;
    float Mass;

    vec3 TmpVelocity;

    uint CellHash;
    uint TmpCellHash;

    uint NumNgbrs;
};

struct CubeParticle{
    vec3 Location;
    vec3 Velocity;
    vec3 DeltaLocation;
    float Lambda;
    float Density;
    float Mass;

    vec3 TmpVelocity;

    uint CellHash;
    uint TmpCellHash;

    uint NumNgbrs;
};

struct ExtraParticle{
    vec3 Location;
    vec3 Velocity;
    vec3 DeltaLocation;
    float Lambda;
    float Density;
    float Mass;

    vec3 TmpVelocity;

    uint CellHash;
    uint TmpCellHash;

    uint NumNgbrs;
    uint type;
};

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

layout(binding=0) uniform SimulateObj{
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
    uint maxCubeParticles;
    uint cubeParticleNum;
    uint rigidParticles;
    uint maxNgbrs;

    float visc;
    float psi;
    float relaxation;

    uint  enableArtificialViscosity;
    float artificialViscosityAlpha;
    float artificialViscosityBeta;

    uint  enableSurfaceTension;
    float surfaceTensionCoefficient;
    float surfaceTensionThreshold;

    float anisotropicBaseRadiusScale;
    float anisotropicMaxRatio;
    float anisotropicMinScale;
    float anisotropicMaxScale;
    float anisotropicNeighborThreshold;

    uint attract;
    vec3 mouse_pos;
    float k;
    vec3 boundary_min;

    vec3  sdfRangeMin;
    uint  voxelResolution;
    float voxelSize;

    vec3  sdfRangeMinDy;
    uint  voxelResolutionDy;
    float voxelSizeDy;

    bool  isSDF;

    uint  maxDiffuseParticles;
    float splashThreshold;
    float surfaceThreshold;
    float foamThreshold;
    float lifeTime;

    uint  waveEnabled;
    float waveIntensity;
    float waveSteepness;
    float waveBaseHeight;
    float windDirX;
    float windDirZ;
    float windStrength;
    float wavePeriod;
};

layout(binding=1) readonly buffer ParticleSSBOIn{
    Particle particlesIn[];
};
layout(binding=2) buffer ParticleSSBOout{
    Particle particlesOut[];
};
layout(binding=3) uniform Boxinfo{
    vec2 boxClampX;
    vec2 boxClampY;
    vec2 boxClampZ;

    vec2 clampX_still;
    vec2 clampY_still;
    vec2 clampZ_still; 
};

layout(binding=4) buffer CellInfoBuffer{
    uint cellinfo[];
};
layout(binding=5) buffer NeighborCountBuffer{
    uint neighborCount[];
};
layout(binding=6) buffer NeighborCpyCountBuffer{
    uint neighborCpyCount[];
};
layout(binding=7) buffer ParticleInCellBuffer{
    uint particleInCell[];
};
layout(binding=8) buffer CellOffsetBuffer{
    uint cellOffset[];
};
layout(binding=9) buffer TestPBFBuffer{
    uint testPBF[];
};
layout(binding=10) buffer CubeParticleBuffer{
    CubeParticle cubeParticles[];
};
layout(binding=11) buffer RigidParticleBuffer{
    ExtraParticle rigidParticlesInfo[];
};
layout(binding=12) buffer ShapeMatchingBuffer{
    ShapeMatchingInfo smi;
};
layout(binding=13) buffer OriginRigidParticleBuffer{
    ExtraParticle originRigidParticlesInfo[];
};

layout(binding=16) buffer WCSPHPressureBuffer {
    float wcsphPressure[];
};

layout(binding=18) buffer WCSPHExternalForceBuffer {
    vec4 wcsphExternalForce[];
};

layout(binding=22) readonly buffer WCSPHParamsBuffer {
    vec4 wcsphParams;
};

layout(push_constant) uniform PushConstants {
    int layer_num;
} push_constant;

#define PI 3.14159265358979323846

// 立方样条核函数值 W(|r|, h)
float cubicKernel(float r, float h, int dim) {
    // 归一化系数 k
    float k = (dim == 2) ? 40.0 / (7.0 * PI) : 8.0 / PI;
    k /= pow(h, dim);

    float q = r / h;
    if (q > 1.0) return 0.0;
    if (q <= 0.5)
    {
        // 6q³ − 6q² + 1
        float q2 = q * q;
        float q3 = q2 * q;
        return k * (6.0 * q3 - 6.0 * q2 + 1.0);
    }
    else
    {
        // 2(1 − q)³
        float oneMinusQ = 1.0 - q;
        return k * 2.0 * oneMinusQ * oneMinusQ * oneMinusQ;
    }
}

float cubicKernel(float r, float h) {
    return cubicKernel(r, h, 3);
}

// 核函数梯度 ∇W(r,h) 
vec3 cubicKernelDeriv(vec3 r, float h, int dim) {
    float len = length(r);
    float q   = len / h;

    // 超出支持半径或距离过小 → 梯度为 0
    if (len < 1e-5 || q > 1.0) return vec3(0.0);

    // 归一化系数 k'
    float k = (dim == 2) ? 40.0 / (7.0 * PI) : 8.0 / PI;
    k = 6.0 * k / pow(h, dim);

    vec3 grad_q = r / (len * h);      // ∂q/∂r = r/(|r|h)

    if (q <= 0.5)
    {
        // q(3q − 2)
        return k * q * (3.0 * q - 2.0) * grad_q;
    }
    else
    {
        // −(1 − q)²
        float fac = 1.0 - q;
        return k * (-fac * fac) * grad_q;
    }
}

vec3 cubicKernelDeriv(vec3 r, float h) {
    return cubicKernelDeriv(r, h, 3);
}

float poly6Kernel(float r, float h) {
    if (r >= h) return 0.0;
    float h2 = h * h;
    float diff = h2 - r * r;
    float scale = 315.0 / (64.0 * PI * pow(h, 9));
    return scale * diff * diff * diff;
}

float nearDensityKernel(float r, float h) {
    if (r >= h) return 0.0;
    float d = h - r;
    float scale = 15.0 / (PI * pow(h, 6));
    return scale * d * d * d;
}

float spikyGradMag(float r, float h) {
    if (r >= h) return 0.0;
    float scale = 45.0 / (PI * pow(h, 6));
    float d = h - r;
    return scale * d * d;
}

float nearSpikyGradMag(float r, float h) {
    if (r >= h) return 0.0;
    float scale = 45.0 / (PI * pow(h, 5));
    float d = h - r;
    return scale * d * d;
}

float viscosityLaplacian(float r, float h) {
    if (r >= h) return 0.0;
    float scale = 45.0 / (PI * pow(h, 6));
    float d = h - r;
    return scale * d;
}

uint FlatCell(vec3 cellPos)
{
    uint gridCount_1 = uint(spaceSize / (0.01 * 2)) + 8; 
    return uint(uint(cellPos.x) + uint(cellPos.y) * gridCount_1 + uint(cellPos.z) * gridCount_1 * gridCount_1);
}

vec3 cell_offset[] = {
    {-1, -1, -1}, {-1, -1, 0}, {-1, -1, 1}, {-1, 0, -1}, {-1, 0, 0}, {-1, 0, 1}, {-1, 1, -1}, {-1, 1, 0}, {-1, 1, 1},
    {0, -1, -1}, {0, -1, 0}, {0, -1, 1}, {0, 0, -1}, {0, 0, 0}, {0, 0, 1}, {0, 1, -1}, {0, 1, 0}, {0, 1, 1},
    {1, -1, -1}, {1, -1, 0}, {1, -1, 1}, {1, 0, -1}, {1, 0, 0}, {1, 0, 1}, {1, 1, -1}, {1, 1, 0}, {1, 1, 1}
};

// 矩阵行列式 3 x 3
float det3x3(mat3 m) {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

// 矩阵的逆 3 x 3
mat3 inverse3x3(mat3 m) {
    float d = det3x3(m);
    if (abs(d) < 0.0001) return mat3(1.0); // 避免除零错误
    
    float invd = 1.0 / d;
    
    mat3 adj;
    adj[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invd;
    adj[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invd;
    adj[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invd;
    adj[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invd;
    adj[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invd;
    adj[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invd;
    adj[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invd;
    adj[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invd;
    adj[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invd;
    
    return adj;
}

// 计算对称正定矩阵的平方根的快速近似
mat3 sqrtm_approximation(mat3 m, int iterations) {
    // 使用牛顿迭代法: X_{n+1} = 0.5 * (X_n + M * X_n^(-1))
    mat3 X = m; // 初始猜测
    
    for (int i = 0; i < iterations; i++) {
        mat3 Xinv = inverse3x3(X);
        X = 0.5 * (X + m * Xinv);
    }
    
    return X;
}