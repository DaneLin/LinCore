#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location =0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;

struct Vertex{
	vec3 position;
	float uv_x;
	vec3 normal;
	float uv_y;
	vec4 color;
};

layout(std140, set = 0, binding = 1) readonly buffer GlobalVertexBuffer{
	Vertex vertices[];
};

// push constants block
layout (push_constant) uniform constants {
    mat4 render_matrix;
}PushConstants;

void main()
{
    // ��ȫ�ֻ�������ȡ��������
    Vertex v = vertices[gl_VertexIndex];

    // ���㶥��λ��
    vec4 position = vec4(v.position, 1.0);
    gl_Position = sceneData.viewproj * PushConstants.render_matrix * position;

    // ���㷨��
    outNormal = (PushConstants.render_matrix * vec4(v.normal, 0.0)).xyz;

    // ������ɫ����ʻ��
    outColor = v.color.xyz * materialData.colorFactors.xyz;

    // UV ����
    outUV.x = v.uv_x;
    outUV.y = v.uv_y;
}
