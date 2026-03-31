#version 450

layout(location=0) flat in float inviewdepth;
layout(location=1) flat in uint intype;

layout(location=0) out vec4 outdepth;
layout(location=1) out vec4 outid;

void main(){
    // 根据粒子类型设置颜色
    vec3 color;
    if (intype == 1) {
        // Spray/Foam: 白色
        color = vec3(1.0, 1.0, 1.0);
    } else if (intype == 2) {
        // Bubble: 淡蓝色
        color = vec3(0.7, 0.9, 1.0);
    } else {
        // 未知类型: 灰色
        color = vec3(0.5, 0.5, 0.5);
    }

    outdepth = vec4(inviewdepth, color);
    outid = vec4(0, 0, 0, 0);
}
