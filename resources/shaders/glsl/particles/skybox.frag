#version 450
layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform samplerCube skybox;

void main() {
    vec4 texColor = texture(skybox, fragTexCoord);
    
    // 增加亮度
    float brightnessMultiplier = 2.0; 
    vec3 brightened = texColor.rgb * brightnessMultiplier;
    
    // 输出增亮后的颜色
    outColor = vec4(brightened, texColor.a);
}