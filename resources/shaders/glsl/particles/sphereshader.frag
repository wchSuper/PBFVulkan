#version 450
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inViewDir;

layout(binding = 1) uniform samplerCube skybox;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(inNormal);
    vec3 viewDir = normalize(inViewDir);
    
    // 计算从相机指向片段的入射向量的反射向量
    vec3 reflectDir = reflect(-viewDir, normal);
    
    // 从天空盒采样反射颜色
    vec4 reflectColor = texture(skybox, reflectDir);

    // 增加反射的亮度和对比度，使其看起来更光滑、更亮
    // 一个较高的亮度值会让反射显得更清晰锐利，从而感觉更"光滑"
    float brightness = 2.0; // 您可以调整此值来获得期望的效果
    vec3 finalColor = reflectColor.rgb * brightness;

    outColor = vec4(finalColor, reflectColor.a);
}