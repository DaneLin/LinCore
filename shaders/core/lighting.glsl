#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "light_structures.glsl"
#include "brdf.glsl"

// 计算衰减
float CalculateAttenuation(vec3 attenuation_params, float distance) {
    float constant = attenuation_params.x;
    float linear = attenuation_params.y;
    float quadratic = attenuation_params.z;
    return 1.0 / (constant + linear * distance + quadratic * distance * distance);
}

// 计算各种光源的辐照度
vec3 CalculateDirectionalLight(Light light, vec3 worldPos, vec3 normal)
{
    vec3 lightDir = normalize(-GetLightDirection(light));
    float NdotL = max(dot(normal, lightDir), 0.0);
    return GetLightColor(light) * GetLightIntensity(light) * NdotL;
}

vec3 CalculatePointLight(Light light, vec3 worldPos, vec3 normal)
{
    vec3 lightDir = normalize(GetLightPosition(light) - worldPos);
    float distance = length(GetLightPosition(light) - worldPos);
    
    // 使用新的衰减计算
    float attenuation = CalculateAttenuation(GetAttenuation(light), distance);
    
    // 范围衰减
    float range = GetLightRange(light);
    float rangeAttenuation = 1.0 - smoothstep(range * 0.75, range, distance);
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    return GetLightColor(light) * GetLightIntensity(light) * attenuation * rangeAttenuation * NdotL;
}

vec3 CalculateSpotLight(Light light, vec3 worldPos, vec3 normal)
{
    vec3 lightDir = normalize(GetLightPosition(light) - worldPos);
    float distance = length(GetLightPosition(light) - worldPos);
    
    // 使用新的衰减计算
    float attenuation = CalculateAttenuation(GetAttenuation(light), distance);
    
    // 范围衰减
    float range = GetLightRange(light);
    float rangeAttenuation = 1.0 - smoothstep(range * 0.75, range, distance);
    
    // 角度衰减
    vec2 angles = GetSpotAngles(light);
    float theta = dot(lightDir, normalize(-GetLightDirection(light)));
    float epsilon = angles.x - angles.y;
    float intensity = clamp((theta - angles.y) / epsilon, 0.0, 1.0);
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    return GetLightColor(light) * GetLightIntensity(light) * attenuation * rangeAttenuation * intensity * NdotL;
}

vec3 CalculateAreaLight(Light light, vec3 worldPos, vec3 normal)
{
    vec3 lightDir = normalize(GetLightPosition(light) - worldPos);
    float distance = length(GetLightPosition(light) - worldPos);
    
    // 使用新的衰减计算
    float attenuation = CalculateAttenuation(GetAttenuation(light), distance);
    
    // 范围衰减
    float range = GetLightRange(light);
    float rangeAttenuation = 1.0 - smoothstep(range * 0.75, range, distance);
    
    // 面积衰减
    vec2 size = GetAreaSize(light);
    float area = size.x * size.y;
    float areaAttenuation = area / (distance * distance) * RECIPROCAL_PI;
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    return GetLightColor(light) * GetLightIntensity(light) * attenuation * rangeAttenuation * areaAttenuation * NdotL;
}

// 计算单个光源的贡献
vec3 CalculateLightContribution(Light light, vec3 worldPos, vec3 normal, vec3 viewDir, vec3 albedo, float roughness, vec3 F0)
{
    if (!IsLightEnabled(light)) return vec3(0.0);
    
    vec3 radiance;
    vec3 lightDir;
    
    uint lightType = GetLightType(light);
    if (lightType == LIGHT_DIRECTIONAL) {
        radiance = CalculateDirectionalLight(light, worldPos, normal);
        lightDir = normalize(-GetLightDirection(light));
    }
    else if (lightType == LIGHT_POINT) {
        radiance = CalculatePointLight(light, worldPos, normal);
        lightDir = normalize(GetLightPosition(light) - worldPos);
    }
    else if (lightType == LIGHT_SPOT) {
        radiance = CalculateSpotLight(light, worldPos, normal);
        lightDir = normalize(GetLightPosition(light) - worldPos);
    }
    else { // LIGHT_AREA
        radiance = CalculateAreaLight(light, worldPos, normal);
        lightDir = normalize(GetLightPosition(light) - worldPos);
    }
    
    vec3 H = normalize(viewDir + lightDir);
    
    // Cook-Torrance BRDF
    float NDF = D_GGX(max(dot(normal, H), 0.0), roughness);
    float G = G_Smith(max(dot(normal, viewDir), 0.0), max(dot(normal, lightDir), 0.0), roughness);
    vec3 F = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    return (kD * albedo * RECIPROCAL_PI + specular) * radiance * NdotL;
}

#endif // LIGHTING_GLSL 