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

layout (set = 0, binding = 1) uniform sampler2D g_position;
layout (set = 0, binding = 2) uniform sampler2D g_normal;
layout (set = 0, binding = 3) uniform sampler2D g_albedo_spec;
layout (set = 0, binding = 4) uniform sampler2D g_arm;
layout (set = 0, binding = 5) uniform sampler2D g_emission;

#include "math.glsl"
#include "brdf.glsl"

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 out_color;

void main()
{
    vec3 position = texture(g_position, in_uv).rgb;
    vec3 normal = texture(g_normal, in_uv).rgb;
    vec4 albedo_spec = texture(g_albedo_spec, in_uv);
    vec3 arm = texture(g_arm, in_uv).rgb;
    vec3 emission = texture(g_emission, in_uv).rgb;

    float roughness = arm.g;
    float metallic = arm.b;
    vec3 base_color = albedo_spec.rgb;
    float reflectance = albedo_spec.a;

    vec3 light_dir = normalize(-scene_data.sunlight_direction.xyz);
    vec3 view_dir = normalize(scene_data.camera_position - position);

    float light_intensity = scene_data.sunlight_direction.w;
    float irradiance = max(dot(light_dir, normal), 0.0) * light_intensity;

    vec3 radiance = rgb2lin(emission.rgb);
    if (irradiance > 0.0)
    {
        vec3 brdf = brdfMicrofacet(light_dir, view_dir, normal, metallic, roughness, base_color, reflectance);
        radiance += brdf * irradiance * scene_data.sunlight_color.rgb;
    }

    out_color = vec4(radiance, 1.0);
}
