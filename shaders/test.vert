#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

#include "mesh_structures.glsl"

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec3 out_color;
layout (location = 2) out vec2 out_uv;

void main() {
    // 获取顶点数据
    Vertex v = vertex_buffer.vertices[gl_VertexIndex];
    
    // 从可见实例缓冲区获取实际的对象ID
    uint object_index = visible_draw_buffer.visible_commands[gl_DrawID].object_id;
    
    // 获取对象的模型矩阵
    mat4 model_matrix = object_buffer.objects[object_index].model;
    
    // 计算最终位置
    vec4 world_pos = model_matrix * vec4(v.position, 1.0);
    gl_Position = scene_data.viewproj * world_pos;
    
    // 计算法线
    out_normal = normalize((model_matrix * vec4(v.normal, 0.0)).xyz);
    
    // 传递颜色和UV
    out_color = v.color.xyz ;
    out_uv = vec2(v.uv_x, v.uv_y);
}