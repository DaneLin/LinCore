// input_structure.glsl
layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;  
    mat4 viewproj;
    vec4 ambientColor;
    vec4 sunlightDirection;
    vec4 sunlightColor;
} sceneData;


layout(set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 colorFactors;
    vec4 metal_rough_factors;
    uint colorTexID;      // 改为textureID 
    uint metalRoughTexID; // 改为textureID
    uint padding[2];
} materialData;

// 全局bindless texture数组
layout(set = 2, binding = 11) uniform sampler2D textures[];

// 通过索引访问texture
#define getColorTexture() textures[materialData.colorTexID]
#define getMetalRoughTexture() textures[materialData.metalRoughTexID]