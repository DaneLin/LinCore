#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../core/global.glsl"
#include "../core/math.glsl"

layout (location = 0) in vec2 in_uv;

layout (location = 0) out float frag_color;

layout(set = 0, binding = 1) uniform sampler2D g_depth; // we will use the depth buffer to get position
layout(set = 0, binding = 2) uniform sampler2D g_normal;
layout(set = 0, binding = 3) uniform sampler2D ssao_noise;

layout(constant_id = 0) const int MAX_KERNEL_SIZE = 32;
layout(set = 0, binding = 4) readonly buffer ssao_kernels {
    vec4 ssao_kernels[MAX_KERNEL_SIZE];
} ssao_kernel_buffer;

layout (push_constant) uniform PushConstants {
    float radius;
    float power;
    float bias;
} push_constants;

void main()
{
    float depth = texture(g_depth, in_uv).r;
    vec3 position = ReconstructWorldPosition(depth, in_uv);
    vec3 normal = normalize(texture(g_normal, in_uv).rgb);

    // Get a random vector using a noise lookup
    ivec2 texDim = textureSize(g_depth, 0); 
    ivec2 noiseDim = textureSize(ssao_noise, 0);
    vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * in_uv;  
    vec3 randomVec = texture(ssao_noise, noiseUV).xyz * 2.0 - 1.0;
    
    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    
    for(int i = 0; i < MAX_KERNEL_SIZE; i++)
    {
        // Get sample position in TBN space
        vec3 samplePos = TBN * ssao_kernel_buffer.ssao_kernels[i].xyz;
        samplePos = position + samplePos * push_constants.radius;
        
        // Project sample position
        vec4 offset = scene_data.viewproj * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        
        float sampleDepth = texture(g_depth, offset.xy).r;
        vec3 sampleWorldPos = ReconstructWorldPosition(sampleDepth, offset.xy);
        
        // Range check using smoothstep
        float rangeCheck = smoothstep(0.0, 1.0, push_constants.radius / abs(position.z - sampleWorldPos.z));
        occlusion += (sampleWorldPos.z >= samplePos.z + push_constants.bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / float(MAX_KERNEL_SIZE));
    frag_color = occlusion;
}