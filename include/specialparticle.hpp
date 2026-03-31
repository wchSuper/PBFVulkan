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
#include "image.hpp"
#include "base.hpp"
#include "buffer.hpp"
#define Allocator nullptr


class SpecialParticle
{
private:
    uint32_t specialParticleCount;
    float specialParticleRadius = 0.008;
    struct SpecialParticleResources {
        std::vector<Particle> specialParticles;

        VkBuffer ParticleBuffer;
        VkDeviceMemory ParticleBufferMemory;

        //std::vector<VkBuffer> UniformRenderingBuffer;
        //std::vector<VkDeviceMemory> UniformRenderingBufferMemory;
        //std::vector<void*> MappedRenderingBuffer;

        //std::vector<VkBuffer> UniformSimulatingBuffer;
        //std::vector<VkDeviceMemory> UniformSimulatingBufferMemory;
        //std::vector<void*> MappedSimulatingBuffer;
        UniformRenderingObject renderingobj;
        VkBuffer UniformRenderingBuffer;
        VkDeviceMemory UniformRenderingBufferMemory;
        void* MappedRenderingBuffer;

        VkRenderPass renderPass;
        std::vector<VkFramebuffer> framebuffer;
        std::vector<VkCommandBuffer> commandbuffer;

        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicPipeline;

        // other resources ...
    };
    SpecialParticleResources spRes;

    VkImage ColorImage;
    VkImageView ColorImageView;
    VkDeviceMemory ColorImageMemory;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VkSemaphore SpecialParticleSemaphore;

public:

    void UpdateUniforms(const UniformRenderingObject& robj)
    {
        if (spRes.MappedRenderingBuffer) {
            memcpy(spRes.MappedRenderingBuffer, &robj, sizeof(UniformRenderingObject));
        }
    }

    void InitParticles(UniformRenderingObject* renderingobj)
    {
        // particle original position
        glm::vec3 startPos = glm::vec3(0.0, 1.0, 0.0);
        uint32_t num_x = 20, num_y = 20, num_z = 20;
        for (uint32_t ix = 0; ix < num_x; ix++) {
            for (uint32_t iy = 0; iy < num_y; iy++) {
                for (uint32_t iz = 0; iz < num_z; iz++) {
                    Particle particle{};
                    particle.Location = glm::vec3(
                        (ix + 1) * 2 * specialParticleRadius,
                        (iy + 1) * 2 * specialParticleRadius,
                        (iz + 1) * 2 * specialParticleRadius) + startPos;
                    particle.Velocity = glm::vec3(0.0, 0.0, 0.0);
                    spRes.specialParticles.push_back(particle);
                }
            }
        }

        // initialize Uniform Object
        spRes.renderingobj = *renderingobj;
        spRes.renderingobj.particleRadius = 0.02f;
    }

    void CreateSupportObject(VkDevice LDevice)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.flags = VK_SEMAPHORE_TYPE_BINARY;

        if (vkCreateSemaphore(LDevice, &semaphoreInfo, Allocator, &SpecialParticleSemaphore) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics objects!");
        }
    }

    // Create spRes.renderPass 
    void CreateRenderPass(VkDevice LDevice, VkFormat SwapChainImageFormat)
    {
        VkAttachmentDescription colorattachement{};
        colorattachement.format = SwapChainImageFormat;
        colorattachement.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorattachement.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorattachement.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorattachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorattachement.samples = VK_SAMPLE_COUNT_1_BIT;
        colorattachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorattachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentDescription, 2> attachments = { colorattachement, depthAttachment };

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

        if (vkCreateRenderPass(LDevice, &createinfo, Allocator, &spRes.renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphic renderpass!");
        }
    }

    void CreateImages(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, VkExtent2D SwapChainExtent)
    {
        VkExtent3D extent = { SwapChainExtent.width, SwapChainExtent.height, 1 };
        Image::CreateImage(PDevice, LDevice, ColorImage, ColorImageMemory, extent, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        ColorImageView = Image::CreateImageView(LDevice, ColorImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
        Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue,
            ColorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // directly output to swapchainimages
    void CreateFrameBuffers(GLFWwindow* Window, VkDevice LDevice, std::vector<VkImageView>& SwapChainImageViews, VkExtent2D SwapChainExtent)
    {
        spRes.framebuffer.resize(SwapChainImageViews.size());
        for (size_t i = 0; i < SwapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = { SwapChainImageViews[i], depthImageView };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = spRes.renderPass;
            framebufferInfo.attachmentCount = 2;
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = SwapChainExtent.width;
            framebufferInfo.height = SwapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(LDevice, &framebufferInfo, nullptr, &spRes.framebuffer[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    // create graphic buffer (GPU | Uniform)
    void CreateGPUBuffer(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue)
    {
        // 1. particles      2. ... ? 
        {
            int particleNums = spRes.specialParticles.size();
            VkDeviceSize size = sizeof(Particle) * particleNums;
            VkBuffer stagingbuffer;
            VkDeviceMemory stagingmemory;

            Buffer::CreateBuffer(PDevice, LDevice, stagingbuffer, stagingmemory, size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            Buffer::CreateBuffer(PDevice, LDevice, spRes.ParticleBuffer, spRes.ParticleBufferMemory, size,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            void* data;
            vkMapMemory(LDevice, stagingmemory, 0, size, 0, &data);
            memcpy(data, spRes.specialParticles.data(), size);

            auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
            VkBufferCopy region{};
            region.size = size;
            region.dstOffset = region.srcOffset = 0;
            vkCmdCopyBuffer(cb, stagingbuffer, spRes.ParticleBuffer, 1, &region);
            VkSubmitInfo submitinfo{};
            Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

            vkDeviceWaitIdle(LDevice);
            Buffer::CleanupBuffer(LDevice, stagingbuffer, stagingmemory, true);
        }
        {
            // .... other GPU buffers ...
        }
    }

    void CreateUniformBuffer(VkPhysicalDevice PDevice, VkDevice LDevice)
    {
        // 1. rendering
        {
            VkDeviceSize size = sizeof(UniformRenderingObject);
            Buffer::CreateBuffer(PDevice, LDevice, spRes.UniformRenderingBuffer, spRes.UniformRenderingBufferMemory, size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkMapMemory(LDevice, spRes.UniformRenderingBufferMemory, 0, size, 0, &spRes.MappedRenderingBuffer);
            memcpy(spRes.MappedRenderingBuffer, &spRes.renderingobj, size);
        }
        // 2. simulating ... waiting ...
        {

        }
    }

    void CreateDescriptorSetLayout(VkDevice LDevice)
    {
        std::vector<BindingDesc> descs = {
            { 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
        };
        Base::descriptorSetLayoutBinding(LDevice, descs, spRes.descriptorSetLayout);
    }

    void CreateDescriptorSet(VkDevice LDevice, VkDescriptorPool& DescriptorPool)
    {
        Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, spRes.descriptorSetLayout, spRes.descriptorSet);
        std::vector<VkWriteDescriptorSet> writes(1);

        VkDescriptorBufferInfo renderingbufferinfo{};
        renderingbufferinfo.buffer = spRes.UniformRenderingBuffer;
        renderingbufferinfo.offset = 0;
        renderingbufferinfo.range = sizeof(UniformRenderingObject);

        std::vector<WriteDesc> writeInfo = {
            {renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}
        };
        Base::DescriptorWrite(LDevice, writeInfo, writes, spRes.descriptorSet);
    }

    void CreateGraphicPipelineLayout(VkDevice LDevice)
    {
        Base::CreateGraphicPipelineLayout(LDevice, spRes.descriptorSetLayout, spRes.pipelineLayout);
    }

    void CreateGraphicPipeline(VkDevice LDevice)
    {
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = { colorBlendAttachment };
        Base::CreateGraphicPipeline(
            LDevice, spRes.pipelineLayout, spRes.renderPass, &spRes.graphicPipeline,
            "resources/shaders/spv/special/specialparticle1.vert.spv",
            "resources/shaders/spv/special/specialparticle1.frag.spv",
            true, 0,
            VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
            VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
            VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, colorBlendAttachments
        );
    }

    void RecordGraphicCommandBuffer(VkDevice LDevice, VkCommandPool commandPool, VkExtent2D SwapChainImageExtent)
    {
        spRes.commandbuffer.resize(spRes.framebuffer.size());
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)spRes.commandbuffer.size();

        if (vkAllocateCommandBuffers(LDevice, &allocInfo, spRes.commandbuffer.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }

        for (size_t i = 0; i < spRes.commandbuffer.size(); i++) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(spRes.commandbuffer[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = spRes.renderPass;
            renderPassInfo.framebuffer = spRes.framebuffer[i];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = SwapChainImageExtent;
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
            clearValues[1].depthStencil = { 1.0f, 0 };
            renderPassInfo.clearValueCount = 2;
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(spRes.commandbuffer[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindDescriptorSets(spRes.commandbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, spRes.pipelineLayout, 0, 1, &spRes.descriptorSet, 0, nullptr);
            vkCmdBindPipeline(spRes.commandbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, spRes.graphicPipeline);

            VkViewport viewport;
            viewport.height = SwapChainImageExtent.height;
            viewport.width = SwapChainImageExtent.width;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            viewport.x = viewport.y = 0;
            VkRect2D scissor;
            scissor.offset = { 0,0 };
            scissor.extent = SwapChainImageExtent;
            vkCmdSetViewport(spRes.commandbuffer[i], 0, 1, &viewport);
            vkCmdSetScissor(spRes.commandbuffer[i], 0, 1, &scissor);
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(spRes.commandbuffer[i], 0, 1, &spRes.ParticleBuffer, &offset);
            vkCmdDraw(spRes.commandbuffer[i], spRes.specialParticles.size(), 1, 0, 0);

            vkCmdEndRenderPass(spRes.commandbuffer[i]);

            if (vkEndCommandBuffer(spRes.commandbuffer[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }

    VkCommandBuffer getCommandBufferForImage(uint32_t image_index) const {
        if (image_index < spRes.commandbuffer.size()) {
            return spRes.commandbuffer[image_index];
        }
        return VK_NULL_HANDLE;
    }

    // parameter in: PreSemaphore   parameter out: SignalSemaphore
    void SpecialParticleRender(uint32_t dstimage, VkQueue GraphicNComputeQueue, VkSemaphore* PreSemaphore, VkSemaphore* SignalSemaphore)
    {
        std::array<VkPipelineStageFlags, 1> waitstages = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
        VkSubmitInfo rendering_submitinfo{};
        rendering_submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        rendering_submitinfo.commandBufferCount = 2;
        rendering_submitinfo.pCommandBuffers = &spRes.commandbuffer[dstimage];

        rendering_submitinfo.waitSemaphoreCount = 1;
        rendering_submitinfo.pWaitSemaphores = PreSemaphore;
        rendering_submitinfo.pWaitDstStageMask = waitstages.data();
        rendering_submitinfo.pSignalSemaphores = &SpecialParticleSemaphore;
        rendering_submitinfo.signalSemaphoreCount = 1;

        if (vkQueueSubmit(GraphicNComputeQueue, 1, &rendering_submitinfo, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit rendering command buffer!");
        }
        SignalSemaphore = &SpecialParticleSemaphore;
    }

    void CreateDepthResources(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, VkExtent2D SwapChainExtent)
    {
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        VkExtent3D extent = { SwapChainExtent.width, SwapChainExtent.height, 1 };

        Image::CreateImage(PDevice, LDevice, depthImage, depthImageMemory, extent, depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT);
        depthImageView = Image::CreateImageView(LDevice, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue,
            depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void PreCreateResources(VkPhysicalDevice PDevice, VkDevice LDevice, VkQueue GraphicNComputeQueue, VkFormat SwapChainImageFormat,
        VkExtent2D SwapChainImageExtent, VkCommandPool& CommandPool, VkDescriptorPool& DescriptorPool, UniformRenderingObject* renderingobj, GLFWwindow* Window,
        std::vector<VkImageView> SwapChainImageViews)
    {
        // 1. spRes.specialParticles; spRes.renderingobj
        InitParticles(renderingobj);

        // 2. ColorImage
        CreateImages(PDevice, LDevice, CommandPool, GraphicNComputeQueue, SwapChainImageExtent);

        CreateDepthResources(PDevice, LDevice, CommandPool, GraphicNComputeQueue, SwapChainImageExtent);

        // 3. spRes.particleBuffer         (GPU Buffer)
        CreateGPUBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue);

        // SpecialParticleSemaphore
        CreateSupportObject(LDevice);

        // 4. spRes.UniformRenderingBuffer (Uniform Buffer)  
        CreateUniformBuffer(PDevice, LDevice);

        // 5. spRes.renderPass
        CreateRenderPass(LDevice, SwapChainImageFormat);

        // 6. spRes.framebuffer (size = swapchainimage.size())
        CreateFrameBuffers(Window, LDevice, SwapChainImageViews, SwapChainImageExtent);

        // 7. spRes.descriptorSetLayout
        CreateDescriptorSetLayout(LDevice);

        // 8. spRes.descriptorSet
        CreateDescriptorSet(LDevice, DescriptorPool);

        // 9. spRes.graphicpipelineLayout
        CreateGraphicPipelineLayout(LDevice);

        // 10. spRes.graphicpipeline
        CreateGraphicPipeline(LDevice);

        // 11. spRes.commandBuffer
        RecordGraphicCommandBuffer(LDevice, CommandPool, SwapChainImageExtent);
    }

    ~SpecialParticle()
    {

    }

    inline void ReCreateResources(VkDevice LDevice, VkDescriptorPool &DescriptorPool)
    {
        // delete framebuffer + images(imageView imageSampler) and then recreate them
        /*vkDestroyFramebuffer(LDevice, spRes.framebuffer, Allocator);

        vkDestroyImageView(LDevice, depthImageView, Allocator);
        vkDestroyImage(LDevice, depthImage, Allocator);
        vkFreeMemory(LDevice, depthImageMemory, Allocator);

        vkDestroyImageView(LDevice, ColorImageView, Allocator);
        vkDestroyImage(LDevice, ColorImage, Allocator);
        vkFreeMemory(LDevice, ColorImageMemory, Allocator);

        CreateImages()
        CreateDescriptorSet(LDevice, DescriptorPool);*/
    }

    inline void Cleanup(VkDevice LDevice, VkCommandPool CommandPool)
    {
        uint32_t N = spRes.framebuffer.size();
        vkFreeCommandBuffers(LDevice, CommandPool, N, spRes.commandbuffer.data());
        vkDestroyPipeline(LDevice, spRes.graphicPipeline, Allocator);
        vkDestroyPipelineLayout(LDevice, spRes.pipelineLayout, Allocator);
        vkDestroyRenderPass(LDevice, spRes.renderPass, Allocator);
        vkDestroyDescriptorSetLayout(LDevice, spRes.descriptorSetLayout, Allocator);
        for (uint32_t i = 0; i < N; i++) {
            vkDestroyFramebuffer(LDevice, spRes.framebuffer[i], Allocator);
        }
        vkDestroyImageView(LDevice, ColorImageView, Allocator);
        vkDestroyImage(LDevice, ColorImage, Allocator);
        vkFreeMemory(LDevice, ColorImageMemory, Allocator);

        vkDestroyImageView(LDevice, depthImageView, Allocator);
        vkDestroyImage(LDevice, depthImage, Allocator);
        vkFreeMemory(LDevice, depthImageMemory, Allocator);

        Buffer::CleanupBuffer(LDevice, spRes.ParticleBuffer, spRes.ParticleBufferMemory, false);
        Buffer::CleanupBuffer(LDevice, spRes.UniformRenderingBuffer, spRes.UniformRenderingBufferMemory, true);

        vkDestroySemaphore(LDevice, SpecialParticleSemaphore, Allocator);
    }
};