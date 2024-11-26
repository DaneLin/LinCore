#pragma once

#include <vk_types.h>
#include <vector>
#include <array>
#include <unordered_map>

namespace vkutils
{
    class VulkanProfiler;

    struct ScopeTimer
    {
        uint32_t start_time_stamp;
        uint32_t end_time_stamp;
        std::string name;
    };

    struct StatRecorder
    {
        uint32_t query;

        std::string name;
    };

    class VulkanScopeTimer
    {
    public:
        VulkanScopeTimer(VkCommandBuffer commands, VulkanProfiler *profiler, const char *name);
        ~VulkanScopeTimer();

    private:
        VulkanProfiler *profiler_;
        VkCommandBuffer commands_;
        ScopeTimer timer_;
    };

    class VulkanPipelineStatRecorder
    {
    public:
        VulkanPipelineStatRecorder(VkCommandBuffer commands, VulkanProfiler *profiler, const char *name);
        ~VulkanPipelineStatRecorder();

    private:
        VulkanProfiler *profiler_;
        VkCommandBuffer commands_;
        StatRecorder recorder_;
    };

    class VulkanProfiler
    {
    public:
        void Init(VkDevice device, float time_stamp_period, int per_frame_pool_sizes = 100);

        void GrabQueries(VkCommandBuffer commands);

        void CleanUp();

        double GetStat(const std::string &name);

        VkQueryPool GetTimerPool();
        VkQueryPool GetStatPool();

        void AddTimer(ScopeTimer &timer);

        void AddStat(StatRecorder &timer);

        uint32_t GetTimeStampId();
        uint32_t GetStatId();

        void DebugPrintQueryUsage();

        std::unordered_map<std::string, double> timing_;
        std::unordered_map<std::string, int32_t> stats_;

    private:
        struct QueryFrameState
        {
            std::vector<ScopeTimer> frame_timers;
            VkQueryPool timer_pool;
            uint32_t timer_last;

            std::vector<StatRecorder> stat_recorders;
            VkQueryPool stat_pool;
            uint32_t stat_last;
            bool needs_reset;
        };

        static constexpr int kQUERY_FRAME_OVERLAP = 3;

        int current_frame_;
        float period_;
        int per_frame_pool_sizes_;
        std::array<QueryFrameState, kQUERY_FRAME_OVERLAP> query_frames_;

        VkDevice device_;
    };
}