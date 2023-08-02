#pragma once

#include <vk_types.h>
#include <vk_engine.h>

namespace vkcubemap {
	bool load_image_from_file(VulkanEngine& engine, std::vector<const char*> files, AllocatedImage& outImage);
    void transitionImaglayout(VulkanEngine &engine,VkImage image,VkFormat format,VkImageLayout oldLayout,VkImageLayout newLayout);
    void copyBuffertoImage(VulkanEngine& engine,VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
}