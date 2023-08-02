#pragma once

#include <vulkan/vulkan.h>
#include <iostream>
#include <array>
#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <string>
#include <string.h>

struct AllocatedImage {
	VkImage _image;
	VkImageView _view;
    VkSampler _sampler;
    VkDeviceMemory _mem;
    VkDescriptorImageInfo descriptor{};
};