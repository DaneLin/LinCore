#ifndef LIGHT_STRUCTURES_GLSL
#define LIGHT_STRUCTURES_GLSL

// 光源类型
const uint LIGHT_DIRECTIONAL = 0;
const uint LIGHT_POINT = 1;
const uint LIGHT_SPOT = 2;
const uint LIGHT_AREA = 3;

// 压缩的光源数据结构
struct Light {
    vec4 color_intensity;     // rgb: 颜色, a: 强度
    vec4 position_range;      // xyz: 位置/方向, w: 范围/未使用(平行光)
    vec4 direction_angles;    // xyz: 方向(聚光灯)/面光源尺寸(xy)+pad, w: 内角外角(聚光灯,xy分量)
    vec4 attenuation;         // xyz: 常数项/线性项/二次项, w: 类型(低30位)和启用标志(最高位)
};

// 辅助函数
uint GetLightType(Light light) {
    return uint(uintBitsToFloat(floatBitsToUint(light.attenuation.w) & 0x7FFFFFFFu));
}

bool IsLightEnabled(Light light) {
    return (floatBitsToUint(light.attenuation.w) & 0x80000000u) != 0u;
}

vec3 GetLightColor(Light light) {
    return light.color_intensity.rgb;
}

float GetLightIntensity(Light light) {
    return light.color_intensity.a;
}

vec3 GetLightPosition(Light light) {
    return light.position_range.xyz;
}

float GetLightRange(Light light) {
    return light.position_range.w;
}

vec3 GetLightDirection(Light light) {
    return light.direction_angles.xyz;
}

vec2 GetSpotAngles(Light light) {
    uint packed = floatBitsToUint(light.direction_angles.w);
    float inner = float(packed >> 16) / 65535.0;
    float outer = float(packed & 0xFFFFu) / 65535.0;
    return vec2(inner, outer);
}

vec2 GetAreaSize(Light light) {
    return light.direction_angles.xy;
}

vec3 GetAttenuation(Light light) {
    return light.attenuation.xyz;
}

#endif // LIGHT_STRUCTURES_GLSL 