#version 450

layout(location = 0) in vec3 inUVW;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform samplerCube skybox;

void main() {
    outColor = texture(skybox, inUVW);
    //outColor = vec4(1.0, 0.0, 0.0, 1.0);
}