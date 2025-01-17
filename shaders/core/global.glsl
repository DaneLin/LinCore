layout(std140, set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;  
    mat4 viewproj;
    vec4 sunlight_direction;
    vec4 sunlight_color;
    vec3 camera_position;
    float padding;
} scene_data;