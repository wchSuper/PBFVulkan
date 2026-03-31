#pragma once
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include <glm/gtc/quaternion.hpp>  
#include <glm/gtc/matrix_transform.hpp> 
#include <vulkan/vulkan.h>

#include "tiny_obj_loader.h"
#include "renderer_types.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "base.hpp"
#include "camera.hpp"
#include "deargui.hpp"
#include "buffer.hpp"
#include "utils.hpp"
#include "image.hpp"
#include "scene.hpp"
#include "sceneset.h"

 
#define MAX_IN_FLIGHT_FRAMES 2

enum class ModelType {
    IMAGE_2D,
    IMAGE_3D,
    GLTFVIEWER,
    OBJVIEWER,
    FLUID
};

struct ModelParam {
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

    // textures
    std::vector<VkImage> textureImages;
    std::vector<VkDeviceMemory> textureImageMemories;
    std::vector<VkImageView> textureImageViews;
    std::vector<VkSampler> textureSamplers;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer voxelBuffer;
    VkDeviceMemory voxelBufferMemory;

    UniformVoxelObject voxinfobj{};
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* mappedBuffer;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
};

struct Model {
    ModelParam model1;
    ModelParam model2;
};

class ModelViewer {

public:
	ModelViewer(uint32_t width, uint32_t height) : width_(width), height_(height) {}
	~ModelViewer();

	TickWindowResult TickWindow(float deltaTime);
	void Init();
	void Cleanup();
	void BaseCleanup();

    // resources
    void CreateBasicDevices();
    void CreateSupportObjects();
    void CleanupSupportObjects();

    void CreateUnifiedObjModel();


    // call back functions
    static void WindowResizeCallback(GLFWwindow* window, int width, int height);



private:
	uint32_t width_;
	uint32_t height_;

    // basic resources
    GLFWwindow* window = nullptr;
    VkInstance instance;
    VkDebugUtilsMessengerEXT messenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice pDevice;
    VkDevice lDevice;
    VkQueue graphicQueue;
    VkQueue presentQueue;

    VkCommandPool commandPool;
    VkSwapchainKHR swapChain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainImageExtent;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    VkDescriptorPool descriptorPool;

    // render resources
    VkRenderPass modelRenderPass;
    VkDescriptorSetLayout modelDescriptorSetLayout;
    VkDescriptorSet modelDescriptorSet;
    VkPipelineLayout modelPipelineLayout;
    VkPipeline modelPipeline;

    // buffer
    VkBuffer uniformRenderingBuffer;
    VkDeviceMemory uniformRenderingBufferMemory;
    void* mappedRenderingBuffer;
    
    // fences and semaphores
    std::vector<VkFence> drawingFences;
    std::vector<VkSemaphore> imageAvaliableSems;
    std::vector<VkSemaphore> renderingFinishSems;
    std::vector<VkSemaphore> simulatingFinishSems;
    
    // custom image




    // camera
    double lastX = 0.0, lastY = 0.0;
    float panSpeed = 0.005f;
    float zoomSpeed = 0.1f;
    float rotateSpeed = 0.1f;

    glm::vec3 mousePos = glm::vec3(0.0f, 0.0f, 0.0f);

    glm::vec3 cameraPos = glm::vec3(0.8f, 0.7f, 1.7f);
    glm::vec3 cameraTarget = glm::vec3(0.8f, 0.3f, 0.3f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 cameraPos_ = glm::vec3(0.8f, 0.7f, 1.7f);
    glm::vec3 cameraTarget_ = glm::vec3(0.8f, 0.3f, 0.3f);
    glm::vec3 cameraUp_ = glm::vec3(0.0f, 1.0f, 0.0f);


    // other params
    bool bFramebufferResized = false;
    bool bEnableValidation = true;
};


