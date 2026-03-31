#pragma once
#include <vector>
#include <map>
#include <iostream>
#include <variant>
#include <cassert>
#include <algorithm>
#include <cmath>
#include <queue>
#include <utility>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "extensionfuncs.h"
#include "tiny_obj_loader.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "base.hpp"
#include "utils.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "scene.hpp"
#include "tiny_gltf.h"
#define Allocator nullptr

#define M_PI 3.1415926
#define BIG_NUM 999999999


/*
	1. voxelize  AABB Tree ? collision detection
	2. sdf
*/

struct CustomUniformData {
	alignas(16) glm::mat4 model;
	alignas(4)  uint32_t triangleCount;
	alignas(4)  uint32_t modelInnerNum;       // model outer: voxel=0    model surface: voxel>0   model inner: voxel=999999999(modelInnerNum)
};

class SDF {
public:
	VkBuffer customVertBuffer;
	VkDeviceMemory customVertBufferMemory;

	VkBuffer customIndexBuffer;
	VkDeviceMemory customIndexBufferMemory;

	VkBuffer customUniformBuffer;
	VkDeviceMemory customUniformBufferMemory;
	void* mappedCustomBuffer;
	CustomUniformData uniformData;

	VkDescriptorSetLayout sdfDescriptorSetLayout;
	VkDescriptorSet sdfDescriptorSet;

	VkPipelineLayout sdfPipelineLayout;
	VkPipeline sdfPipeline;

	VkCommandBuffer sdfCommandBuffer;

	uint32_t GRID_WORK_GROUP_COUNT;

	// key resources
	std::vector<float> sdfValues;
	std::vector<uint32_t> counts;
	uint32_t resX, resY, resZ;

	void CreateBuffers(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, 
		ObjResource& objRes, UniformRenderingObject renderObj)
	{
		// 1. uniform buffer (1) from renderer.cpp  ==> objRes.voxelUniformBuffer
		{
			uint32_t vertCount = objRes.indices.size();
			assert(vertCount % 3 == 0);
			uint32_t triangleCount = vertCount / 3;
			uniformData.triangleCount = triangleCount;
			uniformData.modelInnerNum = BIG_NUM;
			uniformData.model = renderObj.model;

			VkDeviceSize size = sizeof(CustomUniformData);
			Buffer::CreateBuffer(PDevice, LDevice, customUniformBuffer, customUniformBufferMemory, size,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkMapMemory(LDevice, customUniformBufferMemory, 0, size, 0, &mappedCustomBuffer);
			memcpy(mappedCustomBuffer, &uniformData, size);
		}

		// 2. gpu buffer (1) from renderer.cpp ==> objRes.voxelBuffer
		{
			VkDeviceSize size = sizeof(ObjVertex) * objRes.vertices.size();
			Buffer::CreateBuffer(PDevice, LDevice, customVertBuffer, customVertBufferMemory, size,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, objRes.vertices.data(), customVertBuffer);
		}
		{
			VkDeviceSize size = sizeof(uint32_t) * objRes.indices.size();
			Buffer::CreateBuffer(PDevice, LDevice, customIndexBuffer, customIndexBufferMemory, size,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, objRes.indices.data(), customIndexBuffer);
		}
	}

	void CreateDescriptorSet(VkDevice LDevice, VkDescriptorPool DescriptorPool, ObjResource& objRes)
	{
		// 1. descriptorset layout
		{
			std::vector<BindingDesc> descs = {
				{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
				{ 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
				{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
				{ 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
				{ 4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT }
			};
			Base::descriptorSetLayoutBinding(LDevice, descs, sdfDescriptorSetLayout);
		}

		// 2. descriptorset
		{
			Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, sdfDescriptorSetLayout, sdfDescriptorSet);
			std::vector<VkWriteDescriptorSet> writes(5);
			VkDescriptorBufferInfo uniformBufferInfo1;
			uniformBufferInfo1.buffer = objRes.UniformBuffer;
			uniformBufferInfo1.offset = 0;
			uniformBufferInfo1.range = sizeof(UniformVoxelObject);

			VkDescriptorBufferInfo uniformBufferInfo2;
			uniformBufferInfo2.buffer = customUniformBuffer;
			uniformBufferInfo2.offset = 0;
			uniformBufferInfo2.range = sizeof(CustomUniformData);

			VkDescriptorBufferInfo storageBufferInfo1;
			storageBufferInfo1.buffer = objRes.voxelBuffer;
			storageBufferInfo1.offset = 0;
			storageBufferInfo1.range = sizeof(uint32_t) * VOXEL_RES * VOXEL_RES * VOXEL_RES;

			VkDescriptorBufferInfo storageBufferInfo2;
			storageBufferInfo2.buffer = customVertBuffer;
			storageBufferInfo2.offset = 0;
			storageBufferInfo2.range = sizeof(ObjVertex) * objRes.vertices.size();

			VkDescriptorBufferInfo storageBufferInfo3;
			storageBufferInfo3.buffer = customIndexBuffer;
			storageBufferInfo3.offset = 0;
			storageBufferInfo3.range = sizeof(uint32_t) * objRes.indices.size();

			std::vector<WriteDesc> writeInfo = {
				{uniformBufferInfo1,     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
				{uniformBufferInfo2,     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
				{storageBufferInfo1,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
				{storageBufferInfo2,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
				{storageBufferInfo3,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}
			};
			Base::DescriptorWrite(LDevice, writeInfo, writes, sdfDescriptorSet);
		}
	}

	void CreateSimPipeline(VkDevice LDevice)
	{
		// pipelinelayout
		{
			VkPipelineLayoutCreateInfo simulateCreateInfo{};
			simulateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			simulateCreateInfo.pSetLayouts = &sdfDescriptorSetLayout;
			simulateCreateInfo.setLayoutCount = 1;
			if (vkCreatePipelineLayout(LDevice, &simulateCreateInfo, Allocator, &sdfPipelineLayout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create sdf pipeline layout!");
			}
		}
		// pipeline
		{
			auto voxel_raycast = Base::MakeShaderModule(LDevice, "resources/shaders/spv/voxel_raycast.comp.spv");
			std::vector<VkShaderModule> shadermodules = {
				voxel_raycast
			};
			std::vector<VkPipeline*> computePipelines = {
				&sdfPipeline
			};
			for (uint32_t i = 0; i < shadermodules.size(); ++i) {
				VkPipelineShaderStageCreateInfo stageInfo{};
				stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stageInfo.pName = "main";
				stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
				stageInfo.module = shadermodules[i];
				VkComputePipelineCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
				createInfo.layout = sdfPipelineLayout;
				createInfo.stage = stageInfo;
				if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &createInfo, Allocator, computePipelines[i]) != VK_SUCCESS) {
					throw std::runtime_error("failed to create sdf compute pipeline!");
				}
			}
			for (auto& computeshadermodule : shadermodules) {
				vkDestroyShaderModule(LDevice, computeshadermodule, Allocator);
			}
		}
	}

	void RecordSimCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool)
	{
		Base::AllocateCommandBuffer(LDevice, CommandPool, sdfCommandBuffer);

		VkCommandBufferBeginInfo begininfo{};
		begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		if (vkBeginCommandBuffer(sdfCommandBuffer, &begininfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin sdf command buffer!");
		}
		VkMemoryBarrier memorybarrier{};
		memorybarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memorybarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		memorybarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(sdfCommandBuffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		memorybarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		memorybarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		vkCmdBindDescriptorSets(sdfCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sdfPipelineLayout, 0, 1, &sdfDescriptorSet, 0, nullptr);
		vkCmdPipelineBarrier(sdfCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		vkCmdBindPipeline(sdfCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, sdfPipeline);
		vkCmdDispatch(sdfCommandBuffer, GRID_WORK_GROUP_COUNT, 1, 1);

		auto result = vkEndCommandBuffer(sdfCommandBuffer);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to end sdf command buffer!");
		}
	}

	VkCommandBuffer* GetSimCommandBuffer()
	{
		return &sdfCommandBuffer;
	}
	
	void PreCreateResources(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
		VkDescriptorPool DescriptorPool, ObjResource& objRes, UniformRenderingObject& renderObj)
	{
		GRID_WORK_GROUP_COUNT = VOXEL_RES * VOXEL_RES * VOXEL_RES % 256 == 0 ?
			VOXEL_RES * VOXEL_RES * VOXEL_RES / 256 : VOXEL_RES * VOXEL_RES * VOXEL_RES / 256 + 1;
		CreateBuffers(PDevice, LDevice, CommandPool, GraphicNComputeQueue, objRes, renderObj);
		CreateDescriptorSet(LDevice, DescriptorPool, objRes);
		CreateSimPipeline(LDevice);
		RecordSimCommandBuffer(LDevice, CommandPool);
	}

	void Cleanup(VkDevice LDevice)
	{
		vkDeviceWaitIdle(LDevice);

		Buffer::CleanupBuffer(LDevice, customUniformBuffer, customUniformBufferMemory, true);
		Buffer::CleanupBuffer(LDevice, customVertBuffer, customVertBufferMemory, false);
		Buffer::CleanupBuffer(LDevice, customIndexBuffer, customIndexBufferMemory, false);

		vkDestroyPipeline(LDevice, sdfPipeline, Allocator);
		vkDestroyPipelineLayout(LDevice, sdfPipelineLayout, Allocator);

		vkDestroyDescriptorSetLayout(LDevice, sdfDescriptorSetLayout, Allocator);
	}


	// --------------------------------------------------------- make sdf in cpu  core codes ------------------------------------------------------- //
	template <typename T>
	constexpr const T& Min(const T& a, const T& b) noexcept
	{
		return (b < a) ? b : a;
	}

	template <typename T>
	constexpr const T& Max(const T& a, const T& b) noexcept
	{
		return (a < b) ? b : a;
	}

	std::vector<float> GetSDFValue() const
	{
		return sdfValues;
	}

	uint32_t Index3_1(uint32_t x, uint32_t y, uint32_t z)
	{
		return y * resX * resZ + z * resX + x;
	}

	glm::uvec3 Index1_3(uint32_t idx) const
	{
		uint32_t sy = idx / (resZ * resX);
		uint32_t remainder = idx % (resZ * resX);
		uint32_t sz = remainder / resX;
		uint32_t sx = remainder % resX;
		glm::uvec3 sdfPos = { sx, sy, sz };
		return sdfPos;
	}

	uint32_t Clamp(uint32_t val, uint32_t minVal, uint32_t maxVal) {
		return Max<uint32_t>(minVal, Min<uint32_t>(val, maxVal));
	}

	uint32_t SampleVoxel(uint32_t x, uint32_t y, uint32_t z) {
		x = Clamp(x, 0, resX - 1);
		y = Clamp(y, 0, resY - 1);
		z = Clamp(z, 0, resZ - 1);
		return counts[Index3_1(x, y, z)];
	}

	bool IsBoundary(uint32_t x, uint32_t y, uint32_t z, float& initDist) {
		bool center = (SampleVoxel(x, y, z) > 0);
		float minDistSq = BIG_NUM;

		for (int dz = -1; dz <= 1; ++dz) {
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					if (dx == 0 && dy == 0 && dz == 0) continue;
					bool neighbor = (SampleVoxel(x + dx, y + dy, z + dz) > 0);
					if (neighbor != center) {
						float dSq = static_cast<float>(dx * dx + dy * dy + dz * dz);
						if (dSq < minDistSq) minDistSq = dSq;
					}
				}
			}
		}
		initDist = (minDistSq == BIG_NUM) ? 0.0f : sqrtf(minDistSq) * 0.5f;  
		return minDistSq != BIG_NUM;
	}

	void MakeSDF(ObjResource& objRes, std::vector<uint32_t> voxels)
	{
		resX = objRes.voxinfobj.resolution.x;
		resY = objRes.voxinfobj.resolution.y;
		resZ = objRes.voxinfobj.resolution.z;
		this->counts = voxels;

		uint32_t sdfCounts = resX * resY * resZ;
		sdfValues.assign(sdfCounts, BIG_NUM);

		// priority queue  <distance - index>
		using QueueItem = std::pair<float, int>;
		std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;

		// init queue (set initial distance on boundary)
		for (int z = 0; z < resZ; ++z) {
			for (int y = 0; y < resY; ++y) {
				for (int x = 0; x < resX; ++x) {
					float initDist;
					if (IsBoundary(x, y, z, initDist)) {
						uint32_t idx = Index3_1(x, y, z);
						sdfValues[idx] = initDist; 
						queue.push({ initDist, idx });
					}
				}
			}
		}
		// if there is no boundary!
		if (queue.empty()) {
			std::cout << "Warning: No boundaries found. Setting all SDF to 0\n";
			sdfValues.assign(sdfCounts, 0.0f);
			return;
		}

		// FMM
		while (!queue.empty()) {
			auto [dist, idx] = queue.top();
			queue.pop();
			if (dist > sdfValues[idx]) continue;

			glm::uvec3 sdfPos = Index1_3(idx);
			uint32_t sx = sdfPos.x, sy = sdfPos.y, sz = sdfPos.z;
			for (int dz = -1; dz <= 1; ++dz) {
				for (int dy = -1; dy <= 1; ++dy) {
					for (int dx = -1; dx <= 1; ++dx) {
						uint32_t nx = sx + dx, ny = sy + dy, nz = sz + dz;
						if (nx < 0 || nx >= resX || ny < 0 || ny >= resY || nz < 0 || nz >= resZ) {
							continue;
						}
						uint32_t nidx = Index3_1(nx, ny, nz);

						// compute new distance
						float newDist = dist + 
							sqrtf(static_cast<float>((nx - sx) * (nx - sx) + (ny - sy) * (ny - sy) + (nz - sz) * (nz - sz)));
						if (newDist < sdfValues[nidx]) {
							sdfValues[nidx] = newDist;
							queue.push({ newDist, nidx });
						}
					}
				}
			}
		}
		float scale = objRes.voxinfobj.voxelSize;
		for (uint32_t i = 0; i < sdfCounts; i++) {
			if (voxels[i] > 0) {
				sdfValues[i] = -sdfValues[i];
			}
			sdfValues[i] *= scale;
		}
	}
};