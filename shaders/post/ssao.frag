#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../core/global.glsl"
#include "../core/math.glsl"

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 frag_color;

layout(set = 0, binding = 1) uniform sampler2D g_depth; // we will use the depth buffer to get position

const int MAX_KERNEL_SIZE = 128;
layout(set = 0, binding = 2) buffer g_kernels {
    vec3 g_kernels[MAX_KERNEL_SIZE];
} g_kernel_buffer;

layout (push_constant) uniform PushConstants {
    float radius;
    float power;
    float bias;
} push_constants;

void main()
{
    float depth = texture(g_depth, in_uv).r;
    vec3 position = ReconstructWorldPosition(depth, in_uv);

    float AO = 0.0;

    for (int i = 0 ; i < MAX_KERNEL_SIZE; i++)
    {
        // generate a random point
        vec3 sample_pos = position + g_kernel_buffer.g_kernels[i]; 
        vec4 offset = vec4(sample_pos, 1.0);

        offset = scene_data.viewproj * offset;
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + vec2(0.5);

        float sample_depth = texture(g_depth, offset.xy).r;
        
        if (abs(position.z - sample_depth) < push_constants.radius)
        {
            AO += step(sample_depth, sample_pos.z);
        }
    }

    AO = 1.0 - AO / MAX_KERNEL_SIZE;
    frag_color = vec4(pow(AO, push_constants.power));
}