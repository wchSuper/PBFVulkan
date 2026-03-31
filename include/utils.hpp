#pragma once
#include <vector>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "extensionfuncs.h"
#include <iostream>
#include <variant>
#include <fstream>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"


namespace Util
{
	inline VkCommandBuffer CreateCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool)
	{
		VkCommandBuffer cb;
		VkCommandBufferAllocateInfo allocateinfo{};
		allocateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateinfo.commandPool = CommandPool;
		allocateinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateinfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(LDevice, &allocateinfo, &cb) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffer!");
		}
		VkCommandBufferBeginInfo begininfo{};
		begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(cb, &begininfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin command buffer!");
		}
		return cb;
	}

	inline void SubmitCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool, VkCommandBuffer& cb, VkSubmitInfo submitinfo, VkFence fence, VkQueue queue)
	{
		submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitinfo.commandBufferCount = 1;
		submitinfo.pCommandBuffers = &cb;
		if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
			throw std::runtime_error("failed to end command buffer!");
		}
		if (vkQueueSubmit(queue, 1, &submitinfo, fence) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit command buffer!");
		}
		vkDeviceWaitIdle(LDevice);
		vkFreeCommandBuffers(LDevice, CommandPool, 1, &cb);
	}

	// file about
	inline void ReadFile(const char* filename, std::vector<char>& bytes)
	{
		std::ifstream ifs;
		ifs.open(filename, std::ios_base::ate | std::ios_base::binary);
		if (!ifs.is_open()) {
			std::string errinfo = "failed to load file ";
			errinfo += std::string(filename) + '!';
			throw std::runtime_error(errinfo);
		}
		uint32_t size = ifs.tellg();
		ifs.seekg(0);
		bytes.resize(size);
		ifs.read(bytes.data(), size);
		ifs.close();
	}

	inline void WriteParticles(const char* filename, std::vector<Particle> particles, std::vector<ExtraParticle> rigidParticles)
	{
		std::ofstream file(filename);
		if (file.is_open()) {
			for (const auto& particle : particles) {
				file << particle.Location.x << " " << particle.Location.y << " " << particle.Location.z << std::endl;
			}
			file.close();
		}
	}


}