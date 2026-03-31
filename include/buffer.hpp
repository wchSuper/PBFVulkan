#pragma once
#include <vector>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "extensionfuncs.h"
#include <iostream>
#include <stdexcept>
#include <variant>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "utils.hpp"
#define Allocator nullptr


namespace Buffer
{
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


	inline void CreateBuffer(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		VkBuffer& buffer, VkDeviceMemory& memory, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags mempropperties
	)
	{
		if (size == 0) {
			throw std::runtime_error("CreateBuffer: size must be greater than 0");
		}
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
	
	inline void CleanupBuffer(VkDevice LDevice, VkBuffer& buffer, VkDeviceMemory& memory, bool mapped)
	{
		if (mapped && memory != VK_NULL_HANDLE) {
			vkUnmapMemory(LDevice, memory);
		}
		if (buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(LDevice, buffer, Allocator);
		}
		if (memory != VK_NULL_HANDLE) {
			vkFreeMemory(LDevice, memory, Allocator);
		}
		buffer = VK_NULL_HANDLE;
		memory = VK_NULL_HANDLE;
	}

	inline void CreateBufferWithStage(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkDeviceSize Size,
		VkQueue GraphicNComputeQueue, const void* Src, VkBuffer& DstBuffer)
	{
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingMemory, Size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* data;
		vkMapMemory(LDevice, stagingMemory, 0, Size, 0, &data);
		memcpy(data, Src, Size);

		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkBufferCopy region{};
		region.size = Size;
		region.dstOffset = region.srcOffset = 0;
		vkCmdCopyBuffer(cb, stagingBuffer, DstBuffer, 1, &region);
		VkSubmitInfo submitInfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);

		vkDeviceWaitIdle(LDevice);
		Buffer::CleanupBuffer(LDevice, stagingBuffer, stagingMemory, true);
	}
	// ObjResource's Vertex Index Buffer Creating
	/*
		struct ObjResource {
			tinyobj::attrib_t attrib;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;
			std::string warn, err;

			std::vector<ObjVertex> vertices;
			std::vector<uint32_t> indices;
			std::vector<Voxel> voxels;
			glm::vec3 boxMin;
			glm::vec3 boxMax;
			float voxelSize;

			VkBuffer vertexBuffer;
			VkDeviceMemory vertexBufferMemory;

			VkBuffer indexBuffer;
			VkDeviceMemory indexBufferMemory;

			VkBuffer voxelBuffer;
			VkDeviceMemory voxelBufferMemory;

			UniformVoxelObject voxinfobj{};
			VkBuffer UniformBuffer;
			VkDeviceMemory UniformBufferMemory;
			void* MappedBuffer;

			VkDescriptorSetLayout descriptorSetLayout;
			VkDescriptorSet descriptorSet;
			VkPipelineLayout pipelineLayout;
			VkPipeline graphicPipeline;
		};
	*/

	// ObjVertex
	inline void CreateObjVertexIndexBuffer(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
		ObjResource& objRes)
	{
		if (objRes.vertices.empty() || objRes.indices.empty()) {
			objRes.vertexBuffer = VK_NULL_HANDLE;
			objRes.vertexBufferMemory = VK_NULL_HANDLE;
			objRes.indexBuffer = VK_NULL_HANDLE;
			objRes.indexBufferMemory = VK_NULL_HANDLE;
			return;
		}
		{
			VkDeviceSize vertexBufferSize = sizeof(ObjVertex) * objRes.vertices.size();
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingBufferMemory, vertexBufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			void* data;
			vkMapMemory(LDevice, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
			memcpy(data, objRes.vertices.data(), vertexBufferSize);

			Buffer::CreateBuffer(PDevice, LDevice, objRes.vertexBuffer, objRes.vertexBufferMemory, vertexBufferSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
			VkBufferCopy region{};
			region.size = vertexBufferSize;
			region.dstOffset = region.srcOffset = 0;
			vkCmdCopyBuffer(cb, stagingBuffer, objRes.vertexBuffer, 1, &region);
			VkSubmitInfo submitInfo{};
			Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
			vkDeviceWaitIdle(LDevice);
			CleanupBuffer(LDevice, stagingBuffer, stagingBufferMemory, true);
		}
		{
			VkDeviceSize indexBufferSize = sizeof(uint32_t) * objRes.indices.size();
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingBufferMemory, indexBufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			void* data;
			vkMapMemory(LDevice, stagingBufferMemory, 0, indexBufferSize, 0, &data);
			memcpy(data, objRes.indices.data(), indexBufferSize);
			Buffer::CreateBuffer(
				PDevice, LDevice,
				objRes.indexBuffer,
				objRes.indexBufferMemory,
				indexBufferSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
			VkBufferCopy region{};
			region.size = indexBufferSize;
			region.dstOffset = region.srcOffset = 0;
			vkCmdCopyBuffer(cb, stagingBuffer, objRes.indexBuffer, 1, &region);
			VkSubmitInfo submitInfo{};
			Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
			vkDeviceWaitIdle(LDevice);
			CleanupBuffer(LDevice, stagingBuffer, stagingBufferMemory, true);
		}
	}

	// SphereVertex
	inline void CreateSphereVertexIndexBuffer(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
		SphereResource& sphereRes)
	{
		{
			VkDeviceSize vertexBufferSize = sizeof(SphereVertex) * sphereRes.vertices.size();
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingBufferMemory, vertexBufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			void* data;
			vkMapMemory(LDevice, stagingBufferMemory, 0, vertexBufferSize, 0, &data);
			memcpy(data, sphereRes.vertices.data(), vertexBufferSize);

			Buffer::CreateBuffer(PDevice, LDevice, sphereRes.vertexBuffer, sphereRes.vertexBufferMemory, vertexBufferSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
			VkBufferCopy region{};
			region.size = vertexBufferSize;
			region.dstOffset = region.srcOffset = 0;
			vkCmdCopyBuffer(cb, stagingBuffer, sphereRes.vertexBuffer, 1, &region);
			VkSubmitInfo submitInfo{};
			Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
			vkDeviceWaitIdle(LDevice);
			CleanupBuffer(LDevice, stagingBuffer, stagingBufferMemory, true);
		}
		{
			VkDeviceSize indexBufferSize = sizeof(uint32_t) * sphereRes.indices.size();
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingBufferMemory, indexBufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			void* data;
			vkMapMemory(LDevice, stagingBufferMemory, 0, indexBufferSize, 0, &data);
			memcpy(data, sphereRes.indices.data(), indexBufferSize);
			Buffer::CreateBuffer(
				PDevice, LDevice,
				sphereRes.indexBuffer,
				sphereRes.indexBufferMemory,
				indexBufferSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
			VkBufferCopy region{};
			region.size = indexBufferSize;
			region.dstOffset = region.srcOffset = 0;
			vkCmdCopyBuffer(cb, stagingBuffer, sphereRes.indexBuffer, 1, &region);
			VkSubmitInfo submitInfo{};
			Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
			vkDeviceWaitIdle(LDevice);
			CleanupBuffer(LDevice, stagingBuffer, stagingBufferMemory, true);
		}
	}

	// ------------------------------------- get data from buffer --------------------------------------------------- //
	inline std::vector<uint32_t> RetrieveVoxelDataFromGPU
	(
		VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
		ObjResource& objRes
	)
	{
		VkDeviceSize size = VOXEL_RES * VOXEL_RES * VOXEL_RES * sizeof(uint32_t);
		std::vector<uint32_t> counts(VOXEL_RES * VOXEL_RES * VOXEL_RES);

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;
		Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingMemory, size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VkCommandBuffer cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;

		vkCmdCopyBuffer(cb, objRes.voxelBuffer, stagingBuffer, 1, &copyRegion);
		VkSubmitInfo submitInfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
		void* data;
		vkMapMemory(LDevice, stagingMemory, 0, size, 0, &data);
		memcpy(counts.data(), data, size);
		vkUnmapMemory(LDevice, stagingMemory);

		CleanupBuffer(LDevice, stagingBuffer, stagingMemory, false);

		return counts;
	}

}