#pragma once
#include <vector>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "extensionfuncs.h"
#include <iostream>
#include <variant>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include <glm/gtc/quaternion.hpp>  
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

static void check_vk_result2(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

namespace DearGui
{
    inline void CreateGuiDescriptorPool(VkDevice LDevice, VkDescriptorPool& ImGuiDescriptorPool)
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        if (vkCreateDescriptorPool(LDevice, &pool_info, nullptr, &ImGuiDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("could not create ImGui's descriptor pool");
        }
    }

    inline void CreateGuiCommandBuffers(
        VkDevice LDevice,
        QueuefamliyIndices queueFamilyIndices,
        uint32_t MAXInFlightRendering,
        VkCommandPool& ImGuiCommandPool,
        std::vector<VkCommandBuffer>& ImGuiCommandBuffers
    ) {
        // create command pool for imgui
        {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicNcompute.value();

            if (vkCreateCommandPool(LDevice, &poolInfo, nullptr, &ImGuiCommandPool) != VK_SUCCESS) {
                throw std::runtime_error("failed to create graphics command pool!");
            }
        }
        // create command buffers for imgui
        {
            ImGuiCommandBuffers.resize(MAXInFlightRendering);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = ImGuiCommandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = (uint32_t)ImGuiCommandBuffers.size();

            if (vkAllocateCommandBuffers(LDevice, &allocInfo, ImGuiCommandBuffers.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate command buffers!");
            }
        }
    }

    inline void CreateGuiRenderPass(VkDevice LDevice, VkFormat SwapChainImageFormat, VkRenderPass& ImGuiRenderPass)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = SwapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorReference{};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDescription{};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;

        VkSubpassDependency subpassDependency{};
        subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependency.dstSubpass = 0;
        subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependency.srcAccessMask = 0;
        subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 1;
        renderPassCreateInfo.pAttachments = &colorAttachment;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpassDescription;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &subpassDependency;

        if (vkCreateRenderPass(LDevice, &renderPassCreateInfo, nullptr, &ImGuiRenderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("could not create ImGui's render pass");
        }
    }

    inline void CreateGuiFrameBuffers(
        VkDevice LDevice, std::vector<VkImageView>& SwapChainImageViews,
        VkRenderPass& ImGuiRenderPass, VkExtent2D& SwapChainExtent,
        std::vector<VkFramebuffer>& ImGuiFrameBuffers
    )
    {
        ImGuiFrameBuffers.resize(SwapChainImageViews.size());

        for (size_t i = 0; i < SwapChainImageViews.size(); i++) {
            std::array<VkImageView, 1> attachments = {
                SwapChainImageViews[i]
            };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = ImGuiRenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = SwapChainExtent.width;
            framebufferInfo.height = SwapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(LDevice, &framebufferInfo, nullptr, &ImGuiFrameBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    inline void InitImGui(
        GLFWwindow* Window, VkInstance& Instance, VkPhysicalDevice& PDevice, VkDevice& LDevice,
        QueuefamliyIndices& familyIndices, VkQueue& GraphicNComputeQueue, VkDescriptorPool& ImGuiDescriptorPool,
        VkRenderPass& ImGuiRenderPass, std::vector<VkImage>& SwapChainImages
    )
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

        // Setup Dear ImGui style - Modern Theme
        ImGui::StyleColorsDark();
        
        // Apply custom modern style
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Rounding
        style.WindowRounding = 8.0f;
        style.ChildRounding = 5.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 5.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 5.0f;
        style.TabRounding = 5.0f;
        
        // Spacing
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 5);
        style.ItemSpacing = ImVec2(10, 8);
        style.ItemInnerSpacing = ImVec2(6, 6);
        style.IndentSpacing = 20.0f;
        
        // Borders
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        
        // Colors - Modern Dark Theme with Cyan Accent
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.94f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.28f, 0.50f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 0.75f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.34f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.44f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.54f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.36f, 0.90f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.80f, 0.90f, 0.80f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.90f, 1.00f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.30f, 0.80f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.80f, 0.90f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.31f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.26f, 0.80f, 0.90f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.80f, 0.90f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.80f, 0.90f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.80f, 0.90f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.20f, 0.25f, 0.86f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.80f, 0.90f, 0.80f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.65f, 0.75f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.14f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.80f, 0.90f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(Window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = Instance;
        init_info.PhysicalDevice = PDevice;
        init_info.Device = LDevice;
        init_info.QueueFamily = familyIndices.graphicNcompute.value();
        init_info.Queue = GraphicNComputeQueue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = ImGuiDescriptorPool;
        init_info.RenderPass = ImGuiRenderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = 2;
        init_info.ImageCount = SwapChainImages.size();
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.CheckVkResultFn = check_vk_result2;
        ImGui_ImplVulkan_Init(&init_info);

        // upload fonts

        // end upload fonts
    }

    inline void RecordImGuiCommandBuffers(uint32_t imageIndex, VkCommandBuffer& ImGuiCommandBuffer, VkRenderPass& ImGuiRenderPass,
        std::vector<VkFramebuffer>& ImGuiFrameBuffers, VkExtent2D& SwapChainExtent)
    {
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("fluid controller"); // Create a window

            ImGui::Text("Hello ImGui");

            ImGui::End();
        }
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        // Record CommandBuffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(ImGuiCommandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = ImGuiRenderPass;
        renderPassInfo.framebuffer = ImGuiFrameBuffers[imageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = SwapChainExtent;

        std::array<VkClearValue, 2> clearColors;
        clearColors[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearColors[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
        renderPassInfo.pClearValues = clearColors.data();

        vkCmdBeginRenderPass(ImGuiCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(draw_data, ImGuiCommandBuffer);

        vkCmdEndRenderPass(ImGuiCommandBuffer);
        if (vkEndCommandBuffer(ImGuiCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    inline void RecordImGuiCommandBuffers2(uint32_t imageIndex, ImDrawData *draw_data, VkCommandBuffer& ImGuiCommandBuffer, VkRenderPass& ImGuiRenderPass,
        std::vector<VkFramebuffer>& ImGuiFrameBuffers, VkExtent2D& SwapChainExtent)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(ImGuiCommandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = ImGuiRenderPass;
        renderPassInfo.framebuffer = ImGuiFrameBuffers[imageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = SwapChainExtent;

        std::array<VkClearValue, 2> clearColors;
        clearColors[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearColors[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
        renderPassInfo.pClearValues = clearColors.data();

        vkCmdBeginRenderPass(ImGuiCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(draw_data, ImGuiCommandBuffer);

        vkCmdEndRenderPass(ImGuiCommandBuffer);
        if (vkEndCommandBuffer(ImGuiCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}