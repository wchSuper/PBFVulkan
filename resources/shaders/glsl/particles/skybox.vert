#version 450
#extension GL_ARB_shading_language_include : require
#include "renderingcommon.h"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragTexCoord;

void main() {
    fragTexCoord = inPosition;
    fragTexCoord = vec3(fragTexCoord.x, fragTexCoord.y, fragTexCoord.z); 
    mat4 viewNoTranslation = mat4(mat3(view));   // 移除平移分量
    vec4 pos = projection * viewNoTranslation * vec4(inPosition, 1.0);
    gl_Position = pos.xyww;                      // 深度设为1.0
}