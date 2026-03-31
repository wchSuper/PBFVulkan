#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location=0) out vec4 fragColor;

layout(location=0) flat in float inviewdepth;

void main(){
    float radius = particleRadius;
    vec2 pos = gl_PointCoord - vec2(0.5);
    if(length(pos) > 0.5){
        discard;
        return;
    }
    float l = radius*2*length(pos);
    float viewdepth = inviewdepth + sqrt(radius*radius - l*l);
    
    vec4 temp = vec4(0,0,viewdepth,1);
    
    temp = projection*temp;

    float depth = temp.z/temp.w;

    float thickness = 4*sqrt(radius*radius - l*l);
    
    // 计算法线 - 基于点坐标
    vec3 normal = vec3(pos.x, pos.y, sqrt(0.25 - pos.x*pos.x - pos.y*pos.y));
    normal = normalize(normal);
    
    // 视线方向（假设为z轴方向）
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    
    // 基础海洋颜色
    vec3 shallowColor = vec3(0.1, 0.5, 0.8);   // 浅海蓝色
    vec3 deepColor = vec3(0.0, 0.2, 0.4);      // 深海蓝色
    
    // 基于厚度混合颜色
    float depthFactor = clamp(thickness / (radius*4), 0.0, 1.0);
    vec3 oceanColor = mix(shallowColor, deepColor, depthFactor);
    
    // 光照方向
    vec3 lightDir = normalize(vec3(0.5, 0.5, 1.0));
    
    // 漫反射
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * oceanColor;
    
    // 镜面反射
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = vec3(0.5) * spec;
    
    // 环境光
    vec3 ambient = oceanColor * 0.2;
    
    // 菲涅尔效应（边缘发光）
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 4.0);
    vec3 fresnelColor = mix(oceanColor, vec3(0.8, 0.9, 1.0), 0.8);
    
    // 最终颜色
    vec3 finalColor = ambient + diffuse + specular + fresnelColor * fresnel * 0.4;
    
    // 透明度随边缘递减 - 使用平滑的边缘来改善视觉效果
    float edgeStart = 0.35; // 从粒子半径的35%处开始过渡
    float edgeEnd = 0.5;   // 到达粒子半径的50%处（边缘）完全透明
    float dist = length(pos);
    float alpha = 1.0 - smoothstep(edgeStart, edgeEnd, dist);

    alpha *= clamp(thickness / (radius * 2.0), 0.0, 1.0);
    
    // 输出最终颜色
    fragColor = vec4(finalColor, alpha);
}