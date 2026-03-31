#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <cstring>

#include "vulkan/vulkan.h"
#include "glm/glm.hpp"

#include "renderer_types.h"
#include "base.hpp"
#include "buffer.hpp"

#define Allocator nullptr

struct MPM2Cell {
    alignas(4) int vx;
    alignas(4) int vy;
    alignas(4) int vz;
    alignas(4) int mass;
};

struct MPM2Affine {
    alignas(16) glm::vec3 c0;
    alignas(16) glm::vec3 c1;
    alignas(16) glm::vec3 c2;
};

struct UniformMPM2Object {
    alignas(16) glm::vec3 real_box_size;
    alignas(16) glm::vec3 init_box_size;
    alignas(16) glm::vec3 world_min;
    alignas(16) glm::vec3 world_size;
    alignas(4) uint32_t cellCount;
    alignas(4) uint32_t particleCount;
    alignas(4) float gravity;
    alignas(4) float fixed_point_multiplier;
    alignas(4) float dt;
    alignas(4) float stiffness;
    alignas(4) float rest_density;
    alignas(4) float viscosity;
};

class MPM2Sim {
private:
    uint32_t GRID_SIZE = 64;
    uint32_t SUBSTEPS = 4;

    UniformMPM2Object uniformMPMObj{};
    VkBuffer uniformMPMBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uniformMPMBufferMemory = VK_NULL_HANDLE;
    void* mappedMPMBuffer = nullptr;

    std::vector<MPM2Cell> cells;
    std::vector<VkBuffer> cellBuffers;
    std::vector<VkDeviceMemory> cellBufferMemory;

    std::vector<MPM2Affine> affines;
    std::vector<VkBuffer> affineBuffers;
    std::vector<VkDeviceMemory> affineBufferMemory;

    std::vector<Particle> fluidParticles;

    VkDescriptorSetLayout simulateDescriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> simulateDescriptorSet;
    VkPipelineLayout simulatePipelineLayout = VK_NULL_HANDLE;

    VkPipeline clearGridPipeline = VK_NULL_HANDLE;
    VkPipeline particleToGridPipeline1 = VK_NULL_HANDLE;
    VkPipeline particleToGridPipeline2 = VK_NULL_HANDLE;
    VkPipeline updateGridPipeline = VK_NULL_HANDLE;
    VkPipeline mgClearPipeline = VK_NULL_HANDLE;
    VkPipeline divergencePipeline = VK_NULL_HANDLE;
    VkPipeline mgSmoothPipeline = VK_NULL_HANDLE;
    VkPipeline mgResidualPipeline = VK_NULL_HANDLE;
    VkPipeline mgRestrictPipeline = VK_NULL_HANDLE;
    VkPipeline mgProlongPipeline = VK_NULL_HANDLE;
    VkPipeline projectPipeline = VK_NULL_HANDLE;
    VkPipeline gridToParticlePipeline = VK_NULL_HANDLE;

    std::vector<uint32_t> mgLevelN;
    std::vector<uint32_t> mgLevelOffsets;
    uint32_t mgTotalCellCount = 0;

    std::vector<VkBuffer> pressureABuffers;
    std::vector<VkDeviceMemory> pressureABufferMemory;
    std::vector<VkBuffer> pressureBBuffers;
    std::vector<VkDeviceMemory> pressureBBufferMemory;
    std::vector<VkBuffer> rhsBuffers;
    std::vector<VkDeviceMemory> rhsBufferMemory;
    std::vector<VkBuffer> residualBuffers;
    std::vector<VkDeviceMemory> residualBufferMemory;

    std::vector<VkCommandBuffer> simulatingCommandBuffers;

    uint32_t MAXInFlightRendering = 2;
    uint32_t ONE_GROUP_INVOCATION_COUNT = 256;
    uint32_t GRID_WORK_GROUP_COUNT = 1;
    uint32_t PARTICLE_WORK_GROUP_COUNT = 1;
    uint32_t MG_CLEAR_WORK_GROUP_COUNT = 1;

    bool incompressibleProjection = false;

    void InitMultigridLayout()
    {
        mgLevelN.clear();
        mgLevelOffsets.clear();

        uint32_t offset = 0;
        uint32_t n = GRID_SIZE;
        while (n >= 4) {
            mgLevelN.push_back(n);
            mgLevelOffsets.push_back(offset);
            offset += n * n * n;
            if (n == 4) break;
            n /= 2;
        }
        mgTotalCellCount = offset;
        MG_CLEAR_WORK_GROUP_COUNT = (mgTotalCellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT;
    }

public:
    void SetFluidParticles(const std::vector<Particle>& particles, float frameDt, const glm::vec3& worldMin, const glm::vec3& worldMax)
    {
        auto cellCount = static_cast<uint32_t>(GRID_SIZE * GRID_SIZE * GRID_SIZE);
        auto particleCount = static_cast<uint32_t>(particles.size());

        uniformMPMObj.real_box_size = glm::vec3(GRID_SIZE, GRID_SIZE, GRID_SIZE);
        uniformMPMObj.init_box_size = glm::vec3(GRID_SIZE, GRID_SIZE, GRID_SIZE);
        uniformMPMObj.world_min = worldMin;
        uniformMPMObj.world_size = worldMax - worldMin;
        uniformMPMObj.cellCount = cellCount;
        uniformMPMObj.particleCount = particleCount;
        uniformMPMObj.gravity = -2.8f;
        uniformMPMObj.fixed_point_multiplier = 1e7f;
        uniformMPMObj.dt = frameDt;
        uniformMPMObj.stiffness = 1.8f;
        uniformMPMObj.rest_density = 1.0f;
        uniformMPMObj.viscosity = 0.0001f;

        cells.assign(cellCount, MPM2Cell{});
        affines.assign(particleCount, MPM2Affine{ glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f) });
        fluidParticles = particles;

        PARTICLE_WORK_GROUP_COUNT = (particleCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT;
        GRID_WORK_GROUP_COUNT = (cellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT;

        InitMultigridLayout();
    }

    void CreateUniformBuffer(VkPhysicalDevice PDevice, VkDevice LDevice)
    {
        VkDeviceSize size = sizeof(UniformMPM2Object);
        Buffer::CreateBuffer(PDevice, LDevice, uniformMPMBuffer, uniformMPMBufferMemory, size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(LDevice, uniformMPMBufferMemory, 0, size, 0, &mappedMPMBuffer);
        memcpy(mappedMPMBuffer, &uniformMPMObj, size);
    }

    void CreateGPUBuffer(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue)
    {
        {
            cellBufferMemory.resize(MAXInFlightRendering);
            cellBuffers.resize(MAXInFlightRendering);
            VkDeviceSize size = cells.size() * sizeof(MPM2Cell);
            for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
                Buffer::CreateBuffer(PDevice, LDevice, cellBuffers[i], cellBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, cells.data(), cellBuffers[i]);
            }
        }

        {
            affineBufferMemory.resize(MAXInFlightRendering);
            affineBuffers.resize(MAXInFlightRendering);
            VkDeviceSize size = affines.size() * sizeof(MPM2Affine);
            for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
                Buffer::CreateBuffer(PDevice, LDevice, affineBuffers[i], affineBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, affines.data(), affineBuffers[i]);
            }
        }

        {
            pressureABufferMemory.resize(MAXInFlightRendering);
            pressureABuffers.resize(MAXInFlightRendering);
            pressureBBufferMemory.resize(MAXInFlightRendering);
            pressureBBuffers.resize(MAXInFlightRendering);
            rhsBufferMemory.resize(MAXInFlightRendering);
            rhsBuffers.resize(MAXInFlightRendering);
            residualBufferMemory.resize(MAXInFlightRendering);
            residualBuffers.resize(MAXInFlightRendering);

            VkDeviceSize size = static_cast<VkDeviceSize>(mgTotalCellCount) * sizeof(float);
            for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
                Buffer::CreateBuffer(PDevice, LDevice, pressureABuffers[i], pressureABufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                Buffer::CreateBuffer(PDevice, LDevice, pressureBBuffers[i], pressureBBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                Buffer::CreateBuffer(PDevice, LDevice, rhsBuffers[i], rhsBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                Buffer::CreateBuffer(PDevice, LDevice, residualBuffers[i], residualBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            }
        }
    }

    void CreateDescriptorSetLayout(VkDevice LDevice)
    {
        std::vector<BindingDesc> descs = {
            { 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 5, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 6, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 7, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT }
        };
        Base::descriptorSetLayoutBinding(LDevice, descs, simulateDescriptorSetLayout);
    }

    void CreateDescriptorSet(VkDevice LDevice, VkDescriptorPool DescriptorPool, std::vector<VkBuffer>& ParticleBuffers)
    {
        simulateDescriptorSet.resize(MAXInFlightRendering);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = DescriptorPool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &simulateDescriptorSetLayout;

        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            if (vkAllocateDescriptorSets(LDevice, &allocateInfo, &simulateDescriptorSet[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate simulate MPM2 descriptor set!");
            }
        }

        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            VkDescriptorBufferInfo uniformInfo{ uniformMPMBuffer, 0, sizeof(UniformMPM2Object) };
            VkDescriptorBufferInfo cellInfo{ cellBuffers[i], 0, sizeof(MPM2Cell) * static_cast<uint32_t>(cells.size()) };
            VkDescriptorBufferInfo particleInfo{ ParticleBuffers[i], 0, sizeof(Particle) * static_cast<uint32_t>(fluidParticles.size()) };
            VkDescriptorBufferInfo affineInfo{ affineBuffers[i], 0, sizeof(MPM2Affine) * static_cast<uint32_t>(affines.size()) };

            VkDescriptorBufferInfo pressureAInfo{ pressureABuffers[i], 0, sizeof(float) * static_cast<uint32_t>(mgTotalCellCount) };
            VkDescriptorBufferInfo pressureBInfo{ pressureBBuffers[i], 0, sizeof(float) * static_cast<uint32_t>(mgTotalCellCount) };
            VkDescriptorBufferInfo rhsInfo{ rhsBuffers[i], 0, sizeof(float) * static_cast<uint32_t>(mgTotalCellCount) };
            VkDescriptorBufferInfo residualInfo{ residualBuffers[i], 0, sizeof(float) * static_cast<uint32_t>(mgTotalCellCount) };

            std::vector<WriteDesc> writeInfo = {
                {uniformInfo,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0},
                {cellInfo,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
                {particleInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
                {affineInfo,   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
                {pressureAInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
                {pressureBInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5},
                {rhsInfo,       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6},
                {residualInfo,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7}
            };
            std::vector<VkWriteDescriptorSet> writes(writeInfo.size());
            Base::DescriptorWrite(LDevice, writeInfo, writes, simulateDescriptorSet[i]);
        }
    }

    void CreateComputePipeline(VkDevice LDevice)
    {
        {
            VkPipelineLayoutCreateInfo simulateCreateInfo{};
            simulateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            simulateCreateInfo.pSetLayouts = &simulateDescriptorSetLayout;
            simulateCreateInfo.setLayoutCount = 1;

            VkPushConstantRange push{};
            push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            push.offset = 0;
            push.size = sizeof(int) * 4;
            simulateCreateInfo.pushConstantRangeCount = 1;
            simulateCreateInfo.pPushConstantRanges = &push;

            if (vkCreatePipelineLayout(LDevice, &simulateCreateInfo, Allocator, &simulatePipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create MPM2 pipeline layout!");
            }
        }

        auto sm_cleargrid = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/cleargrid.comp.spv");
        auto sm_p2g_1     = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/p2g_1.comp.spv");
        auto sm_p2g_2     = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/p2g_2.comp.spv");
        auto sm_update    = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/updategrid.comp.spv");
        auto sm_mg_clear  = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/mg_clear.comp.spv");
        auto sm_div       = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/divergence.comp.spv");
        auto sm_mg_smooth = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/mg_smooth.comp.spv");
        auto sm_mg_res    = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/mg_residual.comp.spv");
        auto sm_mg_rest   = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/mg_restrict.comp.spv");
        auto sm_mg_prol   = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/mg_prolong.comp.spv");
        auto sm_project   = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/project.comp.spv");
        auto sm_g2p       = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM2/spv/g2p.comp.spv");

        std::vector<VkShaderModule> shaderModules = {
            sm_cleargrid,
            sm_p2g_1,
            sm_p2g_2,
            sm_update,
            sm_mg_clear,
            sm_div,
            sm_mg_smooth,
            sm_mg_res,
            sm_mg_rest,
            sm_mg_prol,
            sm_project,
            sm_g2p
        };
        std::vector<VkPipeline*> pipelines = {
            &clearGridPipeline,
            &particleToGridPipeline1,
            &particleToGridPipeline2,
            &updateGridPipeline,
            &mgClearPipeline,
            &divergencePipeline,
            &mgSmoothPipeline,
            &mgResidualPipeline,
            &mgRestrictPipeline,
            &mgProlongPipeline,
            &projectPipeline,
            &gridToParticlePipeline
        };

        for (uint32_t i = 0; i < shaderModules.size(); ++i) {
            VkPipelineShaderStageCreateInfo stageInfo{};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.pName = "main";
            stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            stageInfo.module = shaderModules[i];

            VkComputePipelineCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            createInfo.layout = simulatePipelineLayout;
            createInfo.stage = stageInfo;

            if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &createInfo, Allocator, pipelines[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create simulating MPM2 pipeline!");
            }
        }

        for (auto& m : shaderModules) {
            vkDestroyShaderModule(LDevice, m, Allocator);
        }
    }

    void RecordMPMCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool, std::vector<VkBuffer>& ParticleBuffers)
    {
        struct MGPush {
            int n;
            int levelOffset;
            int coarseOffset;
            int src;
        };

        simulatingCommandBuffers.resize(MAXInFlightRendering);
        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            Base::AllocateCommandBuffer(LDevice, CommandPool, simulatingCommandBuffers[i]);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

            if (vkBeginCommandBuffer(simulatingCommandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin MPM2 command buffer!");
            }

            VkMemoryBarrier memorybarrier{};
            memorybarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memorybarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            memorybarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 1, &memorybarrier, 0, nullptr, 0, nullptr);

            memorybarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memorybarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

            for (uint32_t k = 0; k < SUBSTEPS; ++k) {
                vkCmdBindDescriptorSets(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, simulatePipelineLayout,
                    0, 1, &simulateDescriptorSet[i], 0, nullptr);

                vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, clearGridPipeline);
                vkCmdDispatch(simulatingCommandBuffers[i], GRID_WORK_GROUP_COUNT, 1, 1);

                vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, particleToGridPipeline1);
                vkCmdDispatch(simulatingCommandBuffers[i], PARTICLE_WORK_GROUP_COUNT, 1, 1);

                vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, particleToGridPipeline2);
                vkCmdDispatch(simulatingCommandBuffers[i], PARTICLE_WORK_GROUP_COUNT, 1, 1);

                vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, updateGridPipeline);
                vkCmdDispatch(simulatingCommandBuffers[i], GRID_WORK_GROUP_COUNT, 1, 1);

                if (incompressibleProjection && !mgLevelN.empty()) {
                    MGPush mg{};

                    vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                    vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mgClearPipeline);
                    mg.n = static_cast<int>(mgTotalCellCount);
                    mg.levelOffset = 0;
                    mg.coarseOffset = 0;
                    mg.src = 0;
                    vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                    vkCmdDispatch(simulatingCommandBuffers[i], MG_CLEAR_WORK_GROUP_COUNT, 1, 1);

                    vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                    vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, divergencePipeline);
                    mg.n = static_cast<int>(mgLevelN[0]);
                    mg.levelOffset = static_cast<int>(mgLevelOffsets[0]);
                    mg.coarseOffset = 0;
                    mg.src = 0;
                    vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                    vkCmdDispatch(simulatingCommandBuffers[i], GRID_WORK_GROUP_COUNT, 1, 1);

                    for (size_t level = 0; level + 1 < mgLevelN.size(); ++level) {
                        uint32_t n = mgLevelN[level];
                        uint32_t offset = mgLevelOffsets[level];
                        uint32_t levelCellCount = n * n * n;
                        uint32_t levelGroupCount = (levelCellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT;

                        for (int iter = 0; iter < 4; ++iter) {
                            vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                            vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mgSmoothPipeline);
                            mg.n = static_cast<int>(n);
                            mg.levelOffset = static_cast<int>(offset);
                            mg.coarseOffset = 0;
                            mg.src = iter & 1;
                            vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                            vkCmdDispatch(simulatingCommandBuffers[i], levelGroupCount, 1, 1);
                        }

                        vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                        vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mgResidualPipeline);
                        mg.n = static_cast<int>(n);
                        mg.levelOffset = static_cast<int>(offset);
                        mg.coarseOffset = 0;
                        mg.src = 0;
                        vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                        vkCmdDispatch(simulatingCommandBuffers[i], levelGroupCount, 1, 1);

                        uint32_t nC = mgLevelN[level + 1];
                        uint32_t offsetC = mgLevelOffsets[level + 1];
                        uint32_t coarseCellCount = nC * nC * nC;
                        uint32_t coarseGroupCount = (coarseCellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT;

                        vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                        vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mgRestrictPipeline);
                        mg.n = static_cast<int>(nC);
                        mg.levelOffset = static_cast<int>(offset);
                        mg.coarseOffset = static_cast<int>(offsetC);
                        mg.src = 0;
                        vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                        vkCmdDispatch(simulatingCommandBuffers[i], coarseGroupCount, 1, 1);
                    }

                    {
                        uint32_t last = static_cast<uint32_t>(mgLevelN.size() - 1);
                        uint32_t n = mgLevelN[last];
                        uint32_t offset = mgLevelOffsets[last];
                        uint32_t levelCellCount = n * n * n;
                        uint32_t levelGroupCount = (levelCellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT;

                        for (int iter = 0; iter < 20; ++iter) {
                            vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                            vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mgSmoothPipeline);
                            mg.n = static_cast<int>(n);
                            mg.levelOffset = static_cast<int>(offset);
                            mg.coarseOffset = 0;
                            mg.src = iter & 1;
                            vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                            vkCmdDispatch(simulatingCommandBuffers[i], levelGroupCount, 1, 1);
                        }
                    }

                    for (int level = static_cast<int>(mgLevelN.size()) - 2; level >= 0; --level) {
                        uint32_t n = mgLevelN[static_cast<size_t>(level)];
                        uint32_t offset = mgLevelOffsets[static_cast<size_t>(level)];
                        uint32_t offsetC = mgLevelOffsets[static_cast<size_t>(level + 1)];
                        uint32_t levelCellCount = n * n * n;
                        uint32_t levelGroupCount = (levelCellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT;

                        vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                        vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mgProlongPipeline);
                        mg.n = static_cast<int>(n);
                        mg.levelOffset = static_cast<int>(offset);
                        mg.coarseOffset = static_cast<int>(offsetC);
                        mg.src = 0;
                        vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                        vkCmdDispatch(simulatingCommandBuffers[i], levelGroupCount, 1, 1);

                        for (int iter = 0; iter < 4; ++iter) {
                            vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                            vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mgSmoothPipeline);
                            mg.n = static_cast<int>(n);
                            mg.levelOffset = static_cast<int>(offset);
                            mg.coarseOffset = 0;
                            mg.src = iter & 1;
                            vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                            vkCmdDispatch(simulatingCommandBuffers[i], levelGroupCount, 1, 1);
                        }
                    }

                    vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                    vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, projectPipeline);
                    mg.n = static_cast<int>(mgLevelN[0]);
                    mg.levelOffset = static_cast<int>(mgLevelOffsets[0]);
                    mg.coarseOffset = 0;
                    mg.src = 0;
                    vkCmdPushConstants(simulatingCommandBuffers[i], simulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MGPush), &mg);
                    vkCmdDispatch(simulatingCommandBuffers[i], GRID_WORK_GROUP_COUNT, 1, 1);
                }

                vkCmdPipelineBarrier(simulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(simulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, gridToParticlePipeline);
                vkCmdDispatch(simulatingCommandBuffers[i], PARTICLE_WORK_GROUP_COUNT, 1, 1);
            }

            VkBufferMemoryBarrier endBarrier{};
            endBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            endBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            endBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            endBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            endBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            endBarrier.buffer = ParticleBuffers[i];
            endBarrier.offset = 0;
            endBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(simulatingCommandBuffers[i],
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                0,
                0, nullptr,
                1, &endBarrier,
                0, nullptr);

            auto result = vkEndCommandBuffer(simulatingCommandBuffers[i]);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to end MPM2 command buffer!");
            }
        }
    }

    std::vector<VkCommandBuffer> GetSimCmdBuffer() const
    {
        return simulatingCommandBuffers;
    }

    void PreCreateResources(
        VkPhysicalDevice PDevice,
        VkDevice LDevice,
        VkCommandPool CommandPool,
        VkQueue GraphicNComputeQueue,
        VkDescriptorPool DescriptorPool,
        std::vector<VkBuffer>& particleBuffers,
        std::vector<Particle>& inParticles,
        float frameDt,
        const glm::vec3& worldMin,
        const glm::vec3& worldMax,
        bool enableIncompressibleProjection)
    {
        incompressibleProjection = enableIncompressibleProjection;
        SetFluidParticles(inParticles, frameDt, worldMin, worldMax);
        CreateUniformBuffer(PDevice, LDevice);
        CreateGPUBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue);
        CreateDescriptorSetLayout(LDevice);
        CreateDescriptorSet(LDevice, DescriptorPool, particleBuffers);
        CreateComputePipeline(LDevice);
        RecordMPMCommandBuffer(LDevice, CommandPool, particleBuffers);
    }

    void Cleanup(VkDevice LDevice)
    {
        vkDeviceWaitIdle(LDevice);

        if (uniformMPMBuffer != VK_NULL_HANDLE) {
            Buffer::CleanupBuffer(LDevice, uniformMPMBuffer, uniformMPMBufferMemory, true);
        }

        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            if (i < cellBuffers.size() && cellBuffers[i] != VK_NULL_HANDLE) {
                Buffer::CleanupBuffer(LDevice, cellBuffers[i], cellBufferMemory[i], false);
            }
            if (i < affineBuffers.size() && affineBuffers[i] != VK_NULL_HANDLE) {
                Buffer::CleanupBuffer(LDevice, affineBuffers[i], affineBufferMemory[i], false);
            }
            if (i < pressureABuffers.size() && pressureABuffers[i] != VK_NULL_HANDLE) {
                Buffer::CleanupBuffer(LDevice, pressureABuffers[i], pressureABufferMemory[i], false);
            }
            if (i < pressureBBuffers.size() && pressureBBuffers[i] != VK_NULL_HANDLE) {
                Buffer::CleanupBuffer(LDevice, pressureBBuffers[i], pressureBBufferMemory[i], false);
            }
            if (i < rhsBuffers.size() && rhsBuffers[i] != VK_NULL_HANDLE) {
                Buffer::CleanupBuffer(LDevice, rhsBuffers[i], rhsBufferMemory[i], false);
            }
            if (i < residualBuffers.size() && residualBuffers[i] != VK_NULL_HANDLE) {
                Buffer::CleanupBuffer(LDevice, residualBuffers[i], residualBufferMemory[i], false);
            }
        }

        if (clearGridPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, clearGridPipeline, Allocator);
        if (particleToGridPipeline1 != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, particleToGridPipeline1, Allocator);
        if (particleToGridPipeline2 != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, particleToGridPipeline2, Allocator);
        if (updateGridPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, updateGridPipeline, Allocator);
        if (mgClearPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, mgClearPipeline, Allocator);
        if (divergencePipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, divergencePipeline, Allocator);
        if (mgSmoothPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, mgSmoothPipeline, Allocator);
        if (mgResidualPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, mgResidualPipeline, Allocator);
        if (mgRestrictPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, mgRestrictPipeline, Allocator);
        if (mgProlongPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, mgProlongPipeline, Allocator);
        if (projectPipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, projectPipeline, Allocator);
        if (gridToParticlePipeline != VK_NULL_HANDLE) vkDestroyPipeline(LDevice, gridToParticlePipeline, Allocator);

        if (simulatePipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(LDevice, simulatePipelineLayout, Allocator);
        if (simulateDescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(LDevice, simulateDescriptorSetLayout, Allocator);
    }
};
