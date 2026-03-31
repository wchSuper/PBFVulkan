#pragma once
#include <vector>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "extensionfuncs.h"
#include <iostream>
#include <variant>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "utils.hpp"
#include "stb_image.h"
#define Allocator nullptr

namespace Image
{
	inline void CleanupBuffer(VkDevice LDevice, VkBuffer& buffer, VkDeviceMemory& memory, bool mapped)
	{
		if (mapped)
			vkUnmapMemory(LDevice, memory);
		vkDestroyBuffer(LDevice, buffer, Allocator);
		vkFreeMemory(LDevice, memory, Allocator);
	}

	inline uint32_t ChooseMemoryType(VkPhysicalDevice PDevice, uint32_t typefilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memproperties{};
		vkGetPhysicalDeviceMemoryProperties(PDevice, &memproperties);
		for (uint32_t i = 0; i < memproperties.memoryTypeCount; ++i) {
			if (!(typefilter & (1 << i))) continue;
			if ((properties & memproperties.memoryTypes[i].propertyFlags) == properties) {
				return i;
			}
		}
		throw std::runtime_error("failed to choose a suitable memory type!");
	}


	inline void CreateImage(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		VkImage& image, VkDeviceMemory& memory, VkExtent3D extent, VkFormat format,
		VkImageUsageFlags usage, VkSampleCountFlagBits samplecount)
	{
		VkImageCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createinfo.arrayLayers = 1;
		createinfo.extent = extent;
		createinfo.format = format;
		createinfo.imageType = VK_IMAGE_TYPE_2D;
		createinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		createinfo.mipLevels = 1;
		createinfo.samples = samplecount;
		createinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		createinfo.usage = usage;

		if (vkCreateImage(LDevice, &createinfo, Allocator, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}
		VkMemoryRequirements memrequirement{};
		vkGetImageMemoryRequirements(LDevice, image, &memrequirement);
		VkMemoryAllocateInfo allocateinfo{};
		allocateinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateinfo.allocationSize = memrequirement.size;
		allocateinfo.memoryTypeIndex = ChooseMemoryType(PDevice, memrequirement.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(LDevice, &allocateinfo, Allocator, &memory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allcate memory for image!");
		}
		vkBindImageMemory(LDevice, image, memory, 0);
	}

	// cube image for skybox
	inline void CreateSpecialImage(VkPhysicalDevice PDevice, VkDevice LDevice, VkImage& image, VkDeviceMemory& memory, VkExtent3D extent, VkFormat format,
		VkImageUsageFlags usage, VkSampleCountFlagBits samplecount)
	{
		VkImageCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		createinfo.arrayLayers = 1;
		createinfo.extent = extent;
		createinfo.format = format;
		createinfo.imageType = VK_IMAGE_TYPE_2D;
		createinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		createinfo.mipLevels = 1;
		createinfo.arrayLayers = 6;                   // special ! 
		createinfo.samples = samplecount;
		createinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		createinfo.usage = usage;
		createinfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;    // special !

		if (vkCreateImage(LDevice, &createinfo, Allocator, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}
		VkMemoryRequirements memrequirement{};
		vkGetImageMemoryRequirements(LDevice, image, &memrequirement);
		VkMemoryAllocateInfo allocateinfo{};
		allocateinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateinfo.allocationSize = memrequirement.size;
		allocateinfo.memoryTypeIndex = ChooseMemoryType(PDevice, memrequirement.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(LDevice, &allocateinfo, Allocator, &memory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allcate memory for image!");
		}
		vkBindImageMemory(LDevice, image, memory, 0);
	}

	inline VkImageView CreateImageView(VkDevice LDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
	{
		VkImageViewCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createinfo.format = format;
		createinfo.image = image;
		createinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createinfo.subresourceRange.aspectMask = aspectMask;
		createinfo.subresourceRange.baseArrayLayer = 0;
		createinfo.subresourceRange.baseMipLevel = 0;
		createinfo.subresourceRange.layerCount = 1;
		createinfo.subresourceRange.levelCount = 1;
		VkImageView imageview;
		if (vkCreateImageView(LDevice, &createinfo, Allocator, &imageview) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image view!");
		}
		return imageview;
	}

	
	inline void ImageLayoutTransition(
		VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
		VkImage& image, VkImageLayout oldlayout, VkImageLayout newlayout, VkImageAspectFlags asepct)
	{
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.newLayout = newlayout;
		barrier.oldLayout = oldlayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.aspectMask = asepct;
		VkPipelineStageFlags srcstage;
		VkPipelineStageFlags dststage;
		if (oldlayout == VK_IMAGE_LAYOUT_UNDEFINED && newlayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			srcstage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = 0;
			dststage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		else if (oldlayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newlayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			srcstage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dststage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}
		else if (oldlayout == VK_IMAGE_LAYOUT_UNDEFINED && newlayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			srcstage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = 0;
			dststage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		else if (oldlayout == VK_IMAGE_LAYOUT_UNDEFINED && newlayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			srcstage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = 0;
			dststage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		}
		else if (oldlayout == VK_IMAGE_LAYOUT_UNDEFINED && newlayout == VK_IMAGE_LAYOUT_GENERAL) {
			srcstage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = 0;
			dststage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		}
		else {
			throw std::runtime_error("bad oldlayout to newlayout!");
		}
		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		vkCmdPipelineBarrier(cb, srcstage, dststage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		VkSubmitInfo submitinfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);
	}


	// special image layout transition1
	// aspect     : VK_IMAGE_ASPECT_COLOR_BIT
	// old layout : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// new layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	inline void SpecialImageLayoutTransition1(VkCommandBuffer& cb, VkImage& image)
	{
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; 
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		vkCmdPipelineBarrier(
			cb,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // src stage
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,           // dst stage
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	// special image layout transition2
	// aspect : VK_IMAGE_ASPECT_DEPTH_BIT
	// old layout : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	// new layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	inline void SpecialImageLayoutTransition2(VkCommandBuffer& cb, VkImage& image)
	{
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		vkCmdPipelineBarrier(
			cb,
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,      // src stage
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,           // dst stage
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	inline void CreateBuffer(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mempropperties
	);

	inline void CreateTextureResources(const char* image_path, VkImage& textureImage, VkImageView& textureImageView, VkDeviceMemory& textureImageMemory, VkSampler& textureImageSampler,
	const unsigned char* in_pixels = nullptr, VkDeviceSize in_size = 0, int width = 0, int height = 0,
	VkPhysicalDevice PDevice = nullptr, VkDevice LDevice = nullptr, VkCommandPool CommandPool = nullptr, VkQueue GraphicNComputeQueue = nullptr)
	{
		int texWidth, texHeight, texChannels;
		const stbi_uc* pixels;
		VkDeviceSize size;
		bool shouldFreePixels = false;
	
		if (in_pixels == nullptr) {
			pixels = stbi_load(image_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			size = texWidth * texHeight * 4;
			if (!pixels) {
				throw std::runtime_error("Failed to load texture image!");
			}
			shouldFreePixels = true; 
		}
		else {
			pixels = in_pixels;
			size = in_size;
			texWidth = width; texHeight = height;
			shouldFreePixels = false; 
		}
		VkBuffer stagingbuffer;
		VkDeviceMemory stagingbuffermemory;
		CreateBuffer(PDevice, LDevice, stagingbuffer, stagingbuffermemory, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* data;
		vkMapMemory(LDevice, stagingbuffermemory, 0, size, 0, &data);
		memcpy(data, pixels, size);

		VkExtent3D extent = { static_cast<uint32_t>(texWidth),static_cast<uint32_t>(texHeight),1 };
		CreateImage(PDevice, LDevice, textureImage, textureImageMemory, extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT);

		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkImageMemoryBarrier imagebarrier{};
		imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imagebarrier.subresourceRange.baseArrayLayer = 0;
		imagebarrier.subresourceRange.baseMipLevel = 0;
		imagebarrier.subresourceRange.layerCount = 1;
		imagebarrier.subresourceRange.levelCount = 1;
		imagebarrier.image = textureImage;

		imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.srcAccessMask = 0;
		imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageExtent = extent;
		region.imageOffset = { 0,0,0 };
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.mipLevel = 0;
		vkCmdCopyBufferToImage(cb, stagingbuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkSubmitInfo submitinfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

		vkQueueWaitIdle(GraphicNComputeQueue);

		CleanupBuffer(LDevice, stagingbuffer, stagingbuffermemory, true);
	
		if (shouldFreePixels && in_pixels == nullptr) {
			stbi_image_free((void*)pixels);
		}

		textureImageView = CreateImageView(LDevice, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		VkSamplerCreateInfo samplerinfo{};
		samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.anisotropyEnable = VK_FALSE;

		samplerinfo.magFilter = VK_FILTER_LINEAR;
		samplerinfo.minFilter = VK_FILTER_LINEAR;
		vkCreateSampler(LDevice, &samplerinfo, Allocator, &textureImageSampler);
	}

	// add mipmap and anisotropy
	inline void CreateTextureResources2(const char* image_path, ObjResource &objRes,
		VkPhysicalDevice PDevice = nullptr, VkDevice LDevice = nullptr, VkCommandPool CommandPool = nullptr, VkQueue GraphicNComputeQueue = nullptr)
	{
		int texWidth, texHeight, texChannels;
		const stbi_uc* pixels;
		VkDeviceSize size;

		pixels = stbi_load(image_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		size = texWidth * texHeight * 4;
		if (!pixels) {
			throw std::runtime_error("Failed to load texture image!");
		}

		VkBuffer stagingbuffer;
		VkDeviceMemory stagingbuffermemory;
		CreateBuffer(PDevice, LDevice, stagingbuffer, stagingbuffermemory, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* data;
		vkMapMemory(LDevice, stagingbuffermemory, 0, size, 0, &data);
		memcpy(data, pixels, size);

		VkExtent3D extent = { static_cast<uint32_t>(texWidth),static_cast<uint32_t>(texHeight),1 };
		CreateImage(PDevice, LDevice, objRes.textureImage, objRes.textureImageMemory, extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT);

		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkImageMemoryBarrier imagebarrier{};
		imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imagebarrier.subresourceRange.baseArrayLayer = 0;
		imagebarrier.subresourceRange.baseMipLevel = 0;
		imagebarrier.subresourceRange.layerCount = 1;
		imagebarrier.subresourceRange.levelCount = 1;
		imagebarrier.image = objRes.textureImage;

		imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.srcAccessMask = 0;
		imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageExtent = extent;
		region.imageOffset = { 0,0,0 };
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.mipLevel = 0;
		vkCmdCopyBufferToImage(cb, stagingbuffer, objRes.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkSubmitInfo submitinfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

		vkQueueWaitIdle(GraphicNComputeQueue);

		CleanupBuffer(LDevice, stagingbuffer, stagingbuffermemory, true);
		stbi_image_free((void*)pixels);

		objRes.textureImageView = CreateImageView(LDevice, objRes.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		VkSamplerCreateInfo samplerinfo{};
		samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		samplerinfo.anisotropyEnable = VK_TRUE;
		samplerinfo.maxAnisotropy = 16.0f;
		samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerinfo.mipLodBias = 0.0f;
		samplerinfo.minLod = 0.0;
		samplerinfo.maxLod = VK_LOD_CLAMP_NONE;

		samplerinfo.magFilter = VK_FILTER_LINEAR;
		samplerinfo.minFilter = VK_FILTER_LINEAR;
		vkCreateSampler(LDevice, &samplerinfo, Allocator, &objRes.textureSampler);
	}

	inline void CreateTextureResources3(const char* image_path, ObjResource& objRes,
		VkPhysicalDevice PDevice = nullptr, VkDevice LDevice = nullptr, VkCommandPool CommandPool = nullptr, VkQueue GraphicNComputeQueue = nullptr)
	{
		int texWidth, texHeight, texChannels;
		const stbi_uc* pixels;
		VkDeviceSize size;

		pixels = stbi_load(image_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		size = texWidth * texHeight * 4;
		if (!pixels) {
			throw std::runtime_error("Failed to load texture image!");
		}

		VkBuffer stagingbuffer;
		VkDeviceMemory stagingbuffermemory;
		CreateBuffer(PDevice, LDevice, stagingbuffer, stagingbuffermemory, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* data;
		vkMapMemory(LDevice, stagingbuffermemory, 0, size, 0, &data);
		memcpy(data, pixels, size);

		VkExtent3D extent = { static_cast<uint32_t>(texWidth),static_cast<uint32_t>(texHeight),1 };
		CreateImage(PDevice, LDevice, objRes.textureImage2, objRes.textureImageMemory2, extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT);

		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkImageMemoryBarrier imagebarrier{};
		imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imagebarrier.subresourceRange.baseArrayLayer = 0;
		imagebarrier.subresourceRange.baseMipLevel = 0;
		imagebarrier.subresourceRange.layerCount = 1;
		imagebarrier.subresourceRange.levelCount = 1;
		imagebarrier.image = objRes.textureImage2;

		imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.srcAccessMask = 0;
		imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageExtent = extent;
		region.imageOffset = { 0,0,0 };
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.mipLevel = 0;
		vkCmdCopyBufferToImage(cb, stagingbuffer, objRes.textureImage2, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkSubmitInfo submitinfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

		vkQueueWaitIdle(GraphicNComputeQueue);

		CleanupBuffer(LDevice, stagingbuffer, stagingbuffermemory, true);
		stbi_image_free((void*)pixels);

		objRes.textureImageView2 = CreateImageView(LDevice, objRes.textureImage2, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		VkSamplerCreateInfo samplerinfo{};
		samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.anisotropyEnable = VK_FALSE;

		samplerinfo.magFilter = VK_FILTER_LINEAR;
		samplerinfo.minFilter = VK_FILTER_LINEAR;
		vkCreateSampler(LDevice, &samplerinfo, Allocator, &objRes.textureSampler2);
	}

	inline void CreateTextureResourcesPBR(const char* image_path, ObjResource& objRes, int tidx,
		VkPhysicalDevice PDevice = nullptr, VkDevice LDevice = nullptr, VkCommandPool CommandPool = nullptr, VkQueue GraphicNComputeQueue = nullptr)
	{
		int texWidth, texHeight, texChannels;
		const stbi_uc* pixels;
		VkDeviceSize size;

		pixels = stbi_load(image_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		size = texWidth * texHeight * 4;
		if (!pixels) {
			throw std::runtime_error("Failed to load texture image!");
		}

		VkBuffer stagingbuffer;
		VkDeviceMemory stagingbuffermemory;
		CreateBuffer(PDevice, LDevice, stagingbuffer, stagingbuffermemory, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* data;
		vkMapMemory(LDevice, stagingbuffermemory, 0, size, 0, &data);
		memcpy(data, pixels, size);

		VkExtent3D extent = { static_cast<uint32_t>(texWidth),static_cast<uint32_t>(texHeight),1 };
		CreateImage(PDevice, LDevice, objRes.textureImagePBR[tidx], objRes.textureImageMemoryPBR[tidx], extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT);

		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkImageMemoryBarrier imagebarrier{};
		imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imagebarrier.subresourceRange.baseArrayLayer = 0;
		imagebarrier.subresourceRange.baseMipLevel = 0;
		imagebarrier.subresourceRange.layerCount = 1;
		imagebarrier.subresourceRange.levelCount = 1;
		imagebarrier.image = objRes.textureImagePBR[tidx];

		imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.srcAccessMask = 0;
		imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageExtent = extent;
		region.imageOffset = { 0,0,0 };
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.mipLevel = 0;
		vkCmdCopyBufferToImage(cb, stagingbuffer, objRes.textureImagePBR[tidx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

		VkSubmitInfo submitinfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

		vkQueueWaitIdle(GraphicNComputeQueue);

		CleanupBuffer(LDevice, stagingbuffer, stagingbuffermemory, true);
		stbi_image_free((void*)pixels);

		objRes.textureImageViewPBR[tidx] = CreateImageView(LDevice, objRes.textureImagePBR[tidx], VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		VkSamplerCreateInfo samplerinfo{};
		samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerinfo.anisotropyEnable = VK_FALSE;

		samplerinfo.magFilter = VK_FILTER_LINEAR;
		samplerinfo.minFilter = VK_FILTER_LINEAR;
		vkCreateSampler(LDevice, &samplerinfo, Allocator, &objRes.textureSamplerPBR[tidx]);
	}


	inline void CreateBuffer(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mempropperties
	)
	{
		VkBufferCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createinfo.size = size;
		createinfo.usage = usage;
		if (vkCreateBuffer(LDevice, &createinfo, Allocator, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create buffer!");
		}
		VkMemoryRequirements requirements{};
		vkGetBufferMemoryRequirements(LDevice, buffer, &requirements);
		VkMemoryAllocateInfo allocateinfo{};
		allocateinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateinfo.allocationSize = requirements.size;
		allocateinfo.memoryTypeIndex = ChooseMemoryType(PDevice, requirements.memoryTypeBits, mempropperties);

		if (vkAllocateMemory(LDevice, &allocateinfo, Allocator, &memory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate memory for buffer!");
		}
		vkBindBufferMemory(LDevice, buffer, memory, 0);
	}
}


