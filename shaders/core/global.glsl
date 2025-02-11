#ifndef GLOBAL_GLSL
#define GLOBAL_GLSL

layout(std140, set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;  
    mat4 viewproj;
    vec4 sunlight_direction;
    vec4 sunlight_color;
    vec3 camera_position;
    float padding;
} scene_data;

vec3 ReconstructWorldPosition(float depth, vec2 uv) {
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePosition = inverse(scene_data.proj) * clipSpacePosition;
    viewSpacePosition /= viewSpacePosition.w;
    vec4 worldSpacePosition = inverse(scene_data.view) * viewSpacePosition;
    return worldSpacePosition.xyz;
}

#endif // GLOBAL_GLSL