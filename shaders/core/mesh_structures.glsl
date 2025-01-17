// mesh_structures.glsl
layout(std140, set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;  
    mat4 viewproj;
    vec4 sunlight_direction;
    vec4 sunlight_color;
    vec3 camera_position;
    float padding;
} scene_data;

struct Vertex {
    vec3 position;      // offset 0  (12 bytes, padded to 16)
    float uv_x;
    vec3 normal;        // offset 16 (12 bytes, padded to 16)
    float uv_y;
    vec4 color;         // offset 32 (16 bytes)
};

// 全局顶点缓冲区
layout(std430, set = 0, binding = 1) readonly buffer GlobalVertexBuffer {
    Vertex vertices[];
} vertex_buffer;

// 对象数据缓冲区
struct ObjectData {
    mat4 model;
    vec4 spherebounds;
    vec4 extents;
    uint material_index;
    uint padding[3];
}; 

layout(std430, set = 0, binding = 2) readonly buffer ObjectBuffer {   
    ObjectData objects[];
} object_buffer;

struct DrawCommand {
    uint index_count;
    uint instance_count; 
    uint first_index;
    uint vertex_offset;
    uint first_instance;
    uint object_id;
    uint padding[2];
};

// 可见绘制命令缓冲区
layout(std430,set = 0, binding =3 ) readonly buffer VisibleDrawBuffer {   
    DrawCommand visible_commands[];
} visible_draw_buffer;


struct MaterialData
{
   vec4 base_color_factor;
   vec3 emissive_factor;
   float metallic_factor;
   float roughness_factor;
   float normal_scale;
   float reflectance_factor;
   float padding;
   uint base_color_tex_id;
   uint metallic_roughness_tex_id;
   uint normal_tex_id;
   uint emissive_tex_id; 
};

layout(set = 0, binding = 4) readonly buffer MaterialDataBuffer {
   MaterialData materials[];
} material_data_buffer;

// bindless texture
layout(set = 1, binding = 0) uniform sampler2D textures[];
