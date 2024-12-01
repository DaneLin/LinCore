#include "asynchronous_loader.h"
#include "vk_engine.h"
#include "stb_image.h"
#include "logging.h"

namespace lc::tools
{
	void RunPinnedTaskLoopTask::Execute()
	{
		while (task_scheduler->GetIsRunning() && execute)
		{
			task_scheduler->WaitForNewPinnedTasks();
			// this thread will 'sleep' until there are new pinned tasks
			task_scheduler->RunPinnedTasks();
		}

	}
	void AsynchronousLoadTask::Execute()
	{
		// The idea behind this task is to always be active and wait for requests for resource loading
		// The while loop ensures that the root pinned task never schedules other tasks on this thread
		// locking it to I/O as intended.
		while (execute)
		{
			async_loader->Update();
		}
	}
	void AsynchronousLoader::Init()
	{

		for (uint32_t idx = 0; idx < kFRAME_OVERLAP; ++idx)
		{
			VkCommandPoolCreateInfo cmd_pool_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmd_pool_info.queueFamilyIndex = VulkanEngine::Get().transfer_queue_family_;
			cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			vkCreateCommandPool(VulkanEngine::Get().device_, &cmd_pool_info, nullptr, &command_pool[idx]);

			VkCommandBufferAllocateInfo cmd_buffer_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
			cmd_buffer_info.commandPool = command_pool[idx];
			cmd_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			cmd_buffer_info.commandBufferCount = 1;
			cmd_buffer_info.pNext = nullptr;

			vkAllocateCommandBuffers(VulkanEngine::Get().device_, &cmd_buffer_info, &command_buffer[idx]);
		}

		staging_buffer = VulkanEngine::Get().CreateBuffer(kSTAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		VkSemaphoreCreateInfo semaphore_create_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(VulkanEngine::Get().device_, &semaphore_create_info, nullptr, &transfer_complete_semaphore);

		VkFenceCreateInfo fence_create_info = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence(VulkanEngine::Get().device_, &fence_create_info, nullptr, &transfer_fence);
	}
	void AsynchronousLoader::Update()
	{
		if (cpu_buffer_ready.index != kInvalidIndex && gpu_buffer_ready.index != kInvalidIndex)
		{
			assert(completed != nullptr);
			(*completed)++;

			cpu_buffer_ready = kInvalidBufferHandle;
			gpu_buffer_ready = kInvalidBufferHandle;
			completed = nullptr;
		}

		texture_ready.index = kInvalidTextureHandle.index;

		// Process upload request
		if (upload_requests.size())
		{
			// wait for transfer fence to be finished
			if (vkGetFenceStatus(VulkanEngine::Get().device_, transfer_fence) != VK_SUCCESS)
			{
				return;
			}

			vkResetFences(VulkanEngine::Get().device_, 1, &transfer_fence);

			// Get last request
			UploadRequest request = upload_requests.back();
			upload_requests.pop_back();

			VkCommandBuffer* cmd = &command_buffer[VulkanEngine::Get().frame_number_];
			VkCommandBufferBeginInfo cmd_begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(*cmd, &cmd_begin_info);

			if (request.texture_handle.index != kInvalidTextureHandle.index)
			{
				
			}


			// Process a file request
			if (file_load_requests.size())
			{
				FileLoadRequest load_request = file_load_requests.back();
				file_load_requests.pop_back();

				// Process request
				int x, y, comp;
				uint8_t* texture_data = stbi_load(load_request.path, &x, &y, &comp, STBI_rgb_alpha);
				
				if (texture_data)
				{
					LOGI("Loaded texture: %s", load_request.path);

					UploadRequest& upload_request = upload_requests.emplace_back();
					upload_request.data = texture_data;
					upload_request.texture_handle = load_request.texture_handle;
					upload_request.cpu_buffer_handle = kInvalidBufferHandle;
				}
				else
				{
					LOGE("Failed to load texture: %s", load_request.path);
				}
			}
		}
	}
	void AsynchronousLoader::Shutdown()
	{
		vkDestroySemaphore(VulkanEngine::Get().device_, transfer_complete_semaphore, nullptr);
		vkDestroyFence(VulkanEngine::Get().device_, transfer_fence, nullptr);

		for (uint32_t idx = 0; idx < kFRAME_OVERLAP; ++idx)
		{
			vkDestroyCommandPool(VulkanEngine::Get().device_, command_pool[idx], nullptr);
		}

		VulkanEngine::Get().DestroyBuffer(staging_buffer);
	}
} // namespace lc::tools