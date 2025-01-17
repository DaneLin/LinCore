#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../core/mesh_structures.glsl"
#include "../core/math.glsl"
#include "../core/brdf.glsl"

// 从顶点着色器接收的输入
layout (location = 0) in vec3 in_normal;
layout (location = 1) in vec2 in_uv;
layout (location = 2) flat in uint in_object_id;
layout (location = 3) in vec3 world_position;

// 输出 - 优化后的GBuffer布局
layout (location = 0) out vec4 g_normal_rough;     // xy: 压缩法线, z: 粗糙度, w: 金属度
layout (location = 1) out vec4 g_albedo_spec;      // rgb: 基础颜色, a: 反射率
layout (location = 2) out vec4 g_emission;         // rgb: 自发光, a: 未使用

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
    // 直接使用纹理颜色，不做颜色空间转换
    vec3 base_color = texture(textures[material.base_color_tex_id], in_uv).rgb * material.base_color_factor.rgb;
    float roughness = texture(textures[material.metallic_roughness_tex_id], in_uv).g * material.roughness_factor;
    float metallic = texture(textures[material.metallic_roughness_tex_id], in_uv).b * material.metallic_factor;
    vec4 emission = texture(textures[material.emissive_tex_id], in_uv);
    float reflectance = material.reflectance_factor;
    vec3 n = normalize(in_normal);
    // 应用法线贴图
    vec3 view_dir = normalize(scene_data.camera_position - world_position);
    n = applyNormalMap(n, view_dir, in_uv, material.normal_tex_id);

    // 压缩法线并输出到优化后的GBuffer
    vec2 encoded_normal = EncodeNormal(n);
    g_normal_rough = vec4(encoded_normal, roughness, metallic);
    g_albedo_spec = vec4(base_color, reflectance);
    g_emission = vec4(emission.rgb, 0.0);
}