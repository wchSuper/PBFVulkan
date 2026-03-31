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

struct DiffuseParticle{
    vec3 Location;
    vec3 Velocity;

    float Mass;
    float DiffuseBuoyancy;
    float LifeTime;
    uint Type;
};

struct DiffuseParticleCount {
    uint surfaceParticleCount;       // 当前帧检测到的表面粒子数
    uint activeCount;                // 活跃粒子数量
    uint compactCount;               // 紧凑化后的活跃粒子数量
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
    uint numParticles;      // 当前流体粒子数
    uint maxParticles;      // 最大流体粒子数
    uint maxCubeParticles;  // 最大obj粒子数  
    uint cubeParticleNum;   // 当前obj粒子数
    uint rigidParticles;    // 当前刚体粒子数
    uint maxNgbrs;

    float visc;             // 粘度系数 
    float psi;              // 固体粒子影响程度 
    float relaxation;       // 流体粒子速度控制

    // Artificial Viscosity Parameters
    uint  enableArtificialViscosity;  // 是否启用粒子间人工粘性 (0=off, 1=on)
    float artificialViscosityAlpha;   // 人工粘性系数 a
    float artificialViscosityBeta;    // 人工粘性系数 b
    
    // Surface Tension Parameters
    uint  enableSurfaceTension;       // 是否启用表面张力 (0=off, 1=on)
    float surfaceTensionCoefficient;  // 表面张力系数
    float surfaceTensionThreshold;    // 表面张力阈值
    
    // Anisotropic Kernel Parameters
    float anisotropicBaseRadiusScale;    // 基础半径缩放系数
    float anisotropicMaxRatio;           // 最大各向异性比
    float anisotropicMinScale;           // 最小半径缩放
    float anisotropicMaxScale;           // 最大半径缩放
    float anisotropicNeighborThreshold;  // 邻居数阈值

    uint attract;
    vec3 mouse_pos;
    float k;
    vec3 boundary_min;

    // SDF
    vec3  sdfRangeMin;       // sdf作用域最小值坐标
    uint  voxelResolution;   // 体素块分辨率
    float voxelSize;         // 体素块大小

    vec3  sdfRangeMinDy;       // dynamic
    uint  voxelResolutionDy;   
    float voxelSizeDy;         

    bool  isSDF;

    // foam
    uint  maxDiffuseParticles;
    float splashThreshold;
    float surfaceThreshold;     // 表面粒子阈值
    float foamThreshold;        // 泡沫阈值
    float lifeTime;
    
    // Dynamic Wave Simulation (Gerstner Wave + Wind Driven)
    uint  waveEnabled;          // 是否启用动态波浪
    float waveIntensity;        // 波浪总体强度
    float waveSteepness;        // 波浪陡峭度/Q值
    float waveBaseHeight;       // 水面基准高度
    float windDirX;             // 风向X
    float windDirZ;             // 风向Z
    float windStrength;         // 风力强度
    float wavePeriod;           // 主波浪周期
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
layout(binding=14) buffer SDFBuffer{
    float sdfValues[];
};
layout(binding=15) buffer SDFDyBuffer{
    float sdfDynamicValues[];
};

layout(binding=23) buffer EllipsoidBuffer{
    mat4 model;
    vec3 center;
    vec3 radii;
    vec3 accumulatedForce;        // 合力 
    vec3 accumulatedTorque;       // 合力矩
    vec3 velocity;
    vec3 angularVelocity;
    vec4 orientation;
};

layout(push_constant) uniform PushConstants {
    int layer_num;
} push_constant;

// ------------------------------------ 椭球 SDF 函数 ------------------------------------ //
// 椭球SDF快速近似版本
float sdEllipsoid_Fast(vec3 p, vec3 r) {
    // 基于Inigo Quilez的椭球距离近似
    // https://iquilezles.org/articles/ellipsoids/
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    
    return k0 * (k0 - 1.0) / k1;
}

// 椭球SDF主函数
// worldPos: 世界空间中的查询点
// radii: 椭球的三个半径（在局部空间）
// invModel: 模型矩阵的逆（包含平移、旋转、缩放）
float sdEllipsoid(vec3 worldPos, vec3 radii, mat4 invModel) {
    // 世界空间 -> 局部空间
    vec4 localPos4 = invModel * vec4(worldPos, 1.0);
    vec3 localPos = localPos4.xyz;
    return sdEllipsoid_Fast(localPos, radii);
}

// 椭球SDF梯度（法向量）
// worldPos: 世界空间中的查询点
// radii: 椭球的三个半径
// invModel: 模型矩阵的逆
vec3 sdEllipsoidGradient(vec3 worldPos, vec3 radii, mat4 invModel) {
    const float eps = 0.001;
    vec3 gradient;
    gradient.x = sdEllipsoid(worldPos + vec3(eps, 0, 0), radii, invModel) 
               - sdEllipsoid(worldPos - vec3(eps, 0, 0), radii, invModel);
    gradient.y = sdEllipsoid(worldPos + vec3(0, eps, 0), radii, invModel) 
               - sdEllipsoid(worldPos - vec3(0, eps, 0), radii, invModel);
    gradient.z = sdEllipsoid(worldPos + vec3(0, 0, eps), radii, invModel) 
               - sdEllipsoid(worldPos - vec3(0, 0, eps), radii, invModel);
    
    return normalize(gradient);
}

// --------------------------- PBF -------------------------------------------------- // 
float W_Poly6(vec3 r, float h)
{
    float radius = length(r);
    float res = 0.0f;
    if (radius <= h && radius >= 0)
    {
        float item = 1 - pow(radius / h, 2);
        res = coffPoly6 * pow(item, 3);
    }
    return res;
}
float WSpiky(vec3 r, float h)
{
    float radius = length(r);
    float res = 0.0f;
    if (radius <= h && radius >= 0)
    {
        float item = 1 - (radius / h);
        res = coffSpiky * pow(item, 6);
    }
    return res;
}
vec3 Grad_W_Spiky(vec3 r, float h)
{
    float radius = length(r);
    vec3 res = vec3(0.0f, 0.0f, 0.0f);
    if (radius < h && radius > 0)
    {
        float item = 1 - (radius / h);
        res = coffGradSpiky * pow(item, 2) * normalize(r);
    }
    return res;
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
    mat3 X = m; 

    for (int i = 0; i < iterations; i++) {
        mat3 Xinv = inverse3x3(X);
        X = 0.5 * (X + m * Xinv);
    }   
    return X;
}

// ------------------------------------ SDF相关 ------------------------------------ //
uint Clamp(uint value, uint min_, uint max_)
{
    return max(min_, min(value, max_));
}

float SampleSDF(uint dim, uint x, uint y, uint z)
{
    uint vx = Clamp(x, 0, dim - 1);
    uint vy = Clamp(y, 0, dim - 1);
    uint vz = Clamp(z, 0, dim - 1);
	return sdfValues[vy * dim * dim + vz * dim + vx];
}

vec3 SampleSDFGrad(uint dim, uint x, uint y, uint z)
{
    uint x0 = max(0,       x - 1);
    uint x1 = min(dim - 1, x + 1);

    uint y0 = max(0,       y - 1);
    uint y1 = min(dim - 1, y + 1);

    uint z0 = max(0,       z - 1);
    uint z1 = min(dim - 1, z + 1);

    float dx = (SampleSDF(dim, x1, y, z) - SampleSDF(dim, x0, y, z)) / (2.0 * voxelSize);
    float dy = (SampleSDF(dim, x, y1, z) - SampleSDF(dim, x, y0, z)) / (2.0 * voxelSize);
    float dz = (SampleSDF(dim, x, y, z1) - SampleSDF(dim, x, y, z0)) / (2.0 * voxelSize);
    return vec3(dx, dy, dz);
}

vec3 SafeNormalize(vec3 v)
{
    float len = length(v);
    if (len > 0.00001) {
        return v / len;
    }
    return vec3(0.0, 1.0, 0.0);
}


// 简单伪随机 hash 函数 -- 基于 Wang hash
uint wang_hash(uint seed) {
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

// 生成随机浮点 [0,1)
float random_float(uint seed) {
    return float(wang_hash(seed)) / 4294967295.0;  // UINT_MAX
}

vec3 SafeRandNormalize(vec3 v, uint seed) 
{
    float len = length(v);
    if (len > 0.00001) {
        return v / len;
    }
    vec3 defaults[6] = {
        vec3(0.0, 1.0, 0.0),   
        vec3(0.0, -1.0, 0.0),  
        vec3(1.0, 0.0, 0.0),   
        vec3(-1.0, 0.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(0.0, 0.0, -1.0)
    };
    float rand = random_float(seed);
    int index = int(rand * 6.0) % 6;
    return defaults[index];
}

// ================================ 自然波浪模拟系统 ================================ //
// 基于驻波 + 对称波浪 产生自然的水面起伏
// 不会推动流体移动，只产生原地的波动效果

#define PI 3.14159265359

// ---------------------- 驻波 (Standing Wave) ---------------------- //
// 驻波是两个相向传播的波叠加，产生原地上下振动，不会推动流体
float standingWaveHeight(vec2 pos, float wavelength, float time, float phase) {
    float k = 2.0 * PI / wavelength;
    float omega = 2.0 * PI / wavePeriod;
    // 驻波: sin(kx) * cos(ωt) - 空间和时间分离
    return sin(k * pos.x + phase) * cos(k * pos.y + phase * 0.7) * cos(omega * time);
}

// 驻波产生的垂直速度 (对时间求导)
float standingWaveVelocity(vec2 pos, float wavelength, float time, float phase) {
    float k = 2.0 * PI / wavelength;
    // 使用较低的角频率，避免波浪太剧烈
    float omega = 1.0 / wavePeriod;  // 降低角频率
    // d/dt[sin(kx)cos(ωt)] = -ω * sin(kx) * sin(ωt)
    return -omega * sin(k * pos.x + phase) * cos(k * pos.y + phase * 0.7) * sin(omega * time);
}

// ---------------------- 对称 Gerstner 波 ---------------------- //
// 使用相反方向的波对，水平分量相互抵消，保留垂直起伏
vec3 symmetricWaveVelocity(vec2 direction, float amplitude, float wavelength, 
                           vec3 position, float time, float depth) {
    float k = 2.0 * PI / wavelength;
    // 使用较小的角频率，避免过大的速度
    float omega = 1.5 / wavePeriod;
    
    // 深度衰减 (使用更温和的衰减)
    float depthFactor = exp(-2.0 * max(depth, 0.0));
    
    // 正向波
    float phase1 = k * dot(direction, position.xz) - omega * time;
    // 反向波 (相反方向)
    float phase2 = k * dot(-direction, position.xz) - omega * time;
    
    // 水平速度相互抵消，垂直速度叠加
    vec3 vel1 = vec3(
        direction.x * amplitude * cos(phase1),
        amplitude * sin(phase1),
        direction.y * amplitude * cos(phase1)
    );
    vec3 vel2 = vec3(
        -direction.x * amplitude * cos(phase2),
        amplitude * sin(phase2),
        -direction.y * amplitude * cos(phase2)
    );
    
    // 叠加后水平分量大部分抵消，垂直分量增强
    return (vel1 + vel2) * 0.5 * depthFactor;
}

// ---------------------- 多层自然波浪 ---------------------- //
vec3 computeNaturalWaves(vec3 position, float time) {
    if (waveEnabled == 0) {
        return vec3(0.0);
    }
    
    // 计算深度 (相对于水面基准)
    float depth = waveBaseHeight - position.y;
    
    // 太深的粒子不受波浪影响
    if (depth > 0.4) {
        return vec3(0.0);
    }
    
    // 表面因子 - 越接近表面影响越大
    float surfaceFactor = 1.0 - smoothstep(-0.05, 0.25, depth);
    
    vec3 totalVelocity = vec3(0.0);
    
    // 基础振幅
    float baseAmp = 0.025 * waveIntensity;
    float baseWaveLen = 0.5 * wavePeriod;
    
    // 第1层: 主驻波 (X方向)
    float wave1 = standingWaveVelocity(position.xz, baseWaveLen, time, 0.0);
    totalVelocity.y += wave1 * baseAmp * 1.0;
    
    // 第2层: 次驻波 (Z方向, 不同相位)
    float wave2 = standingWaveVelocity(position.zx, baseWaveLen * 0.8, time, PI * 0.3);
    totalVelocity.y += wave2 * baseAmp * 0.7;
    
    // 第3层: 对角驻波
    float wave3 = standingWaveVelocity(
        vec2(position.x + position.z, position.x - position.z) * 0.707,
        baseWaveLen * 0.6, time, PI * 0.7
    );
    totalVelocity.y += wave3 * baseAmp * 0.5;
    
    // 第4层: 高频细节波
    float wave4 = standingWaveVelocity(position.xz, baseWaveLen * 0.3, time * 1.5, PI * 1.2);
    totalVelocity.y += wave4 * baseAmp * 0.3;
    
    // 第5层: 对称Gerstner波 (产生轻微的水平扰动但不会累积)
    totalVelocity += symmetricWaveVelocity(
        vec2(1.0, 0.0), baseAmp * 0.3, baseWaveLen * 0.7,
        position, time, depth
    );
    totalVelocity += symmetricWaveVelocity(
        vec2(0.0, 1.0), baseAmp * 0.25, baseWaveLen * 0.6,
        position, time * 1.1, depth
    );
    totalVelocity += symmetricWaveVelocity(
        normalize(vec2(1.0, 1.0)), baseAmp * 0.2, baseWaveLen * 0.5,
        position, time * 0.9, depth
    );
    
    totalVelocity *= surfaceFactor;
    
    return totalVelocity;
}

// ---------------------- 自然扰动 ---------------------- //
// 小尺度的随机扰动，增加自然感，但保持零均值不会推动流体
vec3 computeNaturalPerturbation(vec3 position, float time) {
    if (waveEnabled == 0) {
        return vec3(0.0);
    }
    
    float depth = waveBaseHeight - position.y;
    float surfaceFactor = 1.0 - smoothstep(-0.03, 0.15, depth);
    
    // 使用多个正弦波叠加产生伪随机扰动
    // 关键：使用对称的波形，确保空间平均为零
    float scale = 0.005 * waveIntensity * waveSteepness * surfaceFactor;  // 降低扰动强度
    
    vec3 perturbation;
    
    // X方向扰动 (使用余弦，空间对称) - 减少水平扰动
    perturbation.x = cos(position.x * 15.0 + time * 1.5) * sin(position.z * 12.0 + time * 1.2) * 0.3;
    
    // Y方向扰动 (主要的垂直波动)
    perturbation.y = sin(position.x * 18.0 + position.z * 16.0 + time * 2.0)
                   + sin(position.x * 25.0 - position.z * 22.0 + time * 2.5) * 0.4;
    
    // Z方向扰动 (使用余弦，空间对称) - 减少水平扰动
    perturbation.z = cos(position.z * 14.0 + time * 1.3) * sin(position.x * 13.0 + time * 1.1) * 0.3;
    
    return perturbation * scale;
}

// ---------------------- 呼吸效果 ---------------------- //
// 整体的缓慢起伏，模拟水体的自然"呼吸"
vec3 computeBreathingEffect(vec3 position, float time) {
    if (waveEnabled == 0) {
        return vec3(0.0);
    }
    
    float depth = waveBaseHeight - position.y;
    float surfaceFactor = 1.0 - smoothstep(-0.1, 0.3, depth);
    
    // 非常缓慢的整体起伏
    float breathFreq = 0.2 / wavePeriod;  // 更慢的呼吸
    
    // 空间变化的呼吸相位
    float spatialPhase = sin(position.x * 2.0) * cos(position.z * 2.0);
    float localBreath = sin(time * breathFreq + spatialPhase * 0.5);
    
    vec3 breathVel = vec3(0.0);
    breathVel.y = localBreath * 0.01 * waveIntensity * surfaceFactor;  // 降低强度
    
    return breathVel;
}

// ---------------------- 主函数 ---------------------- //
vec3 computeDynamicWaveVelocity(vec3 position, float time, uint particleIndex) {
    if (waveEnabled == 0) {
        return vec3(0.0);
    }
    
    vec3 totalVelocity = vec3(0.0);
    
    // 1. 多层自然波浪 (主要效果)
    totalVelocity += computeNaturalWaves(position, time);
    
    // 2. 自然扰动 (细节)
    totalVelocity += computeNaturalPerturbation(position, time);
    
    // 3. 呼吸效果 (整体起伏)
    totalVelocity += computeBreathingEffect(position, time);
    
    return totalVelocity;
}