#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec4 Color;
layout(location = 4) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

layout(binding=1) uniform sampler2D baseColorMap;
layout(binding=2) uniform sampler2D normalGLMap;
layout(binding=3) uniform sampler2D aoMap;

const float PI = 3.14159265359;

// Cook-Torrance BRDF
float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

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

void main() {
    // Sample from textures
    vec3 albedo = texture(baseColorMap, TexCoord).rgb;
    vec3 normal_tangent = texture(normalGLMap, TexCoord).rgb * 2.0 - 1.0;
    float ao = texture(aoMap, TexCoord).r;

    // PBR values derived from MTL file (Ns=50, Ni=1.45)
    float metallic = 0.0;  // From low Ks, indicating a non-metal
    float roughness = sqrt(2.0 / (50.0 + 2.0)); // Converted from Ns
    
    vec3 N = normalize(Normal);
    mat3 TBN = cotangent_frame(N, FragPos, TexCoord);
    N = normalize(TBN * normal_tangent);
    vec3 V = normalize(viewPos - FragPos);

    float ior = 1.45; // Index of Refraction from Ni
    float f0_scalar = pow((ior - 1.0) / (ior + 1.0), 2.0);
    vec3 F0 = vec3(f0_scalar);
    F0 = mix(F0, albedo, metallic); // For non-metals, F0 is based on IOR.

    vec3 Lo = vec3(0.0);

    // Point Light - 使用统一光照系统
    vec3 lightPos = lights[0].position;
    vec3 L = normalize(lightPos - FragPos);
    vec3 H = normalize(V + L);
    float distance = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.5 * distance * distance);
    vec3 radiance = vec3(1.0, 1.0, 1.0) * attenuation; // Assuming a white light

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

    // Ambient term modulated by the AO map
    vec3 ambient = vec3(0.1) * albedo * ao;
    vec3 color = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, 1.0);
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