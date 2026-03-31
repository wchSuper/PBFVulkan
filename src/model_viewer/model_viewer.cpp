#include "model_viewer/model_viewer.h"

#define Allocator nullptr

ModelViewer::~ModelViewer()
{

}

TickWindowResult ModelViewer::TickWindow(float deltaTime)
{
	if (glfwWindowShouldClose(window)) return TickWindowResult::EXIT;
	glfwPollEvents();
	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	if (w * h == 0) {
		return TickWindowResult::HIDE;
	}
	return TickWindowResult::NONE;
}

void ModelViewer::WindowResizeCallback(GLFWwindow* window, int width, int height)
{
	auto modelViewer = reinterpret_cast<ModelViewer*>(glfwGetWindowUserPointer(window));
	modelViewer->bFramebufferResized = true;
}

void ModelViewer::Init()
{
	glfwInit();
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(width_, height_, "model viewer", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);

	CreateBasicDevices();

}

void ModelViewer::CreateBasicDevices()
{
	Base::CreateInstance(bEnableValidation, &instance);

	Base::CreateDebugMessenger(instance, messenger);

	if (glfwCreateWindowSurface(instance, window, Allocator, &surface) != VK_SUCCESS) {
		throw std::runtime_error("faile to create surface!");
	}
	Base::PickPhysicalDevice(instance, pDevice, surface);

	Base::CreateLogicalDevice(pDevice, surface, graphicQueue, presentQueue, lDevice);

	// command pool and descriptor pool 
	Base::CreateCommandPool(pDevice, surface, lDevice, commandPool);
	Base::CreateDescriptorPool(lDevice, descriptorPool);

	// swapchain
	Base::CreateSwapChain(pDevice, surface, lDevice, graphicQueue, presentQueue, swapChain, swapChainImageFormat, swapChainImageExtent,
		swapChainImages, swapChainImageViews);

	// semaphores and fences
	CreateSupportObjects();
}

void ModelViewer::CreateSupportObjects()
{
	drawingFences.resize(MAX_IN_FLIGHT_FRAMES);
	imageAvaliableSems.resize(MAX_IN_FLIGHT_FRAMES);
	renderingFinishSems.resize(MAX_IN_FLIGHT_FRAMES);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.flags = VK_SEMAPHORE_TYPE_BINARY;
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < MAX_IN_FLIGHT_FRAMES; i++) {
		if (vkCreateSemaphore(lDevice, &semaphoreInfo, Allocator, &imageAvaliableSems[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphic semaphores!");
		}
		if (vkCreateFence(lDevice, &fenceInfo, Allocator, &drawingFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create drawing fences!");
		}
	}
}

void ModelViewer::CleanupSupportObjects()
{
	for (uint32_t i = 0; i < MAX_IN_FLIGHT_FRAMES; i++) {
		vkDestroySemaphore(lDevice, imageAvaliableSems[i], Allocator);
		vkDestroyFence(lDevice, drawingFences[i], Allocator);
	}
}

void ModelViewer::CreateUnifiedObjModel()
{
	
}