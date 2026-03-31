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


struct UniformSimData {
    alignas(4) float deltaT{ 0.0f };
    alignas(4) float particleMass{ 0.1f };
    alignas(4) float springStiffness{ 2000.0f };
    alignas(4) float damping{ 0.25f };
    // mass-spring system (force)
    alignas(4) float restDistH{ 0 };
    alignas(4) float restDistV{ 0 };
    alignas(4) float restDistD{ 0 };
    alignas(4) float sphereRadius{ 0.25f };

    // pbd system (position)
    alignas(4) uint32_t usePBD;          // is usePBD ? 
    alignas(4) float restDistStretch;
    alignas(4) float restDistShear;
    alignas(4) float restDistBend;

    alignas(16) glm::vec4 spherePos{ 0.5f, 0.0f, 0.5f, 0.0f };
    alignas(16) glm::vec4 gravity{ 0.0f, -9.8f, 0.0f, 0.0f };
    alignas(16) glm::ivec2 particleCount{ 0 };
};

struct UniformEnvData {
    alignas(16) glm::vec4 lightPos;
    alignas(16) glm::vec4 lightDir;
};

class Cloth {

private:
	glm::uvec2 gridSize = { 50, 50 };
	glm::vec2 size = { 1.0f, 1.0f };

	struct ClothResource {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        std::vector<ClothPoint> vertices;
        std::vector<uint32_t> indices;

        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;

        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;

        VkImage textureImage;
        VkDeviceMemory textureImageMemory;
        VkImageView textureImageView;
        VkSampler textureSampler;

        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicPipeline;
	};
    UniformRenderingObject renderingObj;
    VkBuffer uniformRenderingBuffer;
    VkDeviceMemory uniformRenderingBufferMemory;
    void* mappedRenderingBuffer;

    UniformEnvData envObj;
    VkBuffer uniformEnvBuffer;
    VkDeviceMemory uniformEnvBufferMemory;
    void* mappedEnvBuffer;

    VkImage colorImage;
    VkImageView colorImageView;
    VkDeviceMemory colorImageMemory;

    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> frameBuffer;               // size :  swapChainImageView.size
    std::vector<VkCommandBuffer> commandBuffer;           // size :  framebuffer.size
    std::vector<VkCommandBuffer> simCommandBuffer;        // size :  MAXInFlightRendering

    struct ClothCompute {
        VkDescriptorSetLayout descriptorSetLayout;
        // VkDescriptorSet descriptorSet;
        std::vector<VkDescriptorSet> descriptorSet;
        VkPipelineLayout pipelineLayout;
        VkPipeline computePipeline;

        VkPipeline computePipelineEX1;
        VkPipeline computePipelineEX2;
        VkPipeline computePipelineEX3;

        UniformSimData uniformData;
    };

    UniformSimData simulatingObj;
    VkBuffer uniformSimulatingBuffer;
    VkDeviceMemory uniformSimulatingBufferMemory;
    void* mappedSimulatingBuffer;

    ClothResource ctRes;
    ClothCompute  ctCp;

    std::vector<ClothPoint> clothPoints;
    std::vector<VkBuffer> clothPointBuffer;
    std::vector<VkDeviceMemory> clothPointBufferMemory;

    uint32_t MAXInFlightRendering = 2;
    uint32_t ONE_GROUP_INVOCATION_COUNT_X = 10;
    uint32_t ONE_GROUP_INVOCATION_COUNT_Y = 10;

    uint32_t WORK_GROUP_COUNT_X;
    uint32_t WORK_GROUP_COUNT_Y;

    bool usePBD; 

public:
    // images (texture colorImage depthImage)  uniform buffer
    void CreateResources(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, VkExtent2D SwapChainExtent);

    void CreateClothMeshAndRes(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue);

    void CreateDescriptorSetLayout(VkDevice LDevice);

    void CreateDescriptorSet(VkDevice LDevice, VkDescriptorPool DescriptorPool);

    // colorAttachment and depthAttachment
    void CreateRenderPass(VkDevice LDevice, VkFormat SwapChainImageFormat);

    void CreateFrameBuffer(VkDevice LDevice, std::vector<VkImageView>& SwapChainImageViews, VkExtent2D SwapChainExtent);

    // contain pipelineLayout and pipeline creating
    void CreateGraphicPipeline(VkDevice LDevice);

    void CreateComputePipeline(VkDevice LDevice);

    void RecordGraphicCommandBuffer(VkDevice LDevice, VkCommandPool commandPool, VkExtent2D SwapChainImageExtent);

    void RecordSimulatingCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool);

    void UpdateUniforms(const UniformRenderingObject& robj);

    void Init(
        VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue, VkExtent2D SwapChainExtent,
        VkFormat SwapChainImageFormat, std::vector<VkImageView>& SwapChainImageViews, VkDescriptorPool DescriptorPool, UniformRenderingObject* renderingobj,
        GLFWwindow* Window
    );

    VkCommandBuffer getCommandBufferForImage(uint32_t image_index) const;

    VkCommandBuffer getCommandBufferForSimulation(uint32_t frame_index) const;

    void Cleanup(VkDevice LDevice, VkCommandPool CommandPool);
};