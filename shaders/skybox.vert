#version 450

#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec3 outUVW;

// 天空盒顶点位置
const vec3 positions[8] = vec3[8](
    vec3(-1.0, -1.0,  1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3(-1.0,  1.0,  1.0),
    vec3(-1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0)
);

// 索引
const uint indices[36] = uint[36](
    0, 1, 2, 2, 3, 0,  // front
    1, 5, 6, 6, 2, 1,  // right
    5, 4, 7, 7, 6, 5,  // back
    4, 0, 3, 3, 7, 4,  // left
    3, 2, 6, 6, 7, 3,  // top
    4, 5, 1, 1, 0, 4   // bottom
);

#include "core/global.glsl"


void main() {
    uint idx = indices[gl_VertexIndex];
    vec3 pos = positions[idx];
    
    // 移除平移部分，只保留旋转
    mat4 viewRotation = mat4(mat3(scene_data.view));
    vec4 clipPos = scene_data.proj * viewRotation * vec4(pos, 1.0);
    
    // 在 Reverse-Z 中，将深度设置为 0.0（最远）
    gl_Position = vec4(clipPos.x, clipPos.y, 0.0, clipPos.w);
    
    // 将位置作为采样方向
    outUVW = pos;
}