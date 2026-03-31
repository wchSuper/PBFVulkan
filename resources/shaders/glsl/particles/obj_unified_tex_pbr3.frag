#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec4 Color;
layout(location = 4) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

layout(binding=1) uniform sampler2D baseColorMap;      // 基础颜色贴图
layout(binding=2) uniform sampler2D normalGLMap;       // 法线贴图
layout(binding=3) uniform sampler2D roughnessMap;      // 粗糙度贴图
layout(binding=4) uniform sampler2D metallicMap;       // 金属度贴图
layout(binding=5) uniform sampler2D aoMap;             // 环境光遮蔽贴图
layout(binding=6) uniform sampler2D displacementMap;   // 置换贴图

const float PI = 3.14159265359;
const float DISPLACEMENT_SCALE = 0.02; // 置换贴图缩放因子

// Cook-Torrance BRDF
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

// function to create a TBN matrix from normal and derivatives
mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    
    float invmax = inversesqrt(max(dot(T,T), dot(B,B)));
    return mat3(T * invmax, B * invmax, N);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // 应用置换贴图调整片段位置
    float displacement = texture(displacementMap, TexCoord).r;
    vec3 adjustedFragPos = FragPos + Normal * displacement * DISPLACEMENT_SCALE;
    
    // 采样所有贴图
    vec4 albedoSample = texture(baseColorMap, TexCoord);
    vec3 albedo = albedoSample.rgb;
    float alpha = albedoSample.a;
    
    vec3 normal_tangent = texture(normalGLMap, TexCoord).rgb * 2.0 - 1.0;
    
    // 从独立的贴图中采样粗糙度和金属度
    float roughness = texture(roughnessMap, TexCoord).r;
    float metallic = texture(metallicMap, TexCoord).r;
    
    // 环境光遮蔽
    float ao = texture(aoMap, TexCoord).r;

    // 法线映射
    vec3 N = normalize(Normal);
    mat3 TBN = cotangent_frame(N, FragPos, TexCoord);
    N = normalize(TBN * normal_tangent);
    
    vec3 V = normalize(viewPos - adjustedFragPos);
    vec3 R = reflect(-V, N);

    // 计算基础反射率
    vec3 F0 = vec3(0.04); // 非金属的默认F0
    F0 = mix(F0, albedo, metallic);

    // 计算反射方程
    vec3 Lo = vec3(0.0);

    // 点光源 - 使用统一光照系统
    vec3 lightPos = lights[0].position;
    vec3 lightColor = lights[0].color * lights[0].intensity;
    vec3 L = normalize(lightPos - adjustedFragPos);
    vec3 H = normalize(V + L);
    float distance = length(lightPos - adjustedFragPos);
    float attenuation = 1.0 / (1.0 + 0.5 * distance * distance);
    vec3 radiance = lightColor * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G = GeometrySmith(N, V, L, roughness);      
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	  
        
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);                
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    // 环境光照（IBL的简化版本）
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    // 最终颜色（没有自发光）
    vec3 color = ambient + Lo;
	
    // HDR色调映射
    color = color / (color + vec3(1.0));
    
    // Gamma校正
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, alpha);
}