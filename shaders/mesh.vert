#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

#include "mesh_structures.glsl"

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec2 out_uv;
layout (location = 2) out uint out_object_id;
layout (location = 3) out vec3 out_vert_pos; // output 3D position in world space

void main() {
    // 获取顶点数据
    Vertex v = vertex_buffer.vertices[gl_VertexIndex];
    
    // 从可见实例缓冲区获取实际的对象ID
    uint object_index = visible_draw_buffer.visible_commands[gl_DrawID].object_id;
    
    // 获取对象的模型矩阵
    mat4 model_matrix = object_buffer.objects[object_index].model;

    
    // 计算最终位置
    vec4 world_pos = model_matrix * vec4(v.position, 1.0);
    out_vert_pos = vec3(world_pos) / world_pos.w;
    gl_Position = scene_data.viewproj * world_pos;
    
    // 计算法线
    mat3 normal_matrix = transpose(inverse(mat3(model_matrix)));
    out_normal = normalize(normal_matrix * v.normal);

    // 获取材质数据
    uint material_index = object_buffer.objects[object_index].material_index;
    MaterialData material = material_data_buffer.materials[material_index];
    out_uv = vec2(v.uv_x, v.uv_y);
    out_object_id = object_index;
}