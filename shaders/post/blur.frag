#version 450

layout (location = 0) in vec2 in_uv;

layout (location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform sampler2D ao_texture;  

float offsets[4] = float[](-1.5, -0.5, 0.5, 1.5);

void main()
{
    vec3 color = vec3(0.0);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            vec2 tc = in_uv;
            tc.x = in_uv.x + offsets[j] / textureSize(ao_texture, 0).x;
            tc.y = in_uv.y + offsets[i] / textureSize(ao_texture, 0).y;
            color += texture(ao_texture, tc).rgb;
        }
    }

    color /= 16.0;

    frag_color = vec4(color, 1.0);
}