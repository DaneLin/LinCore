#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "mesh_structures.glsl"

// 从顶点着色器接收的输入
layout (location = 0) in vec3 in_normal;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_uv;

// 输出
layout (location = 0) out vec4 out_color;


void main() 
{
    // 基础光照计算
    vec3 normal = normalize(in_normal);
    vec3 light_dir = normalize(scene_data.sunlight_direction.xyz);
    
    // 漫反射系数
    float diff = max(dot(normal, light_dir), 0.0);
    
    // 环境光
    vec3 ambient = scene_data.ambient_color.xyz;
    
    // 漫反射光
    vec3 diffuse = scene_data.sunlight_color.xyz * diff * scene_data.sunlight_direction.w;
    
    // 采样纹理
    //vec4 tex_color = texture(textures[material_data.base_color_tex_id], in_uv);
    
    // 合并所有光照分量
    //vec3 result = (ambient + diffuse) * tex_color.rgb * in_color;
    
    out_color = vec4(diffuse, 1.0);
}