#include "vk_profiler.h"

namespace lincore
{


	VulkanScopeTimer::VulkanScopeTimer(VkCommandBuffer commands, VulkanProfiler* profiler, const char* name)
		: profiler_(profiler), commands_(commands)
	{
		timer_.name = name;
		timer_.start_time_stamp = profiler_->GetTimeStampId();

		VkQueryPool pool = profiler_->GetTimerPool();

		vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, timer_.start_time_stamp);
	}

	VulkanScopeTimer::~VulkanScopeTimer()
	{
		timer_.end_time_stamp = profiler_->GetTimeStampId();
		VkQueryPool pool = profiler_->GetTimerPool();
		vkCmdWriteTimestamp(commands_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, timer_.end_time_stamp);

		profiler_->AddTimer(timer_);
	}

	VulkanPipelineStatRecorder::VulkanPipelineStatRecorder(VkCommandBuffer commands, VulkanProfiler* profiler, const char* name)
		: profiler_(profiler), commands_(commands)
	{
		recorder_.name = name;
		recorder_.query = profiler_->GetStatId();

		VkQueryPool pool = profiler_->GetStatPool();

		vkCmdBeginQuery(commands, pool, recorder_.query, 0);
	}

	VulkanPipelineStatRecorder::~VulkanPipelineStatRecorder()
	{
		VkQueryPool pool = profiler_->GetStatPool();
		vkCmdEndQuery(commands_, pool, recorder_.query);

		profiler_->AddStat(recorder_);
	}

	void VulkanProfiler::Init(VkDevice device, float time_stamp_period, int per_frame_pool_sizes)
	{
		device_ = device;
		period_ = time_stamp_period;
		current_frame_ = 0;
		per_frame_pool_sizes_ = per_frame_pool_sizes;

		VkQueryPoolCreateInfo query_pool_info = {};
		query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		query_pool_info.queryCount = per_frame_pool_sizes_;

		for (int i = 0; i < kQUERY_FRAME_OVERLAP; ++i)
		{
			vkCreateQueryPool(device, &query_pool_info, nullptr, &query_frames_[i].timer_pool);
			query_frames_[i].timer_last = 0;
			query_frames_[i].needs_reset = true;
			vkResetQueryPoolEXT(device, query_frames_[i].timer_pool, 0, per_frame_pool_sizes_);
		}
		query_pool_info.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
		for (int i = 0; i < kQUERY_FRAME_OVERLAP; ++i)
		{
			query_pool_info.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
			vkCreateQueryPool(device, &query_pool_info, nullptr, &query_frames_[i].stat_pool);
			query_frames_[i].stat_last = 0;
			vkResetQueryPoolEXT(device, query_frames_[i].stat_pool, 0, per_frame_pool_sizes_);
		}
	}

	void VulkanProfiler::GrabQueries(VkCommandBuffer commands)
	{
		int frame = current_frame_;
		current_frame_ = (current_frame_ + 1) % kQUERY_FRAME_OVERLAP;

		// 确保当前帧的查询池被重置
		if (query_frames_[current_frame_].needs_reset)
		{
			if (query_frames_[current_frame_].timer_last > 0)
			{
				vkCmdResetQueryPool(commands, query_frames_[current_frame_].timer_pool, 0, query_frames_[current_frame_].timer_last);
			}
			if (query_frames_[current_frame_].stat_last > 0)
			{
				vkCmdResetQueryPool(commands, query_frames_[current_frame_].stat_pool, 0, query_frames_[current_frame_].stat_last);
			}
			query_frames_[current_frame_].needs_reset = false;
		}

		query_frames_[current_frame_].timer_last = 0;
		query_frames_[current_frame_].frame_timers.clear();
		query_frames_[current_frame_].stat_last = 0;
		query_frames_[current_frame_].stat_recorders.clear();

		QueryFrameState& state = query_frames_[frame];
		std::vector<uint64_t> query_state;
		query_state.resize(state.timer_last);
		if (state.timer_last != 0)
		{
			// We use vkGetQueryPoolResults to copy the results into a host visible buffer
			vkGetQueryPoolResults(
				device_,
				state.timer_pool,
				0,
				state.timer_last,
				query_state.size() * sizeof(uint64_t),
				query_state.data(),
				sizeof(uint64_t),
				VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
			// 标记这个帧需要在下次使用前重置
			state.needs_reset = true;
		}
		std::vector<uint64_t> stat_results;
		stat_results.resize(state.stat_last);
		if (state.stat_last != 0)
		{
			vkGetQueryPoolResults(
				device_,
				state.stat_pool,
				0,
				state.stat_last,
				stat_results.size() * sizeof(uint64_t),
				stat_results.data(),
				sizeof(uint64_t),
				VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
		}

		for (auto& timer : state.frame_timers)
		{
			uint64_t begin = query_state[timer.start_time_stamp];
			uint64_t end = query_state[timer.end_time_stamp];

			uint64_t time_stamp = end - begin;
			timing_[timer.name] = (float(time_stamp) * period_) * 1e-6;
		}

		for (auto& st : state.stat_recorders)
		{
			uint64_t result = stat_results[st.query];

			stats_[st.name] = static_cast<int32_t>(result);
		}
	}

	void VulkanProfiler::CleanUp()
	{
		for (int i = 0; i < kQUERY_FRAME_OVERLAP; ++i)
		{
			vkDestroyQueryPool(device_, query_frames_[i].timer_pool, nullptr);
			vkDestroyQueryPool(device_, query_frames_[i].stat_pool, nullptr);
		}
	}

	double VulkanProfiler::GetStat(const std::string& name)
	{
		auto it = timing_.find(name);
		if (it != timing_.end())
		{
			return (*it).second;
		}
		else
		{
			return 0;
		}
	}

	VkQueryPool VulkanProfiler::GetTimerPool()
	{
		return query_frames_[current_frame_].timer_pool;
	}

	VkQueryPool VulkanProfiler::GetStatPool()
	{
		return query_frames_[current_frame_].stat_pool;
	}

	void VulkanProfiler::AddTimer(ScopeTimer& timer)
	{
		query_frames_[current_frame_].frame_timers.push_back(timer);
	}

	void VulkanProfiler::AddStat(StatRecorder& timer)
	{
		query_frames_[current_frame_].stat_recorders.push_back(timer);
	}

	uint32_t VulkanProfiler::GetTimeStampId()
	{
		uint32_t q = query_frames_[current_frame_].timer_last;
		query_frames_[current_frame_].timer_last++;
		return q;
	}

	uint32_t VulkanProfiler::GetStatId()
	{
		uint32_t q = query_frames_[current_frame_].stat_last;
		query_frames_[current_frame_].stat_last++;
		return q;
	}

	void VulkanProfiler::DebugPrintQueryUsage()
	{

		for (int i = 0; i < kQUERY_FRAME_OVERLAP; i++)
		{
			printf("Frame %d:\n", i);
			printf("  Timer queries used: %d\n", query_frames_[i].timer_last);
			printf("  Timer records: %zu\n", query_frames_[i].frame_timers.size());
			printf("  Stat queries used: %d\n", query_frames_[i].stat_last);
			printf("  Stat records: %zu\n", query_frames_[i].stat_recorders.size());
		}
	}

}