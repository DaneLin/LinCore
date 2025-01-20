#version 450

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

// GBuffer输入
layout (input_attachment_index = 0, binding = 0) uniform subpassInput inPosition;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput inNormal;
layout (input_attachment_index = 2, binding = 2) uniform subpassInput inAlbedo;
layout (input_attachment_index = 3, binding = 3) uniform subpassInput inEmission;

// 光源类型
const int LIGHT_DIRECTIONAL = 0;
const int LIGHT_POINT = 1;
const int LIGHT_SPOT = 2;
const int LIGHT_AREA = 3;

// 光源数据结构
struct Light {
    vec3 color;
    float intensity;
    vec3 position;
    float range;
    vec3 direction;
    float inner_angle;
    vec2 size;
    float outer_angle;
    int type;
    int enabled;
    vec2 pad;
};

// 场景数据
layout(std140, binding = 4) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec3 camera_pos;
    uint num_lights;
    Light lights[64];
} scene;

// PBR材质参数
const float PI = 3.14159265359;

// PBR函数
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 计算各种光源的辐照度
vec3 CalculateDirectionalLight(Light light, vec3 worldPos, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    float NdotL = max(dot(normal, lightDir), 0.0);
    return light.color * light.intensity * NdotL;
}

vec3 CalculatePointLight(Light light, vec3 worldPos, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - worldPos);
    float distance = length(light.position - worldPos);
    float attenuation = 1.0 / (1.0 + distance * distance);
    float NdotL = max(dot(normal, lightDir), 0.0);
    return light.color * light.intensity * attenuation * NdotL;
}

vec3 CalculateSpotLight(Light light, vec3 worldPos, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - worldPos);
    float distance = length(light.position - worldPos);
    float attenuation = 1.0 / (1.0 + distance * distance);
    
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.inner_angle - light.outer_angle;
    float intensity = clamp((theta - light.outer_angle) / epsilon, 0.0, 1.0);
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    return light.color * light.intensity * attenuation * intensity * NdotL;
}

vec3 CalculateAreaLight(Light light, vec3 worldPos, vec3 normal, vec3 viewDir)
{
    // 简化的面光源计算
    vec3 lightDir = normalize(light.position - worldPos);
    float distance = length(light.position - worldPos);
    float area = light.size.x * light.size.y;
    float attenuation = area / (1.0 + distance * distance);
    float NdotL = max(dot(normal, lightDir), 0.0);
    return light.color * light.intensity * attenuation * NdotL;
}

void main() 
{
    // 从GBuffer中获取数据
    vec3 worldPos = subpassLoad(inPosition).xyz;
    vec3 normal = normalize(subpassLoad(inNormal).xyz);
    vec4 albedoRough = subpassLoad(inAlbedo);
    vec3 albedo = albedoRough.rgb;
    float roughness = albedoRough.a;
    vec3 emission = subpassLoad(inEmission).rgb;
    
    vec3 viewDir = normalize(scene.camera_pos - worldPos);
    
    // PBR参数
    vec3 F0 = mix(vec3(0.04), albedo, 0.5); // 金属度固定为0.5
    vec3 Lo = vec3(0.0);
    
    // 计算所有光源的贡献
    for(uint i = 0; i < scene.num_lights; i++)
    {
        if(scene.lights[i].enabled == 0) continue;
        
        vec3 radiance;
        vec3 lightDir;
        
        switch(scene.lights[i].type)
        {
            case LIGHT_DIRECTIONAL:
                radiance = CalculateDirectionalLight(scene.lights[i], worldPos, normal, viewDir);
                lightDir = normalize(-scene.lights[i].direction);
                break;
            case LIGHT_POINT:
                radiance = CalculatePointLight(scene.lights[i], worldPos, normal, viewDir);
                lightDir = normalize(scene.lights[i].position - worldPos);
                break;
            case LIGHT_SPOT:
                radiance = CalculateSpotLight(scene.lights[i], worldPos, normal, viewDir);
                lightDir = normalize(scene.lights[i].position - worldPos);
                break;
            case LIGHT_AREA:
                radiance = CalculateAreaLight(scene.lights[i], worldPos, normal, viewDir);
                lightDir = normalize(scene.lights[i].position - worldPos);
                break;
        }
        
        vec3 H = normalize(viewDir + lightDir);
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, viewDir, lightDir, roughness);
        vec3 F = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        
        float NdotL = max(dot(normal, lightDir), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // 环境光和自发光
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo + emission;
    
    // HDR色调映射和gamma校正
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    
    outColor = vec4(color, 1.0);
}