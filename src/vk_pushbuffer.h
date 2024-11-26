#pragma once

#include <vk_types.h>

namespace vkutils
{
	struct PushBuffer
	{
		template <typename T>
		uint32_t Push(T &data);

		uint32_t Push(void *data, size_t size);

		void Init(VmaAllocator &allocator, AllocatedBuffer sourceBuffer, uint32_t alignment);
		void Reset();

		uint32_t PadUniformBufferSize(uint32_t originalSize);

		AllocatedBuffer source;

		uint32_t align;
		uint32_t current_offset;
		void *mapped;
	};

	template <typename T>
	uint32_t vkutils::PushBuffer::Push(T &data)
	{
		return push(&data, sizeof(T));
	}
}
