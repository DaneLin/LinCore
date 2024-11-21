
#pragma once 

namespace vkutils {

	void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

	void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
};