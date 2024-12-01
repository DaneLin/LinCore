#pragma once

#include "vk_types.h"
#include "config.h"
#include <TaskScheduler.h>

namespace lc
{
	namespace tools
	{
		struct FileLoadRequest {
			char path[512];
			TextureHandle texture_handle = kInvalidTextureHandle;
			BufferHandle buffer_handle = kInvalidBufferHandle;
		};

		struct UploadRequest {
			void* data = nullptr;
			uint32_t* completed = nullptr;
			TextureHandle texture_handle = kInvalidTextureHandle;
			BufferHandle cpu_buffer_handle = kInvalidBufferHandle;
			BufferHandle gpu_buffer_handle = kInvalidBufferHandle;
		};

		// AsynchronousLoader class has the following responsibilities:
		// - Process load from file requests
		// - Process GPU upload transfers
		// - Manage a staging buffer to handle a copy of the data
		// - Enqueue the command buffers with copy commands
		// - Signal to the renderer that a texture has finished a transfer
		class AsynchronousLoader {
		public:
			enki::TaskScheduler* task_scheduler = nullptr;

			void Init();

			void Update();

			void Shutdown();

			std::vector<FileLoadRequest> file_load_requests;
			std::vector<UploadRequest> upload_requests;

			TextureHandle texture_ready;
			BufferHandle cpu_buffer_ready;
			BufferHandle gpu_buffer_ready;
			uint32_t* completed = nullptr;

			VkCommandPool command_pool[kFRAME_OVERLAP];
			VkCommandBuffer command_buffer[kFRAME_OVERLAP];
			AllocatedBufferUntyped staging_buffer;
			VkSemaphore transfer_complete_semaphore;
			VkFence transfer_fence;
		};

		struct AsynchronousLoadTask : enki::IPinnedTask {
			void Execute() override;

			AsynchronousLoader* async_loader;
			enki::TaskScheduler* task_scheduler;
			bool execute = true;
		};

		struct RunPinnedTaskLoopTask :enki::IPinnedTask {
			void Execute() override;

			enki::TaskScheduler* task_scheduler;
			bool execute = true;
		}; // struct RunPinnedTaskLoopTask
	};
}// namespace lc