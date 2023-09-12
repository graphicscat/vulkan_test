#include <vk_cubemap.h>
#include <iostream>

#include <vk_initializers.h>


#include <stb_image.h>

bool vkcubemap::load_image_from_file(VulkanEngine& engine, std::vector<const char*> files, AllocatedImage& outImage)
{
    stbi_uc* pixels[6];
    int texWidth, texHeight, texChannels;
    if(files.size() != 6)
    {
        std::cout<<"Error image count is not 6"<<std::endl;
    }

    for(int i = 0 ; i < 6 ; i++)
    {
        pixels[i] = stbi_load(files[i],&texWidth,&texHeight,&texChannels,STBI_rgb_alpha);
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

	VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

    struct StagingBuffer
    {
        /* data */
        VkBuffer buffer;
        VkDeviceMemory memory;
    }stagingBuffer{};

    engine.createBuffer(imageSize*6,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    stagingBuffer.buffer,
    stagingBuffer.memory);

    for(int i = 0 ; i < 6 ; i++)
    {
        void* mapped = nullptr;
        vkMapMemory(engine._device,stagingBuffer.memory,imageSize*i,imageSize,0,&mapped);
        memcpy(mapped,pixels[i],imageSize);
        vkUnmapMemory(engine._device,stagingBuffer.memory);

    }

    for(int i = 0 ; i < 6 ; i++)
    {
        stbi_image_free(pixels[i]);
    }

    VkExtent3D imageExtent;
	imageExtent.width = static_cast<uint32_t>(texWidth);
	imageExtent.height = static_cast<uint32_t>(texHeight);
	imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);
    dimg_info.arrayLayers = 6;
    dimg_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    AllocatedImage newImage;

    engine.createImage(dimg_info,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,newImage._image,newImage._mem);

    transitionImaglayout(engine,newImage._image,image_format,VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    copyBuffertoImage(engine,stagingBuffer.buffer,newImage._image,imageExtent.width,imageExtent.height);
    transitionImaglayout(engine,newImage._image,image_format,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(engine._device,stagingBuffer.buffer,nullptr);
    vkFreeMemory(engine._device,stagingBuffer.memory,nullptr);

    VkImageViewCreateInfo imageViewInfo{};
    VkImageSubresourceRange range{};

    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseArrayLayer = 0;
    range.baseMipLevel = 0;
    range.layerCount = 6;
    range.levelCount = 1;
    
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = newImage._image;
    imageViewInfo.subresourceRange = range;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imageViewInfo.format = image_format;
    newImage._view = engine.createImageView(newImage._image,imageViewInfo);

    newImage._sampler = engine.createSampler();

    newImage.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    newImage.descriptor.imageView = newImage._view;
    newImage.descriptor.sampler = newImage._sampler;

    outImage = newImage;

    engine._mainDeletionQueue.push_function([=](){
        vkDestroySampler(engine._device,outImage._sampler,nullptr);
        vkDestroyImageView(engine._device,outImage._view,nullptr);
        vkDestroyImage(engine._device,outImage._image,nullptr);
		vkFreeMemory(engine._device,outImage._mem,nullptr);
    });

    return true;
}

void vkcubemap::transitionImaglayout(VulkanEngine &engine,VkImage image,VkFormat format,VkImageLayout oldLayout,VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = engine.beginSingleCommand();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 6;
	barrier.srcAccessMask = 0; // TODO
	barrier.dstAccessMask = 0;

	VkPipelineStageFlags sourceStage{};
	VkPipelineStageFlags destinationStage{};

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		//throw std::invalid_argument("unsupported layout transition!");
		std::cout << "error" << std::endl;
	}

	
	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	engine.endSingleCommand(commandBuffer);
}

void vkcubemap::copyBuffertoImage(VulkanEngine& engine,VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
 {

    VkCommandBuffer commandBuffer = engine.beginSingleCommand();

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 6;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	engine.endSingleCommand(commandBuffer);

 }