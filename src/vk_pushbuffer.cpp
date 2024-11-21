#include <vk_pushbuffer.h>

uint32_t vkutils::PushBuffer::Push(void* data, size_t size)
{
	uint32_t offset = current_offset;
	char* target = (char*)mapped;
	target += current_offset;
	memcpy(target, data, size);
	current_offset += static_cast<uint32_t>(size);
	current_offset = PadUniformBufferSize(current_offset);

	return offset;
}

void vkutils::PushBuffer::Init(VmaAllocator& allocator, AllocatedBufferUntyped sourceBuffer, uint32_t alignment)
{
	align = alignment;
	source = sourceBuffer;
	current_offset = 0;
	vmaMapMemory(allocator, sourceBuffer.allocation, &mapped);
}

void vkutils::PushBuffer::Reset()
{
	current_offset = 0;
}

uint32_t vkutils::PushBuffer::PadUniformBufferSize(uint32_t originalSize)
{
	// Calculate required aligment based on minimum device offset alignment
	size_t minUboAlignment = align;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0)
	{
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return static_cast<uint32_t>(alignedSize);
}