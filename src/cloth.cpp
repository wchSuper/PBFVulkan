#include "cloth.h"


void Cloth::CreateResources(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, VkExtent2D SwapChainExtent)
{
	// 1. texture image
	{
		Image::CreateTextureResources(
			"resources/textures/cloth.jpg",
			ctRes.textureImage, ctRes.textureImageView, ctRes.textureImageMemory, ctRes.textureSampler,
			nullptr, 0, 0, 0, PDevice, LDevice, CommandPool, GraphicNComputeQueue
		);
	}
	// 2. color image and depth image
	{
		VkExtent3D extent = { SwapChainExtent.width, SwapChainExtent.height, 1 };
		Image::CreateImage(PDevice, LDevice, colorImage, colorImageMemory, extent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
		colorImageView = Image::CreateImageView(LDevice, colorImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
		Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue,
			colorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		Image::CreateImage(PDevice, LDevice, depthImage, depthImageMemory, extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_ASPECT_DEPTH_BIT, VK_SAMPLE_COUNT_1_BIT);
		depthImageView = Image::CreateImageView(LDevice, depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
		Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue,
			depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
	// 3. uniform buffers
	{
		// rendering uniform buffer
		{
			VkDeviceSize size = sizeof(UniformRenderingObject);
			Buffer::CreateBuffer(PDevice, LDevice, uniformRenderingBuffer, uniformRenderingBufferMemory, size,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkMapMemory(LDevice, uniformRenderingBufferMemory, 0, size, 0, &mappedRenderingBuffer);
			memcpy(mappedRenderingBuffer, &renderingObj, size);
		}

		// env uniform buffer for rendering 
		{
			VkDeviceSize size = sizeof(UniformEnvData);
			Buffer::CreateBuffer(PDevice, LDevice, uniformEnvBuffer, uniformEnvBufferMemory, size,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkMapMemory(LDevice, uniformEnvBufferMemory, 0, size, 0, &mappedEnvBuffer);
			memcpy(mappedEnvBuffer, &envObj, size);
		}

		// simulating uniform buffer
		{
			VkDeviceSize size = sizeof(UniformSimData);
			Buffer::CreateBuffer(PDevice, LDevice, uniformSimulatingBuffer, uniformSimulatingBufferMemory, size,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkMapMemory(LDevice, uniformSimulatingBufferMemory, 0, size, 0, &mappedSimulatingBuffer);
			memcpy(mappedSimulatingBuffer, &ctCp.uniformData, size);
		}
	}
}

void Cloth::CreateClothMeshAndRes(
	VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue)
{
	// gridSize
	clothPoints.resize(gridSize.x * gridSize.y);
	float dx = size.x / (gridSize.x - 1), dy = size.y / (gridSize.y - 1);
	float du = 1.0f / (gridSize.x - 1), dv = 1.0f / (gridSize.y - 1);

	for (uint32_t i = 0; i < gridSize.y; i++) {
		for (uint32_t j = 0; j < gridSize.x; j++) {
			uint32_t index = i * gridSize.x + j;  
			clothPoints[index].pos = glm::vec4(dx * j, 1.0f, dy * i, 1.0f);
			clothPoints[index].vel = glm::vec4(0.0f);
			clothPoints[index].uv = glm::vec4(1.0f - du * i, dv * j, 0.0f, 0.0f);
			clothPoints[index].normal = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f); 
		}
	}
	ctCp.uniformData.deltaT = 1 / 8000.0f;  
	ctCp.uniformData.particleMass = 0.05f;  
	ctCp.uniformData.springStiffness = 1000.0f;  
	ctCp.uniformData.damping = 0.5f;  
	ctCp.uniformData.restDistH = dx;
	ctCp.uniformData.restDistV = dy;
	ctCp.uniformData.restDistD = sqrtf(dx * dx + dy * dy);
	ctCp.uniformData.particleCount = gridSize;
	ctCp.uniformData.usePBD = 0;
	if (usePBD) {
		ctCp.uniformData.usePBD = 1;
		ctCp.uniformData.restDistStretch = dx;
		ctCp.uniformData.restDistShear = sqrtf(dx * dx + dy * dy);
		ctCp.uniformData.restDistBend = 2.0 * dx;
		ctCp.uniformData.deltaT = 1 / 400.0f;
	}
	WORK_GROUP_COUNT_X = gridSize[0] % ONE_GROUP_INVOCATION_COUNT_X == 0 ? gridSize[0] / ONE_GROUP_INVOCATION_COUNT_X :
		gridSize[0] / ONE_GROUP_INVOCATION_COUNT_X + 1;
	WORK_GROUP_COUNT_Y = gridSize[1] % ONE_GROUP_INVOCATION_COUNT_Y == 0 ? gridSize[1] / ONE_GROUP_INVOCATION_COUNT_Y : 
		gridSize[1] / ONE_GROUP_INVOCATION_COUNT_Y + 1;

	// Create clothPointBuffer 2 frame  storage buffer
	{
		clothPointBuffer.resize(MAXInFlightRendering);
		clothPointBufferMemory.resize(MAXInFlightRendering);
		VkDeviceSize size = clothPoints.size() * sizeof(ClothPoint);
		for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
			VkBuffer stagingbuffer;
			VkDeviceMemory stagingmemory;
			Buffer::CreateBuffer(PDevice, LDevice, stagingbuffer, stagingmemory, size,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			Buffer::CreateBuffer(PDevice, LDevice, clothPointBuffer[i], clothPointBufferMemory[i], size,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			void* data;
			vkMapMemory(LDevice, stagingmemory, 0, size, 0, &data);
			memcpy(data, clothPoints.data(), size);

			auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
			VkBufferCopy region{};
			region.size = size;
			region.dstOffset = region.srcOffset = 0;
			vkCmdCopyBuffer(cb, stagingbuffer, clothPointBuffer[i], 1, &region);
			VkSubmitInfo submitinfo{};
			Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

			vkDeviceWaitIdle(LDevice);
			Buffer::CleanupBuffer(LDevice, stagingbuffer, stagingmemory, true);
		}
	}

	// Create indices and indexBuffer
	{
		for (uint32_t y = 0; y < gridSize.y - 1; y++) {
			for (uint32_t x = 0; x < gridSize.x; x++) {
				ctRes.indices.push_back((y + 1) * gridSize.x + x);
				ctRes.indices.push_back(y * gridSize.x + x);
			}
			// Primitive restart
			ctRes.indices.push_back(0xFFFFFFFF);
		}

		VkDeviceSize indexBufferSize = sizeof(uint32_t) * ctRes.indices.size();
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingBufferMemory, indexBufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* data;
		vkMapMemory(LDevice, stagingBufferMemory, 0, indexBufferSize, 0, &data);
		memcpy(data, ctRes.indices.data(), indexBufferSize);
		Buffer::CreateBuffer(
			PDevice, LDevice,
			ctRes.indexBuffer,
			ctRes.indexBufferMemory,
			indexBufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkBufferCopy region{};
		region.size = indexBufferSize;
		region.dstOffset = region.srcOffset = 0;
		vkCmdCopyBuffer(cb, stagingBuffer, ctRes.indexBuffer, 1, &region);
		VkSubmitInfo submitInfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
		vkDeviceWaitIdle(LDevice);
		Buffer::CleanupBuffer(LDevice, stagingBuffer, stagingBufferMemory, true);
	}
}

void Cloth::CreateDescriptorSetLayout(VkDevice LDevice)
{
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, ctRes.descriptorSetLayout);
	}
	// simulating descriptor layout
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, ctCp.descriptorSetLayout);
	}
}

void Cloth::CreateDescriptorSet(VkDevice LDevice, VkDescriptorPool DescriptorPool)
{
	// Graphic
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, ctRes.descriptorSetLayout, ctRes.descriptorSet);
		std::vector<VkWriteDescriptorSet> writes(3);

		VkDescriptorBufferInfo renderingbufferinfo{};
		renderingbufferinfo.buffer = uniformRenderingBuffer;
		renderingbufferinfo.offset = 0;
		renderingbufferinfo.range = sizeof(renderingObj);

		VkDescriptorBufferInfo envbufferinfo{};
		envbufferinfo.buffer = uniformEnvBuffer;
		envbufferinfo.offset = 0;
		envbufferinfo.range = sizeof(envObj);

		VkDescriptorImageInfo textureimageinfo{};
		textureimageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textureimageinfo.imageView = ctRes.textureImageView;
		textureimageinfo.sampler = ctRes.textureSampler;

		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{envbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{textureimageinfo,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
		};
		assert(writeInfo.size() == writes.size());
		Base::DescriptorWrite(LDevice, writeInfo, writes, ctRes.descriptorSet);
	}

	// Simulate
	{
		ctCp.descriptorSet.resize(MAXInFlightRendering);
		for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
			Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, ctCp.descriptorSetLayout, ctCp.descriptorSet[i]);
		}
		std::vector<VkWriteDescriptorSet> writes(4);
		for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
			VkDescriptorBufferInfo renderingbufferinfo{};
			renderingbufferinfo.buffer = uniformRenderingBuffer;
			renderingbufferinfo.offset = 0;
			renderingbufferinfo.range = sizeof(renderingObj);

			VkDescriptorBufferInfo simulatingbufferinfo{};
			simulatingbufferinfo.buffer = uniformSimulatingBuffer;
			simulatingbufferinfo.offset = 0;
			simulatingbufferinfo.range = sizeof(ctCp.uniformData);

			VkDescriptorBufferInfo pointbufferinfo_last{};
			pointbufferinfo_last.buffer = clothPointBuffer[(i - 1 + MAXInFlightRendering) % MAXInFlightRendering];
			pointbufferinfo_last.offset = 0;
			pointbufferinfo_last.range = sizeof(ClothPoint) * clothPoints.size();

			VkDescriptorBufferInfo pointbufferinfo_current{};
			pointbufferinfo_current.buffer = clothPointBuffer[i % MAXInFlightRendering];
			pointbufferinfo_current.offset = 0;
			pointbufferinfo_current.range = sizeof(ClothPoint) * clothPoints.size();

			std::vector<WriteDesc> writeInfo = {
				{renderingbufferinfo,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
				{simulatingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
				{pointbufferinfo_last, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
				{pointbufferinfo_current, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}
			};
			Base::DescriptorWrite(LDevice, writeInfo, writes, ctCp.descriptorSet[i]);
		}
	}
}

void Cloth::CreateRenderPass(VkDevice LDevice, VkFormat SwapChainImageFormat)
{
	VkAttachmentDescription colorAttachement{};
	colorAttachement.format = SwapChainImageFormat;
	colorAttachement.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachement.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	std::array<VkAttachmentDescription, 2> attachments = { colorAttachement, depthAttachment };

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkSubpassDescription, 1> subpasses{};
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colorAttachmentRef;
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

	VkRenderPassCreateInfo createinfo{};
	createinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createinfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createinfo.pAttachments = attachments.data();
	createinfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	createinfo.pSubpasses = subpasses.data();
	if (vkCreateRenderPass(LDevice, &createinfo, Allocator, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create cloth-graphic renderpass!");
	}
}

void Cloth::CreateFrameBuffer(VkDevice LDevice, std::vector<VkImageView>& SwapChainImageViews, VkExtent2D SwapChainExtent)
{
	frameBuffer.resize(SwapChainImageViews.size());
	for (size_t i = 0; i < SwapChainImageViews.size(); i++) {
		std::array<VkImageView, 2> attachments = { SwapChainImageViews[i], depthImageView };
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = SwapChainExtent.width;
		framebufferInfo.height = SwapChainExtent.height;
		framebufferInfo.layers = 1;
		if (vkCreateFramebuffer(LDevice, &framebufferInfo, nullptr, &frameBuffer[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create cloth-framebuffer!");
		}
	}
}

void Cloth::CreateGraphicPipeline(VkDevice LDevice)
{
	Base::CreateGraphicPipelineLayout(LDevice, ctRes.descriptorSetLayout, ctRes.pipelineLayout);
	
	// color blend
	VkPipelineColorBlendAttachmentState clothcolorblendattachment{};
	clothcolorblendattachment.blendEnable = VK_FALSE;
	clothcolorblendattachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	std::vector<VkPipelineColorBlendAttachmentState> clothcolorblendattachments = { clothcolorblendattachment };
	VkPipelineColorBlendStateCreateInfo colorblend{};
	colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorblend.attachmentCount = static_cast<uint32_t>(clothcolorblendattachments.size());
	colorblend.logicOpEnable = VK_FALSE;
	colorblend.pAttachments = clothcolorblendattachments.data();

	// shader stages
	auto vertshadermodule = Base::MakeShaderModule(LDevice, "resources/shaders/spv/clothshader.vert.spv");
	auto fragshadermodule = Base::MakeShaderModule(LDevice, "resources/shaders/spv/clothshader.frag.spv");
	VkPipelineShaderStageCreateInfo vertshader{};
	vertshader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertshader.module = vertshadermodule;
	vertshader.pName = "main";
	vertshader.stage = VK_SHADER_STAGE_VERTEX_BIT;
	VkPipelineShaderStageCreateInfo fragshader{};
	fragshader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragshader.module = fragshadermodule;
	fragshader.pName = "main";
	fragshader.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	std::vector<VkPipelineShaderStageCreateInfo> shaderstages = { vertshader, fragshader };

	// dynamic state
	std::array<VkDynamicState, 2> dynamicstates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicstate{};
	dynamicstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicstate.dynamicStateCount = static_cast<uint32_t>(dynamicstates.size());
	dynamicstate.pDynamicStates = dynamicstates.data();

	// vertexinput
	VkPipelineVertexInputStateCreateInfo vertexinput{};
	vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VkVertexInputBindingDescription vertexinputbinding = ClothPoint::GetBinding();
	std::vector<VkVertexInputAttributeDescription> vertexinputattributes = ClothPoint::GetAttributes();
	vertexinput.vertexBindingDescriptionCount = 1;
	vertexinput.pVertexBindingDescriptions = &vertexinputbinding;
	vertexinput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexinputattributes.size());
	vertexinput.pVertexAttributeDescriptions = vertexinputattributes.data();

	// assembly
	VkPipelineInputAssemblyStateCreateInfo assembly{};
	assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	assembly.primitiveRestartEnable = VK_TRUE;                          // !
	assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;           // !

	// viewport & multisample
	VkPipelineViewportStateCreateInfo viewport{};
	viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;
	VkPipelineMultisampleStateCreateInfo multisample{};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// rasterization
	VkPipelineRasterizationStateCreateInfo rasterization{};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.depthClampEnable = VK_FALSE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.lineWidth = 1.0f;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;

	// depthstencil
	VkPipelineDepthStencilStateCreateInfo depthstencil{};
	depthstencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthstencil.depthTestEnable = VK_TRUE; 
	depthstencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthstencil.depthWriteEnable = VK_TRUE;
	depthstencil.maxDepthBounds = 1.0f;
	depthstencil.minDepthBounds = 0.0f;

	// create pipeline
	VkGraphicsPipelineCreateInfo createinfo{};
	createinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createinfo.layout = ctRes.pipelineLayout;
	createinfo.pColorBlendState = &colorblend;
	createinfo.pDepthStencilState = &depthstencil;
	createinfo.pDynamicState = &dynamicstate;
	createinfo.pInputAssemblyState = &assembly;
	createinfo.pMultisampleState = &multisample;
	createinfo.pRasterizationState = &rasterization;
	createinfo.pStages = shaderstages.data();
	createinfo.stageCount = static_cast<uint32_t>(shaderstages.size());
	createinfo.pVertexInputState = &vertexinput;
	createinfo.pViewportState = &viewport;
	createinfo.renderPass = renderPass;
	createinfo.subpass = 0;
	if (vkCreateGraphicsPipelines(LDevice, VK_NULL_HANDLE, 1, &createinfo, Allocator, &ctRes.graphicPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create cloth-graphic pipeline!");
	}
	vkDestroyShaderModule(LDevice, vertshadermodule, Allocator);
	vkDestroyShaderModule(LDevice, fragshadermodule, Allocator);
}

void Cloth::RecordGraphicCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool, VkExtent2D SwapChainImageExtent)
{
	commandBuffer.resize(frameBuffer.size());
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = CommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)commandBuffer.size();
	if (vkAllocateCommandBuffers(LDevice, &allocInfo, commandBuffer.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate cloth-command buffers!");
	}

	for (uint32_t i = 0; i < commandBuffer.size(); i++) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (vkBeginCommandBuffer(commandBuffer[i], &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = frameBuffer[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = SwapChainImageExtent;
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport;
		viewport.height = SwapChainImageExtent.height;
		viewport.width = SwapChainImageExtent.width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = viewport.y = 0;
		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = SwapChainImageExtent;
		vkCmdSetViewport(commandBuffer[i], 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer[i], 0, 1, &scissor);
		VkDeviceSize offset = 0;
		
		vkCmdBindPipeline(commandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ctRes.graphicPipeline);
		vkCmdBindDescriptorSets(commandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ctRes.pipelineLayout, 0, 1, &ctRes.descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(commandBuffer[i], 0, 1, &clothPointBuffer[i % 2], &offset);
		vkCmdBindIndexBuffer(commandBuffer[i], ctRes.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer[i], static_cast<uint32_t>(ctRes.indices.size()), 1, 0, 0, 0);
		vkCmdEndRenderPass(commandBuffer[i]);

		if (vkEndCommandBuffer(commandBuffer[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to record cloth-command buffer!");
		}
	}
}

void Cloth::CreateComputePipeline(VkDevice LDevice)
{
	// compute pipeline layout
	VkPipelineLayoutCreateInfo simulatecreateinfo{};
	simulatecreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	simulatecreateinfo.pSetLayouts = &ctCp.descriptorSetLayout;
	simulatecreateinfo.setLayoutCount = 1;
	if (vkCreatePipelineLayout(LDevice, &simulatecreateinfo, Allocator, &ctCp.pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create cloth-simulate pipeline layout!");
	}

	// compute pipeline
	auto computeshadermodule = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/cloth/spv/computecloth.comp.spv");
	auto computeshadermodulepbd1 = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/cloth/spv/computecloth_pbd1.comp.spv");
	auto computeshadermodulepbd2 = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/cloth/spv/computecloth_pbd2.comp.spv");
	auto computeshadermodulepbd3 = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/cloth/spv/computecloth_pbd3.comp.spv");
	std::vector<VkShaderModule> shadermodules = {};
	std::vector<VkPipeline*> pcomputepipelines = {};
	if (!usePBD) {
		shadermodules.push_back(computeshadermodule);
		pcomputepipelines.push_back(&ctCp.computePipeline);
	}
	else {
		shadermodules.push_back(computeshadermodulepbd1);
		shadermodules.push_back(computeshadermodulepbd2);
		shadermodules.push_back(computeshadermodulepbd3);
		pcomputepipelines.push_back(&ctCp.computePipelineEX1);
		pcomputepipelines.push_back(&ctCp.computePipelineEX2);
		pcomputepipelines.push_back(&ctCp.computePipelineEX3);
	}
	assert(shadermodules.size() > 0 && pcomputepipelines.size() > 0);
	for (uint32_t i = 0; i < shadermodules.size(); i++) {
		VkPipelineShaderStageCreateInfo stageinfo{};
		stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageinfo.pName = "main";
		stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stageinfo.module = shadermodules[i];
		VkComputePipelineCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		createinfo.layout = ctCp.pipelineLayout;
		createinfo.stage = stageinfo;
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &createinfo, Allocator, pcomputepipelines[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create simulating compute pipeline!");
		}
	}
	vkDestroyShaderModule(LDevice, computeshadermodule, Allocator);
	vkDestroyShaderModule(LDevice, computeshadermodulepbd1, Allocator);
	vkDestroyShaderModule(LDevice, computeshadermodulepbd2, Allocator);
	vkDestroyShaderModule(LDevice, computeshadermodulepbd3, Allocator);
}

void Cloth::RecordSimulatingCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool)
{
	simCommandBuffer.resize(MAXInFlightRendering);
	for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
		VkCommandBufferAllocateInfo allocateinfo{};
		allocateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateinfo.commandPool = CommandPool;
		allocateinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateinfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(LDevice, &allocateinfo, &simCommandBuffer[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate simulating command buffer!");
		}
		VkCommandBufferBeginInfo begininfo{};
		begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		if (vkBeginCommandBuffer(simCommandBuffer[i], &begininfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin simulating command buffer!");
		}

		// acquire from graphics
		VkMemoryBarrier acquireBarrier{};
		acquireBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		acquireBarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(simCommandBuffer[i], VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &acquireBarrier, 0, nullptr, 0, nullptr);

		if (!usePBD) {
			vkCmdBindPipeline(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.computePipeline);
			uint32_t readSet = 1;
			const uint32_t iterations = 4;
			for (uint32_t iter = 0; iter < iterations; iter++) {
				readSet = 1 - readSet;
				vkCmdBindDescriptorSets(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.pipelineLayout, 0, 1, &ctCp.descriptorSet[readSet], 0, nullptr);
				vkCmdDispatch(simCommandBuffer[i], WORK_GROUP_COUNT_X, WORK_GROUP_COUNT_Y, 1);
				if (iter != iterations - 1) {
					VkMemoryBarrier computeBarrier{};
					computeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
					computeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					computeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
					vkCmdPipelineBarrier(simCommandBuffer[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &computeBarrier, 0, nullptr, 0, nullptr);
				}
			}
		}
		else {
			VkMemoryBarrier computeBarrier{};
			computeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			computeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			computeBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			vkCmdBindDescriptorSets(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.pipelineLayout, 0, 1, &ctCp.descriptorSet[i], 0, nullptr);
			vkCmdBindPipeline(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.computePipelineEX1);
			vkCmdDispatch(simCommandBuffer[i], WORK_GROUP_COUNT_X, WORK_GROUP_COUNT_Y, 1);
			vkCmdPipelineBarrier(simCommandBuffer[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &computeBarrier, 0, nullptr, 0, nullptr);
			const uint32_t iterations = 4;
			for (uint32_t iter = 0; iter < iterations; iter++) {
				vkCmdBindPipeline(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.computePipelineEX2);
				vkCmdBindDescriptorSets(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.pipelineLayout, 0, 1, &ctCp.descriptorSet[i], 0, nullptr);
				vkCmdDispatch(simCommandBuffer[i], WORK_GROUP_COUNT_X, WORK_GROUP_COUNT_Y, 1);
				vkCmdPipelineBarrier(simCommandBuffer[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &computeBarrier, 0, nullptr, 0, nullptr);

				vkCmdBindPipeline(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.computePipelineEX3);
				vkCmdBindDescriptorSets(simCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, ctCp.pipelineLayout, 0, 1, &ctCp.descriptorSet[i], 0, nullptr);
				vkCmdDispatch(simCommandBuffer[i], WORK_GROUP_COUNT_X, WORK_GROUP_COUNT_Y, 1);
				if (iter != iterations - 1 || true) {
					vkCmdPipelineBarrier(simCommandBuffer[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &computeBarrier, 0, nullptr, 0, nullptr);
				}
			}
		}
	
		// Release to graphics
		VkMemoryBarrier releaseBarrier{};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		releaseBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		vkCmdPipelineBarrier(simCommandBuffer[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &releaseBarrier, 0, nullptr, 0, nullptr);

		auto result = vkEndCommandBuffer(simCommandBuffer[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to end cloth-simulating command buffer!");
		}
	}
}

// real-time updates ensure the camera perspective is consistent with 'renderer.cpp'
void Cloth::UpdateUniforms(const UniformRenderingObject& robj)
{
	if (mappedRenderingBuffer) {
		memcpy(mappedRenderingBuffer, &robj, sizeof(UniformRenderingObject));
	}
}

void Cloth::Init
(
	VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, VkExtent2D SwapChainExtent,
	VkFormat SwapChainImageFormat, std::vector<VkImageView>& SwapChainImageViews, VkDescriptorPool DescriptorPool, UniformRenderingObject* renderingobj,
	GLFWwindow* Window
)
{
	renderingObj = *renderingobj;
	envObj.lightPos = { -1.0f, 2.0f, -1.0f, 1.0f };
	usePBD = false;
	CreateClothMeshAndRes(PDevice, LDevice, CommandPool, GraphicNComputeQueue);
	CreateResources(PDevice, LDevice, CommandPool, GraphicNComputeQueue, SwapChainExtent);

	CreateDescriptorSetLayout(LDevice);
	CreateDescriptorSet(LDevice, DescriptorPool);

	CreateRenderPass(LDevice, SwapChainImageFormat);
	CreateFrameBuffer(LDevice, SwapChainImageViews, SwapChainExtent);

	CreateGraphicPipeline(LDevice);
	CreateComputePipeline(LDevice);

	RecordGraphicCommandBuffer(LDevice, CommandPool, SwapChainExtent);
	RecordSimulatingCommandBuffer(LDevice, CommandPool);
}

VkCommandBuffer Cloth::getCommandBufferForImage(uint32_t image_index) const
{
	return commandBuffer[image_index];
}

VkCommandBuffer Cloth::getCommandBufferForSimulation(uint32_t frame_index) const
{
	return simCommandBuffer[frame_index];
}

void Cloth::Cleanup(VkDevice LDevice, VkCommandPool CommandPool)
{
	uint32_t N = frameBuffer.size();
	vkFreeCommandBuffers(LDevice, CommandPool, N, commandBuffer.data());
	vkFreeCommandBuffers(LDevice, CommandPool, MAXInFlightRendering, simCommandBuffer.data());
	vkDestroyPipeline(LDevice, ctRes.graphicPipeline, Allocator);
	vkDestroyPipeline(LDevice, ctCp.computePipeline, Allocator);
	if (usePBD) {
		vkDestroyPipeline(LDevice, ctCp.computePipelineEX1, Allocator);
		vkDestroyPipeline(LDevice, ctCp.computePipelineEX2, Allocator);
		vkDestroyPipeline(LDevice, ctCp.computePipelineEX3, Allocator);
	}
	vkDestroyPipelineLayout(LDevice, ctRes.pipelineLayout, Allocator);
	vkDestroyPipelineLayout(LDevice, ctCp.pipelineLayout, Allocator);
	vkDestroyRenderPass(LDevice, renderPass, Allocator);
	vkDestroyDescriptorSetLayout(LDevice, ctRes.descriptorSetLayout, Allocator);
	vkDestroyDescriptorSetLayout(LDevice, ctCp.descriptorSetLayout, Allocator);
	for (uint32_t i = 0; i < N; i++) {
		vkDestroyFramebuffer(LDevice, frameBuffer[i], Allocator);
	}
	vkDestroyImageView(LDevice, colorImageView, Allocator);
	vkDestroyImage(LDevice, colorImage, Allocator);
	vkFreeMemory(LDevice, colorImageMemory, Allocator);

	vkDestroyImageView(LDevice, depthImageView, Allocator);
	vkDestroyImage(LDevice, depthImage, Allocator);
	vkFreeMemory(LDevice, depthImageMemory, Allocator);

	vkDestroySampler(LDevice, ctRes.textureSampler, Allocator);
	vkDestroyImageView(LDevice, ctRes.textureImageView, Allocator);
	vkDestroyImage(LDevice, ctRes.textureImage, Allocator);
	vkFreeMemory(LDevice, ctRes.textureImageMemory, Allocator);

	for (uint32_t i = 0; i < clothPointBuffer.size(); i++) {
		Buffer::CleanupBuffer(LDevice, clothPointBuffer[i], clothPointBufferMemory[i], false);
	}
	Buffer::CleanupBuffer(LDevice, uniformRenderingBuffer, uniformRenderingBufferMemory, true);
	Buffer::CleanupBuffer(LDevice, uniformSimulatingBuffer, uniformSimulatingBufferMemory, true);
	Buffer::CleanupBuffer(LDevice, uniformEnvBuffer, uniformEnvBufferMemory, true);
	Buffer::CleanupBuffer(LDevice, ctRes.vertexBuffer, ctRes.vertexBufferMemory, false);
	Buffer::CleanupBuffer(LDevice, ctRes.indexBuffer, ctRes.indexBufferMemory, false);
}