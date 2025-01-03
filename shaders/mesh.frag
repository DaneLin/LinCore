#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "mesh_structures.glsl"
#include "math.glsl"
#include "brdf.glsl"

// 从顶点着色器接收的输入
layout (location = 0) in vec3 in_normal;
layout (location = 1) in vec2 in_uv;
layout (location = 2) flat in uint in_object_id;
layout (location = 3) in vec3 world_position;
// 输出
layout (location = 0) out vec4 out_color;


void main() 
{
    MaterialData material = material_data_buffer.materials[object_buffer.objects[in_object_id].material_index];
    vec3 base_color = rgb2lin(texture(textures[material.base_color_tex_id], in_uv).rgb);
    float roughness = texture(textures[material.metallic_roughness_tex_id], in_uv).r;
    float metallic = texture(textures[material.metallic_roughness_tex_id], in_uv).g;
    float reflectance = 0.5;
    vec3 light_dir = normalize(-scene_data.sunlight_direction.xyz);
    vec3 view_dir = normalize(scene_data.camera_position - world_position);
    vec3 n = normalize(in_normal);
    vec3 radiance = vec3(0.0); // no emission

    float irradiance = max(dot(light_dir, n), 0.0) * 10.0f; // 10.0f for irradiance perp
    if (irradiance > 0.0)
    {
        vec3 brdf = brdfMicrofacet(light_dir, view_dir, n, metallic, roughness, base_color, reflectance);
        radiance += brdf * irradiance * scene_data.sunlight_color.rbg;
    }

    out_color = vec4(lin2rgb(radiance), 1.0);
}