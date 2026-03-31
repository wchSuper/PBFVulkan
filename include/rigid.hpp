#pragma once
#include <vector>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "extensionfuncs.h"
#include "tiny_obj_loader.h"
#include <iostream>
#include <variant>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "utils.hpp"
#include "buffer.hpp"
#include "base.hpp"
#include "tiny_gltf.h"
#define Allocator nullptr


// rigid particles to .obj rendering

class RigidCube
{
private:
	ObjResource rigid_obj;
	UniformRenderingObject render_obj;

public:
	void CreateObjFromParticle(
		VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
		const std::vector<ExtraParticle> &initialParticles, 
		const ShapeMatchingInfo &smi
	)
	{
		// get vertices and indices from gpu buffer
		glm::vec3 rigidBoxMin(std::numeric_limits<float>::max());
		glm::vec3 rigidBoxMax(std::numeric_limits<float>::lowest());

		for (const auto& p : initialParticles) {
			rigidBoxMin = glm::min(rigidBoxMin, p.Location);
			rigidBoxMax = glm::max(rigidBoxMax, p.Location);
		}

		std::vector<glm::vec3> p = {
			glm::vec3(rigidBoxMin.x, rigidBoxMin.y, rigidBoxMax.z), // 0 front-bottom-left
			glm::vec3(rigidBoxMax.x, rigidBoxMin.y, rigidBoxMax.z), // 1 front-bottom-right
			glm::vec3(rigidBoxMax.x, rigidBoxMin.y, rigidBoxMin.z), // 2 back-bottom-right
			glm::vec3(rigidBoxMin.x, rigidBoxMin.y, rigidBoxMin.z), // 3 back-bottom-left
			glm::vec3(rigidBoxMin.x, rigidBoxMax.y, rigidBoxMax.z), // 4 front-top-left
			glm::vec3(rigidBoxMax.x, rigidBoxMax.y, rigidBoxMax.z), // 5 front-top-right
			glm::vec3(rigidBoxMax.x, rigidBoxMax.y, rigidBoxMin.z), // 6 back-top-right
			glm::vec3(rigidBoxMin.x, rigidBoxMax.y, rigidBoxMin.z)  // 7 back-top-left
		};

		rigid_obj.vertices.clear();
		rigid_obj.indices.clear();

		auto make_face = [&](const std::vector<int>& vert_indices, const glm::vec3& normal) {
			uint32_t base_index = static_cast<uint32_t>(rigid_obj.vertices.size());
			rigid_obj.vertices.push_back({ p[vert_indices[0]], normal, glm::vec2(0.0f, 0.0f) });
			rigid_obj.vertices.push_back({ p[vert_indices[1]], normal, glm::vec2(1.0f, 0.0f) });
			rigid_obj.vertices.push_back({ p[vert_indices[2]], normal, glm::vec2(1.0f, 1.0f) });
			rigid_obj.vertices.push_back({ p[vert_indices[3]], normal, glm::vec2(0.0f, 1.0f) });
			rigid_obj.indices.insert(rigid_obj.indices.end(), { base_index, base_index + 1, base_index + 2, base_index, base_index + 2, base_index + 3 });
		};

		make_face({ 0, 1, 5, 4 }, glm::vec3(0.0f, 0.0f, 1.0f));   // Front
		make_face({ 2, 3, 7, 6 }, glm::vec3(0.0f, 0.0f, -1.0f));  // Back
		make_face({ 1, 2, 6, 5 }, glm::vec3(1.0f, 0.0f, 0.0f));   // Right
		make_face({ 3, 0, 4, 7 }, glm::vec3(-1.0f, 0.0f, 0.0f));  // Left
		make_face({ 4, 5, 6, 7 }, glm::vec3(0.0f, 1.0f, 0.0f));   // Top
		make_face({ 3, 2, 1, 0 }, glm::vec3(0.0f, -1.0f, 0.0f));  // Bottom
		
		// create vertex index buffer  (buffer.hpp)	
		Buffer::CreateObjVertexIndexBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue, rigid_obj);
	}

	void CreateUniformBuffer(VkPhysicalDevice PDevice, VkDevice LDevice)
	{
		// 1. renderingUniform
		{
			VkDeviceSize size = sizeof(UniformRenderingObject);
			Buffer::CreateBuffer(PDevice, LDevice, rigid_obj.UniformBuffer, rigid_obj.UniformBufferMemory, size,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkMapMemory(LDevice, rigid_obj.UniformBufferMemory, 0, size, 0, &rigid_obj.MappedBuffer);
			memcpy(rigid_obj.MappedBuffer, &render_obj, size);
		}

		// 2. others ... 
	}

	void CreateDescriptorSetLayout(VkDevice LDevice)
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT }
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, rigid_obj.descriptorSetLayout);
	}

	void CreateDescriptorSet(VkDevice LDevice, VkDescriptorPool& DescriptorPool, VkBuffer& ShapeMatchingBuffer)
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, rigid_obj.descriptorSetLayout, rigid_obj.descriptorSet);
		std::vector<VkWriteDescriptorSet> writes(2);

		VkDescriptorBufferInfo renderingbufferinfo{};
		renderingbufferinfo.buffer = rigid_obj.UniformBuffer;
		renderingbufferinfo.offset = 0;
		renderingbufferinfo.range = sizeof(UniformRenderingObject);

		VkDescriptorBufferInfo shapeMatchingBufferInfo{};
		shapeMatchingBufferInfo.buffer = ShapeMatchingBuffer;
		shapeMatchingBufferInfo.offset = 0;
		shapeMatchingBufferInfo.range = sizeof(ShapeMatchingInfo);

		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo,     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{shapeMatchingBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, rigid_obj.descriptorSet);
	}

	void CreateGraphicPipelineLayout(VkDevice LDevice)
	{
		Base::CreateGraphicPipelineLayout(LDevice, rigid_obj.descriptorSetLayout, rigid_obj.pipelineLayout);
	}

	void CreateGraphicPipeline(VkDevice LDevice, VkRenderPass &BoxRenderPass)
	{
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = { colorBlendAttachment };
		Base::CreateGraphicPipeline(
			LDevice, rigid_obj.pipelineLayout, BoxRenderPass, &rigid_obj.graphicPipeline,
			"resources/shaders/spv/obj_rigidcube.vert.spv",
			"resources/shaders/spv/obj_rigidcube.frag.spv",
			true, 2,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
			VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, colorBlendAttachments
		);
	}

	// insert into box command buffer recording
	inline void RecordRigidCubeCommandBuffer(VkCommandBuffer & BoxRenderingCommandBuffer)
	{
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindPipeline(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rigid_obj.graphicPipeline);
		vkCmdBindDescriptorSets(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rigid_obj.pipelineLayout, 0, 1, &rigid_obj.descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(BoxRenderingCommandBuffer, 0, 1, &rigid_obj.vertexBuffer, offsets);
		vkCmdBindIndexBuffer(BoxRenderingCommandBuffer, rigid_obj.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(BoxRenderingCommandBuffer, static_cast<uint32_t>(rigid_obj.indices.size()), 1, 0, 0, 0);
	}

	// create resources  
	void PreCreateResources(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, VkRenderPass& BoxRenderPass,
		VkDescriptorPool& DescriptorPool, const std::vector<ExtraParticle>& initialParticles, 
		const ShapeMatchingInfo& smi, const UniformRenderingObject& new_render_obj, VkBuffer &shape_matching_buffer)
	{
		render_obj = new_render_obj;
		CreateObjFromParticle(PDevice, LDevice, CommandPool, GraphicNComputeQueue, initialParticles, smi);
		CreateUniformBuffer(PDevice, LDevice);
		CreateDescriptorSetLayout(LDevice);
		CreateDescriptorSet(LDevice, DescriptorPool, shape_matching_buffer);
		CreateGraphicPipelineLayout(LDevice);
		CreateGraphicPipeline(LDevice, BoxRenderPass);
	}

	void UpdateUniforms(const UniformRenderingObject& new_render_obj)
	{
		render_obj = new_render_obj;
		if (rigid_obj.MappedBuffer) {
			memcpy(rigid_obj.MappedBuffer, &render_obj, sizeof(UniformRenderingObject));
		}
	}


	inline void Cleanup(VkDevice LDevice)
	{
		vkDestroyPipeline(LDevice, rigid_obj.graphicPipeline, Allocator);
		vkDestroyPipelineLayout(LDevice, rigid_obj.pipelineLayout, Allocator);

		vkDestroyDescriptorSetLayout(LDevice, rigid_obj.descriptorSetLayout, Allocator);
		Buffer::CleanupBuffer(LDevice, rigid_obj.UniformBuffer, rigid_obj.UniformBufferMemory, true);
		Buffer::CleanupBuffer(LDevice, rigid_obj.vertexBuffer, rigid_obj.vertexBufferMemory, false);
		Buffer::CleanupBuffer(LDevice, rigid_obj.indexBuffer, rigid_obj.indexBufferMemory, false);
	}

};