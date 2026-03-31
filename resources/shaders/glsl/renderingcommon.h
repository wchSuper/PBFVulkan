// ==================== 光源结构体 ====================
// 光源类型: 0 = 点光源, 1 = 平行光
struct LightSource {
    vec3 position;      // 点光源位置 / 平行光方向
    vec3 color;         // 光源颜色
    float intensity;    // 光源强度
    uint type;          // 0=点光源, 1=平行光
    uint enabled;       // 是否启用
    float _padding;     // 对齐填充
};

layout(binding=0) uniform UniformRenderingObject{
    float zNear;
    float zFar;
    float fovy;
    float aspect;

    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 inv_projection;
    mat4 inv_view;          // 视图矩阵的逆，用于将视空间方向转换到世界空间
    float particleRadius;
    float rigidRadius;
    uint renderType;
    uint fluidType;
    vec4 fluid_color;
    vec3 camera_pos;
    vec3 camera_view;
    vec3 camera_up;

    // ==================== 统一光照系统 (最多3个光源) ====================
    LightSource lights[3];      // 3个光源
    uint numActiveLights;       // 当前激活的光源数量
    
    // 环境光
    vec3 ambientColor;
    
    // 焦散参数
    float causticsIntensity;      // 焦散强度
    float causticsPhotonRadius;   // 光子影响半径
    
    // 折射率 (用于焦散色散)
    float refractionIOR;          // 水的折射率
    float redIOR;                 // 红光折射率
    float greenIOR;               // 绿光折射率
    float blueIOR;                // 蓝光折射率

    bool isFilter;
    uint filterRadius;
};

// ==================== 光照辅助函数 ====================
// 获取光源方向 (从表面点指向光源)
vec3 getLightDirection(LightSource light, vec3 surfacePos) {
    if (light.type == 0u) {
        // 点光源: 从表面点指向光源位置
        return normalize(light.position - surfacePos);
    } else {
        // 平行光: 直接使用方向 (取反，因为存储的是光线方向)
        return normalize(-light.position);
    }
}

// 获取主光源位置 (用于焦散计算等)
vec3 getMainLightPosition() {
    return lights[0].position;
}

// 获取主光源颜色
vec3 getMainLightColor() {
    return lights[0].color * lights[0].intensity;
}

// ---------------------------- Ray Tracing相关 ----------------------------// 
float LinearizeDepth(float depth) {
    float near = 0.1;
    float far = 100.0;
    float z = depth * 2.0 - 1.0; 
    return (2.0 * near * far) / (far + near - z * (far - near));    
}

struct Ray {
    vec3 origin;
    vec3 direction;
    float maxDistance;
};

struct RayHitResult {
    bool isHit;
    bool isEnvMap;    // 是否是环境映射（未真实命中）
    vec2 uv;
    float distance;
};

struct FluidMaterial {
    vec3 baseColor;
    float baseColorStrength;
    float ior;                  // 折射率
    vec3 absorptionColor;       // 吸收颜色
    float absorptionStrength;   // 吸收强度
    float reflectivity;         // 反射率
    float roughness;            // 粗糙度
    float scatterStrength;      // 散射强度
    vec3 scatterColor;          // 散射颜色
};
