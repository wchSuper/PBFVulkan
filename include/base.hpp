#pragma once
#include <vector>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "utils.hpp"
#include "extensionfuncs.h"
#include <iostream>
#include <algorithm>
#include <variant>

#define Allocator nullptr

struct BindingDesc {
	uint32_t bindingId;
	uint32_t disciptorCount;
	VkDescriptorType descriptorType;
	VkShaderStageFlags stageFlags;
};
using DescriptorBufferImageInfo = std::variant<VkDescriptorBufferInfo, VkDescriptorImageInfo>;

struct WriteDesc {
	DescriptorBufferImageInfo info;
	VkDescriptorType descriptorType;
	uint32_t bindingId;
};


static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	std::cerr << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}
namespace Base
{
	// ******************************** instance ********************************************************** //
	inline void GetRequestInstaceExts(std::vector<const char*>& exts, bool bEnableValidation)
	{
		const char** glfwexts;
		uint32_t glfwext_count;
		glfwexts = glfwGetRequiredInstanceExtensions(&glfwext_count);
		exts.assign(glfwexts, glfwexts + glfwext_count);
		if (bEnableValidation) {
			exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
	}
	inline void GetRequestInstanceLayers(std::vector<const char*>& layers, bool bEnableValidation)
	{
		layers.resize(0);
		if (bEnableValidation) {
			layers.push_back("VK_LAYER_KHRONOS_validation");
		}
	}
	inline void MakeMessengerInfo(VkDebugUtilsMessengerCreateInfoEXT& createinfo)
	{
		createinfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createinfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		createinfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		createinfo.pUserData = nullptr;
		createinfo.pfnUserCallback = DebugCallback;
	}

	inline void CreateInstance(bool bEnableValidation, VkInstance *Instance)
	{
		VkApplicationInfo appinfo{};
		appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appinfo.pApplicationName = "Fluids Renderer";
		appinfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);

		VkInstanceCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		std::vector<const char*> exts;
		std::vector<const char*> layers;
		GetRequestInstaceExts(exts, bEnableValidation);
		GetRequestInstanceLayers(layers, bEnableValidation);
		createinfo.enabledExtensionCount = static_cast<uint32_t>(exts.size());
		createinfo.ppEnabledExtensionNames = exts.data();
		createinfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
		createinfo.ppEnabledLayerNames = layers.data();
		createinfo.pApplicationInfo = &appinfo;
		VkDebugUtilsMessengerCreateInfoEXT messengerinfo{};
		MakeMessengerInfo(messengerinfo);
		createinfo.pNext = &messengerinfo;
		if (vkCreateInstance(&createinfo, Allocator, Instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	inline void CreateDebugMessenger(VkInstance Instance, VkDebugUtilsMessengerEXT &Messenger)
	{
		VkDebugUtilsMessengerCreateInfoEXT createinfo{};
		MakeMessengerInfo(createinfo);
		if (ExtensionFuncs::vkCreateDebugUtilsMessengerEXT(Instance, &createinfo, Allocator, &Messenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to create debug messenger!");
		}
	}

	// *************************************************************************************************************//

	inline QueuefamliyIndices GetPhysicalDeviceQueueFamilyIndices(VkPhysicalDevice pdevice, VkSurfaceKHR surface)
	{
		QueuefamliyIndices indices;
		uint32_t queuefamily_count;
		vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &queuefamily_count, nullptr);
		std::vector<VkQueueFamilyProperties> queuefamilies(queuefamily_count);
		vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &queuefamily_count, queuefamilies.data());
		for (uint32_t i = 0; i < queuefamilies.size(); ++i) {
			auto& qf = queuefamilies[i];
			if ((qf.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (qf.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
				if (!indices.graphicNcompute.has_value()) {
					indices.graphicNcompute = i;
				}
			}
			if (!indices.present.has_value()) {
				VkBool32 support = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, surface, &support);
				if (support) {
					indices.present = i;
				}
			}
			if (indices.IsCompleted()) {
				break;
			}
		}
		return indices;
	}

	inline void GetRequestDeviceExts(std::vector<const char*>& exts)
	{
		exts.resize(0);
		exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		exts.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);
		exts.push_back(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
		exts.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
		exts.push_back("VK_KHR_dynamic_rendering");
		exts.push_back("VK_KHR_depth_stencil_resolve");
		exts.push_back("VK_KHR_create_renderpass2");
		exts.push_back("VK_EXT_shader_atomic_float");
	}

	inline void GetRequestDeviceFeature(VkPhysicalDeviceFeatures& features)
	{
		features = VkPhysicalDeviceFeatures{};
		features.samplerAnisotropy = VK_TRUE;
		features.fillModeNonSolid = VK_TRUE;
		features.independentBlend = VK_TRUE;
		features.fragmentStoresAndAtomics = VK_TRUE;
	}

	inline bool IsPhysicalDeviceSuitable(VkPhysicalDevice pdevice, VkSurfaceKHR Surface)
	{
		auto indices = Base::GetPhysicalDeviceQueueFamilyIndices(pdevice, Surface);
		if (!indices.IsCompleted()) return false;
		std::vector<const char*> exts;
		GetRequestDeviceExts(exts);
		uint32_t avext_count;
		vkEnumerateDeviceExtensionProperties(pdevice, "", &avext_count, nullptr);
		std::vector<VkExtensionProperties> avexts(avext_count);
		vkEnumerateDeviceExtensionProperties(pdevice, "", &avext_count, avexts.data());
		for (auto ext : exts) {
			bool support = false;
			for (auto avext : avexts) {
				if (strcmp(avext.extensionName, ext) == 0) {
					support = true;
					break;
				}
			}
			if (!support) return false;
		}
		uint32_t surfaceformat_count;
		uint32_t surfacepresentmode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, Surface, &surfacepresentmode_count, nullptr);
		if (surfacepresentmode_count == 0) return false;
		vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, Surface, &surfaceformat_count, nullptr);
		if (surfaceformat_count == 0) return false;

		VkPhysicalDeviceFeatures features{};
		vkGetPhysicalDeviceFeatures(pdevice, &features);
		if (features.samplerAnisotropy != VK_TRUE) return false;
		if (features.fillModeNonSolid != VK_TRUE) return false;
		return true;
	}

	inline void PickPhysicalDevice(VkInstance &Instance, VkPhysicalDevice &PDevice, VkSurfaceKHR Surface)
	{
		uint32_t pdevice_count;
		vkEnumeratePhysicalDevices(Instance, &pdevice_count, nullptr);
		std::vector<VkPhysicalDevice> pdeives(pdevice_count);
		vkEnumeratePhysicalDevices(Instance, &pdevice_count, pdeives.data());
		for (auto& pdevice : pdeives) {
			if (IsPhysicalDeviceSuitable(pdevice, Surface)) {
				PDevice = pdevice;
				return;
			}
		}
		throw std::runtime_error("failed to find a suitable physical device!");
	}

	inline void CreateLogicalDevice(
		VkPhysicalDevice PDevice, VkSurfaceKHR Surface, VkQueue &GraphicNComputeQueue, VkQueue &PresentQueue, VkDevice &LDevice
	)
	{
		auto queueindices = GetPhysicalDeviceQueueFamilyIndices(PDevice, Surface);
		std::array<uint32_t, 2> indices = { queueindices.graphicNcompute.value(),queueindices.present.value() };
		sort(indices.begin(), indices.end());
		std::vector<VkDeviceQueueCreateInfo> queueinfos;
		float priority = 1.0f;
		for (int i = 0; i < indices.size(); ++i) {
			if (i > 0 && indices[i] == indices[i - 1]) continue;
			VkDeviceQueueCreateInfo queueinfo{};
			queueinfo = VkDeviceQueueCreateInfo{};
			queueinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueinfo.pQueuePriorities = &priority;
			queueinfo.queueCount = 1;
			queueinfo.queueFamilyIndex = indices[i];
			queueinfos.push_back(queueinfo);
		}

		VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures{};
		atomicFloatFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
		atomicFloatFeatures.shaderBufferFloat32AtomicAdd = VK_TRUE; 
		atomicFloatFeatures.pNext = nullptr;

		VkDeviceCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		std::vector<const char*> exts;
		GetRequestDeviceExts(exts);
		createinfo.enabledExtensionCount = static_cast<uint32_t>(exts.size());
		createinfo.ppEnabledExtensionNames = exts.data();

		createinfo.queueCreateInfoCount = static_cast<uint32_t>(queueinfos.size());
		createinfo.pQueueCreateInfos = queueinfos.data();

		VkPhysicalDeviceFeatures features;
		GetRequestDeviceFeature(features);
		 
		createinfo.pEnabledFeatures = &features;
		createinfo.pNext = &atomicFloatFeatures;


		if (vkCreateDevice(PDevice, &createinfo, Allocator, &LDevice) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}
		vkGetDeviceQueue(LDevice, queueindices.graphicNcompute.value(), 0, &GraphicNComputeQueue);
		vkGetDeviceQueue(LDevice, queueindices.present.value(), 0, &PresentQueue);
	}

    inline void CreateSupportObjects(VkDevice LDevice, std::vector<VkSemaphore *> semaphores, std::vector<VkFence *> fences) 
	{
		VkSemaphoreCreateInfo seminfo{};
		seminfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		seminfo.flags = VK_SEMAPHORE_TYPE_BINARY;

		for (int i = 0; i < semaphores.size(); i++) {
			if (vkCreateSemaphore(LDevice, &seminfo, Allocator, semaphores[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create semaphore");
			}
		}
		
		VkFenceCreateInfo fenceinfo{};
		fenceinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceinfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < fences.size(); i++) {
			if (vkCreateFence(LDevice, &fenceinfo, Allocator, fences[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create fence");
			}
		}
	}

	inline void CreateCommandPool(VkPhysicalDevice PDevice, VkSurfaceKHR Surface, VkDevice LDevice, VkCommandPool &CommandPool)
	{
		auto queueindices = GetPhysicalDeviceQueueFamilyIndices(PDevice, Surface);
		VkCommandPoolCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createinfo.queueFamilyIndex = queueindices.graphicNcompute.value();
		createinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		if (vkCreateCommandPool(LDevice, &createinfo, Allocator, &CommandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	inline SurfaceDetails GetSurfaceDetails(VkPhysicalDevice PDevice, VkSurfaceKHR Surface)
	{
		SurfaceDetails details{};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PDevice, Surface, &details.capabilities);
		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(PDevice, Surface, &format_count, nullptr);
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(PDevice, Surface, &format_count, details.formats.data());
		uint32_t presentmode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(PDevice, Surface, &presentmode_count, nullptr);
		details.presentmode.resize(presentmode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(PDevice, Surface, &presentmode_count, details.presentmode.data());
		return details;
	}

	inline VkSurfaceFormatKHR ChooseSwapChainImageFormat(VkPhysicalDevice PDevice, std::vector<VkSurfaceFormatKHR>& formats)
	{
		for (auto& format : formats) {
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(PDevice, format.format, &formatProperties);
			if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
				continue;
			}
			if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return format;
			}
		}
		return formats[0];
	}

	inline VkPresentModeKHR ChooseSwapChainImagePresentMode(std::vector<VkPresentModeKHR>& presentmodes)
	{
		for (auto& presentmode : presentmodes) {
			if (presentmode == VK_PRESENT_MODE_MAILBOX_KHR)
				return presentmode;
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	inline VkExtent2D ChooseSwapChainImageExtents(VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.height != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		VkExtent2D extent = capabilities.minImageExtent;
		extent.height = std::min(extent.height, capabilities.maxImageExtent.height);
		extent.width = std::min(extent.width, capabilities.maxImageExtent.width);
		return extent;
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

	inline void CreateSwapChain(
		VkPhysicalDevice PDevice, VkSurfaceKHR Surface, VkDevice LDevice, VkQueue& GraphicNComputeQueue, VkQueue& PresentQueue,
		VkSwapchainKHR &SwapChain, VkFormat &SwapChainImageFormat, VkExtent2D &SwapChainImageExtent, std::vector<VkImage> &SwapChainImages,
		std::vector<VkImageView> &SwapChainImageViews
	)
	{
		auto surfacedetails = GetSurfaceDetails(PDevice, Surface);
		auto format = ChooseSwapChainImageFormat(PDevice, surfacedetails.formats);
		auto presentmode = ChooseSwapChainImagePresentMode(surfacedetails.presentmode);
		auto extent = ChooseSwapChainImageExtents(surfacedetails.capabilities);
		VkSwapchainCreateInfoKHR createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createinfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		createinfo.surface = Surface;
		createinfo.clipped = VK_TRUE;
		createinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createinfo.imageArrayLayers = 1;
		createinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		createinfo.imageColorSpace = format.colorSpace;
		createinfo.imageExtent = extent;
		createinfo.imageFormat = format.format;
		std::vector<uint32_t> indices;
		if (GraphicNComputeQueue == PresentQueue) {
			createinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		else {
			auto queueindices = Base::GetPhysicalDeviceQueueFamilyIndices(PDevice, Surface);
			createinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createinfo.queueFamilyIndexCount = 2;
			indices.push_back(queueindices.graphicNcompute.value());
			indices.push_back(queueindices.present.value());
			createinfo.pQueueFamilyIndices = indices.data();
		}
		createinfo.minImageCount = surfacedetails.capabilities.minImageCount + 1;
		if (createinfo.minImageCount > surfacedetails.capabilities.maxImageCount) {
			createinfo.minImageCount = surfacedetails.capabilities.maxImageCount;
		}
		if (vkCreateSwapchainKHR(LDevice, &createinfo, Allocator, &SwapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swapchain!");
		}
		SwapChainImageFormat = format.format;
		SwapChainImageExtent = extent;
		uint32_t image_count;
		vkGetSwapchainImagesKHR(LDevice, SwapChain, &image_count, nullptr);
		SwapChainImages.resize(image_count);
		SwapChainImageViews.resize(image_count);
		vkGetSwapchainImagesKHR(LDevice, SwapChain, &image_count, SwapChainImages.data());
		for (uint32_t i = 0; i < image_count; ++i) {
			SwapChainImageViews[i] = CreateImageView(LDevice, SwapChainImages[i], SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		}
	}

	inline void descriptorSetLayoutBinding(
		VkDevice LDevice,
		std::vector<BindingDesc> bindingdescs,
		VkDescriptorSetLayout& descriptorsetlayout
	)
	{
		int desc_size = bindingdescs.size();
		std::vector<VkDescriptorSetLayoutBinding> bindings(desc_size);
		int cnt = 0;
		for (auto desc : bindingdescs) {
			bindings[cnt].binding = desc.bindingId;
			bindings[cnt].descriptorCount = desc.disciptorCount;
			bindings[cnt].descriptorType = desc.descriptorType;
			bindings[cnt].stageFlags = desc.stageFlags;
			cnt++;
		}

		VkDescriptorSetLayoutCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createinfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createinfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(LDevice, &createinfo, Allocator, &descriptorsetlayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	inline void CreateDescriptorPool(
		VkDevice LDevice,
		VkDescriptorPool& descriptorpool
	)
	{
		std::array<VkDescriptorPoolSize, 4> poolsizes{};
		poolsizes[0].descriptorCount = 1024;
		poolsizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolsizes[1].descriptorCount = 1024;
		poolsizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolsizes[2].descriptorCount = 1024;
		poolsizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolsizes[3].descriptorCount = 1024;
		poolsizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		VkDescriptorPoolCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createinfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; 
		createinfo.maxSets = 1024;
		createinfo.poolSizeCount = static_cast<uint32_t>(poolsizes.size());
		createinfo.pPoolSizes = poolsizes.data();
		if (vkCreateDescriptorPool(LDevice, &createinfo, Allocator, &descriptorpool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	inline void CreateGraphicDescriptorSet(
		VkDevice &LDevice, VkDescriptorPool &DescriptorPool, VkDescriptorSetLayout &DescriptorSetLayout,
		VkDescriptorSet &DescriptorSet
	)
	{
		VkDescriptorSetAllocateInfo allocateinfo{};
		allocateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateinfo.descriptorPool = DescriptorPool;
		allocateinfo.descriptorSetCount = 1;
		allocateinfo.pSetLayouts = &DescriptorSetLayout;
		if (vkAllocateDescriptorSets(LDevice, &allocateinfo, &DescriptorSet) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor set!");
		}
	}

	inline void DescriptorWrite(
		VkDevice LDevice, std::vector<WriteDesc> &writeInfo, std::vector<VkWriteDescriptorSet> &writes,
		VkDescriptorSet& DescriptorSet
	)
	{
		// wsize == BufferInfo size
		int wsize = writes.size();
		for (int i = 0; i < wsize; i++) {
			auto& shape = writeInfo[i].info;
			auto& type = writeInfo[i].descriptorType;
			auto& bindingId = writeInfo[i].bindingId;

			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = type;
			writes[i].dstArrayElement = 0;
			writes[i].dstBinding = bindingId;
			if (bindingId == 0) {
				writes[i].dstBinding = i;
			}
			writes[i].dstSet = DescriptorSet;

			if (std::holds_alternative<VkDescriptorBufferInfo>(shape)) {
				writes[i].pBufferInfo = &std::get<VkDescriptorBufferInfo>(shape);
			}
			else {
				writes[i].pImageInfo = &std::get<VkDescriptorImageInfo>(shape);
			}
		}
		vkUpdateDescriptorSets(LDevice, writes.size(), writes.data(), 0, nullptr);
	}

	inline void CreateGraphicPipelineLayout(VkDevice LDevice, VkDescriptorSetLayout &DescriptorSetLayout, VkPipelineLayout &PipelineLayout)
	{
		VkPipelineLayoutCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		createinfo.pSetLayouts = &DescriptorSetLayout;
		createinfo.setLayoutCount = 1;
		if (vkCreatePipelineLayout(LDevice, &createinfo, Allocator, &PipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphic pipeline layout!");
		}
	}

	inline void CreateSimulationPipelineLayout(VkDevice LDevice, VkDescriptorSetLayout& DescriptorSetLayout, VkPipelineLayout& PipelineLayout)
	{
	}

	// *************************************************  Graphic Pipeline ************************************************************//
	inline VkShaderModule MakeShaderModule(VkDevice LDevice, const char* filename)
	{
		std::vector<char> bytes;
		Util::ReadFile(filename, bytes);
		VkShaderModuleCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createinfo.codeSize = bytes.size();
		createinfo.pCode = reinterpret_cast<uint32_t*>(bytes.data());
		VkShaderModule module;
		if (vkCreateShaderModule(LDevice, &createinfo, Allocator, &module) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}
		return module;
	}
	
	inline std::vector<VkPipelineShaderStageCreateInfo> GetShaderStages(
		VkDevice LDevice,
		const char* VertSpvPath,
		const char* FragSpvPath
	)
	{
		auto vertshadermodule = MakeShaderModule(LDevice, VertSpvPath);
		auto fragshadermodule = MakeShaderModule(LDevice, FragSpvPath);
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
		return shaderstages;
	}

	inline void CreateGraphicPipeline(
		VkDevice LDevice, VkPipelineLayout &PipelineLayout, VkRenderPass &RenderPass, VkPipeline *Pipeline,
		const char* VertSpvPath, const char* FragSpvPath,
		bool InputBinding, int StructType,
		VkPrimitiveTopology AssemblyType,
		VkCullModeFlags RasterCullMode, VkFrontFace RasterFrontFace, VkPolygonMode RasterPolygonMode,
		VkBool32 DepthTestEnable, VkBool32 DepthWriteEnable, VkCompareOp CompareOp,
		std::vector<VkPipelineColorBlendAttachmentState> colorblendAttachments
	)
	{
		// shaderstages
		auto vertshadermodule = MakeShaderModule(LDevice, VertSpvPath);
		auto fragshadermodule = MakeShaderModule(LDevice, FragSpvPath);
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

		// dynamicstate
		std::array<VkDynamicState, 2> dynamicstates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicstate{};
		dynamicstate.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicstate.dynamicStateCount = static_cast<uint32_t>(dynamicstates.size());
		dynamicstate.pDynamicStates = dynamicstates.data();

		// vertexinput
		VkPipelineVertexInputStateCreateInfo vertexinput{};
		vertexinput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		std::vector<VkVertexInputBindingDescription> vertexinputbinding = Particle::GetBindings();
		std::vector<VkVertexInputAttributeDescription> vertexinputattributes = Particle::GetAttributes();
		if      (StructType == 0) {
			vertexinputbinding = Particle::GetBindings();
			vertexinputattributes = Particle::GetAttributes();
		}
		else if (StructType == 1) {
			vertexinputbinding = ExtraParticle::GetBinding();
			vertexinputattributes = ExtraParticle::GetAttributes();
		}
		else if (StructType == 2) {
			vertexinputbinding = ObjVertex::GetBinding();
			vertexinputattributes = ObjVertex::GetAttributes();
		}
		else if (StructType == 3) {
			vertexinputbinding = CubeParticle::GetBinding();
			vertexinputattributes = CubeParticle::GetAttributes();
		}
		else if (StructType == 4) {
			vertexinputbinding = SkyboxVertex::GetBinding();
			vertexinputattributes = SkyboxVertex::GetAttributes();
		}
		else if (StructType == 5) {
			vertexinputbinding = SphereVertex::GetBinding();
			vertexinputattributes = SphereVertex::GetAttributes();
		}
		else if (StructType == 6) {
			vertexinputbinding = CylinderVertex::GetBinding();
			vertexinputattributes = CylinderVertex::GetAttributes();
		}
		else if (StructType == 7) {
			vertexinputbinding = ParticleAni::GetBinding();
			vertexinputattributes = ParticleAni::GetAttributes();
		}

		if (InputBinding) {
			vertexinput.vertexBindingDescriptionCount = vertexinputbinding.size();
			vertexinput.pVertexBindingDescriptions = vertexinputbinding.data();
		
			vertexinput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexinputattributes.size());
			vertexinput.pVertexAttributeDescriptions = vertexinputattributes.data();
		}

		// assembly
		VkPipelineInputAssemblyStateCreateInfo assembly{};
		assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly.primitiveRestartEnable = VK_FALSE;
		assembly.topology = AssemblyType;

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
		rasterization.cullMode = RasterCullMode;
		rasterization.depthClampEnable = VK_FALSE;
		rasterization.frontFace = RasterFrontFace;
		rasterization.lineWidth = 1.0f;
		rasterization.polygonMode = RasterPolygonMode;

		// depthstencil
		VkPipelineDepthStencilStateCreateInfo depthstencil{};
		depthstencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthstencil.depthTestEnable = DepthTestEnable;
		depthstencil.depthCompareOp = CompareOp;
		depthstencil.depthWriteEnable = DepthWriteEnable;
		depthstencil.maxDepthBounds = 1.0f;
		depthstencil.minDepthBounds = 0.0f;

		// color blend
		VkPipelineColorBlendStateCreateInfo colorblend{};
		colorblend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorblend.attachmentCount = static_cast<uint32_t>(colorblendAttachments.size());
		colorblend.logicOpEnable = VK_FALSE;
		colorblend.pAttachments = colorblendAttachments.data();

		// create pipeline
		VkGraphicsPipelineCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createinfo.layout = PipelineLayout;
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
		createinfo.renderPass = RenderPass;
		createinfo.subpass = 0;

		if (vkCreateGraphicsPipelines(LDevice, VK_NULL_HANDLE, 1, &createinfo, Allocator, Pipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphic pipeline!");
		}
		vkDestroyShaderModule(LDevice, vertshadermodule, Allocator);
		vkDestroyShaderModule(LDevice, fragshadermodule, Allocator);
	}

	inline void Test()
	{
	}

	inline void AllocateCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool, VkCommandBuffer& commandBuffer)
	{
		VkCommandBufferAllocateInfo allocateinfo{};
		allocateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateinfo.commandPool = CommandPool;
		allocateinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateinfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(LDevice, &allocateinfo, &commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffer!");
		}
	}
}