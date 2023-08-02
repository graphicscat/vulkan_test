// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_engine.h>

namespace vkutil {
	bool load_image_from_file(VulkanEngine& engine, const char* file, AllocatedImage& outImage);
    void transitionImaglayout(VulkanEngine &engine,VkImage image,VkFormat format,VkImageLayout oldLayout,VkImageLayout newLayout);
    void copyBuffertoImage(VulkanEngine& engine,VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
}