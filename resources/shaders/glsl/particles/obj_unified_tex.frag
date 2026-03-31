#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoord;
layout(location = 3) in vec4 Color;
layout(location = 4) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

layout(binding=1) uniform sampler2D ObjTexture;

void main() {
    // Triplanar mapping
	
	// 1. Blending weights based on world normal
	vec3 blending = normalize(pow(abs(Normal), vec3(4.0))); // The power sharpens the blend
	blending /= dot(blending, vec3(1.0));

	// 2. Texture coordinates from three planes (XY, YZ, XZ) based on world position
	vec2 texCoordX = FragPos.yz;
	vec2 texCoordY = FragPos.xz;
	vec2 texCoordZ = FragPos.xy;

	// 3. Sample the texture from three directions
	vec4 colorX = texture(ObjTexture, texCoordX);
	vec4 colorY = texture(ObjTexture, texCoordY);
	vec4 colorZ = texture(ObjTexture, texCoordZ);

	// 4. Blend the three samples together
	vec4 albedo = colorX * blending.x + colorY * blending.y + colorZ * blending.z;

    // 获取光照参数
    vec3 lightPos = lights[0].position;
    vec3 lightColor = lights[0].color * lights[0].intensity;

    // Ambient
    float ambientStrength = 0.99;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * albedo.rgb;
	outColor = vec4(result, albedo.a);
}