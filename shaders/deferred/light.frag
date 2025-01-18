#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

layout(std140, set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;  
    mat4 viewproj;
    vec4 sunlight_direction;
    vec4 sunlight_color;
    vec3 camera_position;
    float padding;
} scene_data;

layout (set = 0, binding = 1) uniform sampler2D g_normal_rough;
layout (set = 0, binding = 2) uniform sampler2D g_albedo_spec;
layout (set = 0, binding = 3) uniform sampler2D g_emission;
layout (set = 0, binding = 4) uniform sampler2D depth_texture;
layout (set = 0, binding = 5) uniform sampler2D ssao_blur;

#include "../core/math.glsl"
#include "../core/brdf.glsl"

layout (location = 0) in vec2 in_uv;
layout (location = 0) out vec4 out_color;

void main()
{
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

    vec3 light_dir = normalize(-scene_data.sunlight_direction.xyz);
    vec3 view_dir = normalize(scene_data.camera_position - position);

    float light_intensity = scene_data.sunlight_direction.w;
    float irradiance = max(dot(light_dir, normal), 0.0) * light_intensity;

    vec3 ambient = base_color * 0.25 * ao;

    vec3 radiance = rgb2lin(emission) + ambient;
    if (irradiance > 0.0)
    {
        vec3 brdf = brdfMicrofacet(light_dir, view_dir, normal, metallic, roughness, base_color, reflectance);
        radiance += brdf * irradiance * scene_data.sunlight_color.rgb;
    }

    out_color = vec4(radiance, 1.0);
}
