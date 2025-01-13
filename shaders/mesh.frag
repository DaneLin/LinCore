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

vec3 applyNormalMap(in vec3 normal, in vec3 viewVec, in vec2 texcoord, in uint normal_tex_id)
{
    vec3 highResNormal = texture(textures[normal_tex_id], texcoord).rgb;
    highResNormal = normalize(highResNormal * 2.0 - 1.0);
    mat3 TBN = cotangentFrame(normal, -viewVec, texcoord);
    return normalize(TBN * highResNormal);
}

void main() 
{
    MaterialData material = material_data_buffer.materials[object_buffer.objects[in_object_id].material_index];
    vec3 base_color = rgb2lin(texture(textures[material.base_color_tex_id], in_uv).rgb * material.base_color_factor.rgb);
    float roughness = texture(textures[material.metallic_roughness_tex_id], in_uv).g * material.roughness_factor;
    float metallic = texture(textures[material.metallic_roughness_tex_id], in_uv).b * material.metallic_factor;
    vec4 emission = vec4(0.0);
    float reflectance = material.reflectance_factor;
    
    vec3 light_dir = normalize(-scene_data.sunlight_direction.xyz);
    vec3 view_dir = normalize(scene_data.camera_position - world_position);
    vec3 n = normalize(in_normal);
    
    // 应用法线贴图
    n = applyNormalMap(n, view_dir, in_uv, material.normal_tex_id);

    vec3 radiance = rgb2lin(emission.rgb);
    
    // 添加环境光
    // float ambient_intensity = 0.03;
    // vec3 ambient = base_color * ambient_intensity;
    // if (metallic > 0.5) {
    //     ambient *= metallic; // 金属的环境反射更强
    // }
    // radiance += ambient;

    float light_intensity = scene_data.sunlight_direction.w;
    float irradiance = max(dot(light_dir, n), 0.0) * light_intensity;
    if (irradiance > 0.0)
    {
        vec3 brdf = brdfMicrofacet(light_dir, view_dir, n, metallic, roughness, base_color, reflectance);
        radiance += brdf * irradiance * scene_data.sunlight_color.rgb;
    }

    out_color = vec4(lin2rgb(radiance), 1.0);
}