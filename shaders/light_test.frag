
#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "core/math.glsl"
#include "core/global.glsl"
#include "core/light_structures.glsl"
#include "core/lighting.glsl"
#include "core/brdf.glsl"

layout (set = 0, binding = 1) uniform sampler2D g_normal_rough;   // rg: encoded normal, b: roughness, a: metallic
layout (set = 0, binding = 2) uniform sampler2D g_albedo_spec;    // rgb: albedo, a: reflectance
layout (set = 0, binding = 3) uniform sampler2D g_emission;       // rgb: emission
layout (set = 0, binding = 4) uniform sampler2D depth_texture;
layout (set = 0, binding = 5) uniform sampler2D ssao_blur;        // r: ambient occlusion

// 光源数据
layout (std140, set = 1, binding = 0) uniform LightData {
    Light lights[16];
    int light_count;
} light_data;

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

void main() 
{
    // 从G-Buffer采样
    float depth = texture(depth_texture, in_uv).r;
    vec3 position = ReconstructWorldPosition(depth, in_uv);
    
    vec4 normal_rough = texture(g_normal_rough, in_uv);
    vec3 normal = DecodeNormal(normal_rough.xy);
    float roughness = normal_rough.z;
    float metallic = normal_rough.w;
    
    vec4 albedo_spec = texture(g_albedo_spec, in_uv);
    vec3 base_color = albedo_spec.rgb;
    float reflectance = albedo_spec.a;
    
    vec3 emission = texture(g_emission, in_uv).rgb;
    float ao = texture(ssao_blur, in_uv).r;
    
    vec3 view_dir = normalize(scene_data.camera_position - position);
    
    // 计算基础反射率F0
    vec3 F0 = vec3(0.16 * reflectance * reflectance);
    F0 = mix(F0, base_color, metallic);
    
    vec3 radiance = vec3(0.0);
    
    // 其他光源
    for(int i = 0; i < light_data.light_count; i++) {
        radiance += CalculateLightContribution(
            light_data.lights[i],
            position, normal, view_dir, base_color, roughness, F0
        );
    }
    
    // 环境光和自发光
    vec3 ambient = base_color * 0.25 * ao;
    radiance += rgb2lin(emission) + ambient;
    
    out_color = vec4(radiance, 1.0);
}