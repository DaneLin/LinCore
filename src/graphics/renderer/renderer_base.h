#pragma once

#include "graphics/vk_types.h"
#include "graphics/vk_resources.h"

namespace lincore
{
    // forward declaration
    class GpuDevice;
    class CommandBuffer;

    // base render info 
    struct RenderBaseInfo
    {
        VkDescriptorSet global_set{VK_NULL_HANDLE};
        VkDescriptorSet bindless_set{VK_NULL_HANDLE};
        VkExtent2D draw_extent{};
    };

    // base class for renderer
    class IRenderer {
    public:
        virtual ~IRenderer() = default;
        
        virtual void Init(GpuDevice* gpu_device) = 0;
        virtual void Shutdown() = 0;
        virtual void Draw(CommandBuffer* cmd, const RenderBaseInfo* render_info)
        {
            PreRenderBarriers(cmd);

            DrawInternal(cmd, render_info);

            PostRenderBarriers(cmd);
        }

        // 渲染前的 barrier
        virtual void PreRenderBarriers(CommandBuffer* cmd) {}
        
        // 渲染后的 barrier
        virtual void PostRenderBarriers(CommandBuffer* cmd) {}
        
        // 实际渲染实现
        virtual void DrawInternal(CommandBuffer* cmd, const RenderBaseInfo* render_info) = 0;

        GpuDevice* gpu_device_{nullptr};
        DeletionQueue deletion_queue_{};
    };

}