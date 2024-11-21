
#pragma once 

namespace vkutils {

	void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout current_layout, VkImageLayout new_layout);

	void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D src_size, VkExtent2D dst_size);

	void GenerateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D image_size);
};