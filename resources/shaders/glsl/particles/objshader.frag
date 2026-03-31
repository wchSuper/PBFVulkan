#version 450
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {

    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); 
    vec3 normal = normalize(fragNormal);
    float diff = max(dot(normal, lightDir), 0.0);
    
    outColor = vec4(fragTexCoord, 0.0, 1.0);
}