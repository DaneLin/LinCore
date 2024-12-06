#pragma once

namespace ResourceUpdateType {

    enum Enum {
        Buffer, Texture, Pipeline, Sampler, DescriptorSetLayout, DescriptorSet, RenderPass, Framebuffer, ShaderState, TextureView, PagePool, Count
    };

    static const char* s_value_names[] = {
        "Buffer", "Texture", "Pipeline", "Sampler", "DescriptorSetLayout", "DescriptorSet", "RenderPass", "Framebuffer", "ShaderState", "TextureView", "PagePool"
    };

    static const char* ToString( Enum e ) {
        return ( ( uint32_t )e < Enum::Count ? s_value_names[ ( int )e ] : "unsupported" );
    }
} // namespace ResourceUpdateType

namespace TextureType {
    enum Enum {
        Texture1D, Texture2D, Texture3D, Texture_1D_Array, Texture_2D_Array, Texture_Cube_Array, Count
    };

    enum Mask {
        Texture1D_mask = 1 << 0, Texture2D_mask = 1 << 1, Texture3D_mask = 1 << 2, Texture_1D_Array_mask = 1 << 3, Texture_2D_Array_mask = 1 << 4, Texture_Cube_Array_mask = 1 << 5, Count_mask = 1 << 6
    };

    static const char* s_value_names[] = {
        "Texture1D", "Texture2D", "Texture3D", "Texture_1D_Array", "Texture_2D_Array", "Texture_Cube_Array", "Count"
    };

    static const char* ToString( Enum e ) {
        return ((uint32_t)e < Enum::Count ? s_value_names[(int)e] : "unsupported" );
    }
} // namespace TextureType

typedef enum ResourceState {
    RESOURCE_STATE_UNDEFINED = 0,
    RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    RESOURCE_STATE_INDEX_BUFFER = 0x2,
    RESOURCE_STATE_RENDER_TARGET = 0x4,
    RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
    RESOURCE_STATE_DEPTH_WRITE = 0x10,
    RESOURCE_STATE_DEPTH_READ = 0x20,
    RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
    RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
    RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
    RESOURCE_STATE_STREAM_OUT = 0x100,
    RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
    RESOURCE_STATE_COPY_DEST = 0x400,
    RESOURCE_STATE_COPY_SOURCE = 0x800,
    RESOURCE_STATE_GENERIC_READ = ( ( ( ( ( 0x1 | 0x2 ) | 0x40 ) | 0x80 ) | 0x200 ) | 0x800 ),
    RESOURCE_STATE_PRESENT = 0x1000,
    RESOURCE_STATE_COMMON = 0x2000,
    RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x4000,
    RESOURCE_STATE_SHADING_RATE_SOURCE = 0x8000,
} ResourceState; // ResourceState