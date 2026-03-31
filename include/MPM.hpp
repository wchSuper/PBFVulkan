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


struct Cell {
    alignas(16) glm::vec3 vel;
    alignas(4)  float mass;
};

struct Fs {
    alignas(16) glm::vec3 fs1;
    alignas(16) glm::vec3 fs2;
    alignas(16) glm::vec3 fs3;
};

// particle extra attributes
struct ExtraInfo {
    alignas(4) float mass;
    alignas(4) float volume_0;
    alignas(16) glm::vec3 C1;
    alignas(16) glm::vec3 C2;
    alignas(16) glm::vec3 C3;
};

struct UniformMPMSimObject {
    alignas(4) float deltaT;
    alignas(4) float particleCount;
    alignas(4) float elastic_lambda;
    alignas(4) float elastic_mu;
    alignas(16) glm::vec3 world_min;
    alignas(16) glm::vec3 world_size;
};


class MPMSim {
private:
    // MPM fluid range  0.0 - 1.8 for xyz
    const int   resolution = 32;
    const float grid_size = 1.0 / resolution;
    const float inv_grid_size = resolution / 1.0;

    // MPM particle radius = 0.008 equal to PBF particle
    // volumn_0 = (0.5 * grid_size)^3
    // lambda 大  mu 小 => 流体 dt要小于0.0005   lambda 小  mu 大 => 弹性体 
    /*
        5.0 | 5.0 | 0.0005 ；    500 | 0.00000001 | 0.0001
    */
    const float volumn_0 = 0.0000035;
    const float lambda = 500.0;
    const float mu = 0.00000001;
    const float dt = 0.0001;

    // 
    const int times = 1;

    // Buffer
    UniformRenderingObject renderobj;
    VkBuffer UniformRenderBuffer;
    VkDeviceMemory UniformRenderingBufferMemory;
    void* MappedRenderingBuffer;

    UniformMPMSimObject mpmsimobj;
    VkBuffer UniformMPMSimBuffer;
    VkDeviceMemory UniformMPMSimBufferMemory;
    void* MappedMPMSimBuffer;

    std::vector<VkBuffer> GridBuffers;
    std::vector<VkDeviceMemory> GridBufferMemory;

    std::vector<VkBuffer> FsBuffers;
    std::vector<VkDeviceMemory> FsBufferMemory;

    std::vector<VkBuffer> ExBuffers;
    std::vector<VkDeviceMemory> ExBufferMemory;

    // Pipeline
    VkDescriptorSetLayout SimulateDescriptorSetLayout;
    std::vector<VkDescriptorSet> SimulateDescriptorSet;
    VkPipelineLayout SimulatePipelineLayout;
    VkPipeline SimulatePipeline_ClearGrid;
    VkPipeline SimulatePipeline_ParticleToGrid;
    VkPipeline SimulatePipeline_UpdateGrid;
    VkPipeline SimulatePipeline_GridToParticle;

    std::vector<VkCommandBuffer> SimulatingCommandBuffers;

    uint32_t MAXInFlightRendering = 2;
    uint32_t ONE_GROUP_INVOCATION_COUNT = 256;
    uint32_t GRID_WORK_GROUP_COUNT;
    uint32_t WORK_GROUP_COUNT;
    std::vector<Cell> grids;
    std::vector<Fs> fs;
    std::vector<ExtraInfo> ex;           // particles   MPM extension 
    std::vector<Particle> particles;
public:
    // init data
    /*
        @OuterData:  particles
        @InnnerData: grids / fs / ex
        ExtraInfo is an extension of Particle in attributes
    */
    void InitData(const std::vector<Particle>& inParticles, const UniformRenderingObject& inRenderObj, const glm::vec3& worldMin, const glm::vec3& worldMax)
    {
        int n = inParticles.size();
        for (int i = 0; i < n; i++) {
            ExtraInfo exi;
            exi.mass     = volumn_0;
            exi.volume_0 = volumn_0;
            exi.C1       = glm::vec3(1.0, 0.0, 0.0);
            exi.C2       = glm::vec3(0.0, 1.0, 0.0);
            exi.C3       = glm::vec3(0.0, 0.0, 1.0);

            Fs fss;
            fss.fs1 = glm::vec3(1.0, 0.0, 0.0);
            fss.fs2 = glm::vec3(0.0, 1.0, 0.0);
            fss.fs3 = glm::vec3(0.0, 0.0, 1.0);

            ex.push_back(exi);
            fs.push_back(fss);
        }

        for (int i = 0; i < resolution * resolution * resolution; i++) {
            Cell grid;
            grid.mass = 0.0f;
            grid.vel  = glm::vec3(0.0, 0.0, 0.0);
            grids.push_back(grid);
        }

        particles = inParticles;

        renderobj = inRenderObj;
        mpmsimobj.deltaT         = dt;
        mpmsimobj.particleCount  = particles.size();
        mpmsimobj.elastic_lambda = lambda;
        mpmsimobj.elastic_mu     = mu;
        mpmsimobj.world_min      = worldMin;
        mpmsimobj.world_size     = worldMax - worldMin;
    }

    // Uniform Buffer
    void CreateUniformBuffer(VkPhysicalDevice PDevice, VkDevice LDevice)
    {
        // 1. render
        {
            VkDeviceSize size = sizeof(UniformRenderingObject);
            Buffer::CreateBuffer(PDevice, LDevice, UniformRenderBuffer, UniformRenderingBufferMemory, size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkMapMemory(LDevice, UniformRenderingBufferMemory, 0, size, 0, &MappedRenderingBuffer);
            memcpy(MappedRenderingBuffer, &renderobj, size);
        }
        // 2. sim
        {
            VkDeviceSize size = sizeof(UniformMPMSimObject);
            Buffer::CreateBuffer(PDevice, LDevice, UniformMPMSimBuffer, UniformMPMSimBufferMemory, size,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkMapMemory(LDevice, UniformMPMSimBufferMemory, 0, size, 0, &MappedMPMSimBuffer);
            memcpy(MappedMPMSimBuffer, &mpmsimobj, size);
        }
    }

    void CreateGPUBuffer(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue)
    {
        // 1. grid
        {
            if (grids.size() % ONE_GROUP_INVOCATION_COUNT != 0) {
                GRID_WORK_GROUP_COUNT = grids.size() / ONE_GROUP_INVOCATION_COUNT + 1;
            }
            else {
                GRID_WORK_GROUP_COUNT = grids.size() / ONE_GROUP_INVOCATION_COUNT;
            }
            GridBufferMemory.resize(MAXInFlightRendering);
            GridBuffers.resize(MAXInFlightRendering);
            VkDeviceSize size = grids.size() * sizeof(Cell);

            for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
                VkBuffer stagingbuffer;
                VkDeviceMemory stagingmemory;
                Buffer::CreateBuffer(PDevice, LDevice, stagingbuffer, stagingmemory, size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                Buffer::CreateBuffer(PDevice, LDevice, GridBuffers[i], GridBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                void* data;
                vkMapMemory(LDevice, stagingmemory, 0, size, 0, &data);
                memcpy(data, grids.data(), size);

                auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
                VkBufferCopy region{};
                region.size = size;
                region.dstOffset = region.srcOffset = 0;
                vkCmdCopyBuffer(cb, stagingbuffer, GridBuffers[i], 1, &region);
                VkSubmitInfo submitinfo{};
                Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

                vkDeviceWaitIdle(LDevice);
                Buffer::CleanupBuffer(LDevice, stagingbuffer, stagingmemory, true);
            }
        }
        // 2. Fs
        {
            FsBufferMemory.resize(MAXInFlightRendering);
            FsBuffers.resize(MAXInFlightRendering);
            VkDeviceSize size = particles.size() * sizeof(Fs);

            for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
                VkBuffer stagingbuffer;
                VkDeviceMemory stagingmemory;
                Buffer::CreateBuffer(PDevice, LDevice, stagingbuffer, stagingmemory, size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                Buffer::CreateBuffer(PDevice, LDevice, FsBuffers[i], FsBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                void* data;
                vkMapMemory(LDevice, stagingmemory, 0, size, 0, &data);
                memcpy(data, fs.data(), size);

                auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
                VkBufferCopy region{};
                region.size = size;
                region.dstOffset = region.srcOffset = 0;
                vkCmdCopyBuffer(cb, stagingbuffer, FsBuffers[i], 1, &region);
                VkSubmitInfo submitinfo{};
                Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

                vkDeviceWaitIdle(LDevice);
                Buffer::CleanupBuffer(LDevice, stagingbuffer, stagingmemory, true);
            }
        }
        // 3. ExtraInfo
        {
            if (particles.size() % ONE_GROUP_INVOCATION_COUNT != 0) {
                WORK_GROUP_COUNT = particles.size() / ONE_GROUP_INVOCATION_COUNT + 1;
            }
            else {
                WORK_GROUP_COUNT = particles.size() / ONE_GROUP_INVOCATION_COUNT;
            }
            ExBufferMemory.resize(MAXInFlightRendering);
            ExBuffers.resize(MAXInFlightRendering);
            VkDeviceSize size = particles.size() * sizeof(ExtraInfo);

            for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
                VkBuffer stagingbuffer;
                VkDeviceMemory stagingmemory;
                Buffer::CreateBuffer(PDevice, LDevice, stagingbuffer, stagingmemory, size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                Buffer::CreateBuffer(PDevice, LDevice, ExBuffers[i], ExBufferMemory[i], size,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                void* data;
                vkMapMemory(LDevice, stagingmemory, 0, size, 0, &data);
                memcpy(data, ex.data(), size);

                auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
                VkBufferCopy region{};
                region.size = size;
                region.dstOffset = region.srcOffset = 0;
                vkCmdCopyBuffer(cb, stagingbuffer, ExBuffers[i], 1, &region);
                VkSubmitInfo submitinfo{};
                Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

                vkDeviceWaitIdle(LDevice);
                Buffer::CleanupBuffer(LDevice, stagingbuffer, stagingmemory, true);
            }
        }
    }

    void UpdateUniforms(const UniformRenderingObject& new_renderobj)
    {
        renderobj = new_renderobj;
        if (MappedRenderingBuffer) {
            memcpy(MappedRenderingBuffer, &renderobj, sizeof(UniformRenderingObject));
        }
    }

    // DescriptorSet
    void CreateDescriptorSetLayout(VkDevice LDevice)
    {
        std::vector<BindingDesc> descs = {
            { 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 5, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 6, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT },
            { 7, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT }
        };
        Base::descriptorSetLayoutBinding(LDevice, descs, SimulateDescriptorSetLayout);
    }
    void CreateDescriptorSet(VkDevice LDevice, VkDescriptorPool DescriptorPool, std::vector<VkBuffer>& ParticleBuffers)
    {
        SimulateDescriptorSet.resize(MAXInFlightRendering);
        VkDescriptorSetAllocateInfo allocateinfo{};
        allocateinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateinfo.descriptorPool = DescriptorPool;
        allocateinfo.descriptorSetCount = 1;
        allocateinfo.pSetLayouts = &SimulateDescriptorSetLayout;
        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            if (vkAllocateDescriptorSets(LDevice, &allocateinfo, &SimulateDescriptorSet[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate simulate descriptor set!");
            }
        }
        std::vector<VkWriteDescriptorSet> writes(8);
        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            VkDescriptorBufferInfo particlebufferinfo_lastframe{};
            particlebufferinfo_lastframe.buffer = ParticleBuffers[(i - 1) % MAXInFlightRendering];
            particlebufferinfo_lastframe.offset = 0;
            particlebufferinfo_lastframe.range = sizeof(Particle) * particles.size();

            VkDescriptorBufferInfo particlebufferinfo_thisframe{};
            particlebufferinfo_thisframe.buffer = ParticleBuffers[i];
            particlebufferinfo_thisframe.offset = 0;
            particlebufferinfo_thisframe.range = sizeof(Particle) * particles.size();

            VkDescriptorBufferInfo fsbufferinfo_lastframe{};
            fsbufferinfo_lastframe.buffer = FsBuffers[(i - 1) % MAXInFlightRendering];
            fsbufferinfo_lastframe.offset = 0;
            fsbufferinfo_lastframe.range = sizeof(Fs) * fs.size();

            VkDescriptorBufferInfo fsbufferinfo_thisframe{};
            fsbufferinfo_thisframe.buffer = FsBuffers[i];
            fsbufferinfo_thisframe.offset = 0;
            fsbufferinfo_thisframe.range = sizeof(Fs) * fs.size();

            VkDescriptorBufferInfo gridbufferinfo_thisframe{};
            gridbufferinfo_thisframe.buffer = GridBuffers[i];
            gridbufferinfo_thisframe.offset = 0;
            gridbufferinfo_thisframe.range = sizeof(Cell) * grids.size();

            VkDescriptorBufferInfo extrabufferinfo_lastframe{};
            extrabufferinfo_lastframe.buffer = ExBuffers[(i - 1) % MAXInFlightRendering];
            extrabufferinfo_lastframe.offset = 0;
            extrabufferinfo_lastframe.range = sizeof(ExtraInfo) * particles.size();

            VkDescriptorBufferInfo extrabufferinfo_thisframe{};
            extrabufferinfo_thisframe.buffer = ExBuffers[i];
            extrabufferinfo_thisframe.offset = 0;
            extrabufferinfo_thisframe.range = sizeof(ExtraInfo) * particles.size();

            VkDescriptorBufferInfo simulatingbufferinfo{};
            simulatingbufferinfo.buffer = UniformMPMSimBuffer;
            simulatingbufferinfo.offset = 0;
            simulatingbufferinfo.range = sizeof(UniformMPMSimObject);

            std::vector<WriteDesc> writeInfo = {
                {particlebufferinfo_lastframe,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                {particlebufferinfo_thisframe,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                {gridbufferinfo_thisframe,         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                {fsbufferinfo_lastframe,           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                {fsbufferinfo_thisframe,           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                {extrabufferinfo_lastframe,        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                {extrabufferinfo_thisframe,        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
                {simulatingbufferinfo,             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
            };
            Base::DescriptorWrite(LDevice, writeInfo, writes, SimulateDescriptorSet[i]);
        }
    }

    // Pipeline - SimulatePipelineLayout
    void CreateSimPipelineLayout(VkDevice LDevice) {
        VkPipelineLayoutCreateInfo simulatecreateinfo{};
        simulatecreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        simulatecreateinfo.pSetLayouts = &SimulateDescriptorSetLayout;
        simulatecreateinfo.setLayoutCount = 1;
        if (vkCreatePipelineLayout(LDevice, &simulatecreateinfo, Allocator, &SimulatePipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create simulate pipeline layout!");
        }
    }

    void CreateSimPipeline(VkDevice LDevice)
    {
        auto computershadermodule_cleargrid      = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM/spv/gridclear.comp.spv");
        auto computershadermodule_particletogrid = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM/spv/p2g.comp.spv");
        auto computershadermodule_updategrid     = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM/spv/updategrid.comp.spv");
        auto computershadermodule_gridtoparticle = Base::MakeShaderModule(LDevice, "resources/shaders/glsl/MPM/spv/g2p.comp.spv");

        std::vector<VkShaderModule> shadermodules = {
            computershadermodule_cleargrid, computershadermodule_particletogrid,
            computershadermodule_updategrid,  computershadermodule_gridtoparticle
        };
        std::vector<VkPipeline*> pcomputepipelines = {
            &SimulatePipeline_ClearGrid,
            &SimulatePipeline_ParticleToGrid,
            &SimulatePipeline_UpdateGrid,
            &SimulatePipeline_GridToParticle 
        };
        for (uint32_t i = 0; i < shadermodules.size(); ++i) {
            VkPipelineShaderStageCreateInfo stageinfo{};
            stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageinfo.pName = "main";
            stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            stageinfo.module = shadermodules[i];
            VkComputePipelineCreateInfo createinfo{};
            createinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            createinfo.layout = SimulatePipelineLayout;
            createinfo.stage = stageinfo;
            if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &createinfo, Allocator, pcomputepipelines[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create simulating compute pipeline!");
            }
        }
        for (auto& computershadermodule : shadermodules) {
            vkDestroyShaderModule(LDevice, computershadermodule, Allocator);
        }
    }

    void RecordSimCommandBuffer(VkDevice LDevice, VkCommandPool CommandPool)
    {
        SimulatingCommandBuffers.resize(MAXInFlightRendering);
        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            VkCommandBufferAllocateInfo allocateinfo{};
            allocateinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateinfo.commandPool = CommandPool;
            allocateinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateinfo.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(LDevice, &allocateinfo, &SimulatingCommandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate simulating command buffer!");
            }
            VkCommandBufferBeginInfo begininfo{};
            begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

            if (vkBeginCommandBuffer(SimulatingCommandBuffers[i], &begininfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin simulating command buffer!");
            }

            VkMemoryBarrier memorybarrier{};
            memorybarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memorybarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            memorybarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
            memorybarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memorybarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

            for (int k = 0; k < times; k++) {
                vkCmdBindDescriptorSets(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipelineLayout, 0, 1, &SimulateDescriptorSet[i], 0, nullptr);
                vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ClearGrid);
                vkCmdDispatch(SimulatingCommandBuffers[i], GRID_WORK_GROUP_COUNT, 1, 1);

                vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ParticleToGrid);
                vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

                vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_UpdateGrid);
                vkCmdDispatch(SimulatingCommandBuffers[i], GRID_WORK_GROUP_COUNT, 1, 1);

                vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
                vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_GridToParticle);
                vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);
            }

            auto result = vkEndCommandBuffer(SimulatingCommandBuffers[i]);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to end simulating command buffer!");
            }
        }
    }

    std::vector<VkCommandBuffer> GetSimCmdBuffer() const
    {
        return this->SimulatingCommandBuffers;
    }

    void PreCreateResources(VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
        VkDescriptorPool DescriptorPool, std::vector<VkBuffer>& ParticleBuffers, std::vector<Particle>& inParticles, UniformRenderingObject& RenderObj,
        const glm::vec3& worldMin, const glm::vec3& worldMax)
    {
        // init data
        InitData(inParticles, RenderObj, worldMin, worldMax);
        
        // create uniform and GPU buffers
        CreateUniformBuffer(PDevice, LDevice);
        CreateGPUBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue);

        // descriptorset 
        CreateDescriptorSetLayout(LDevice);
        CreateDescriptorSet(LDevice, DescriptorPool, ParticleBuffers);

        // simulation pipelines
        CreateSimPipelineLayout(LDevice);
        CreateSimPipeline(LDevice);

        // record simulation command buffer
        RecordSimCommandBuffer(LDevice, CommandPool);
    }

    void Cleanup(VkDevice LDevice, VkCommandPool CommandPool)
    {
        vkDeviceWaitIdle(LDevice);

        Buffer::CleanupBuffer(LDevice, UniformRenderBuffer, UniformRenderingBufferMemory, true);
        Buffer::CleanupBuffer(LDevice, UniformMPMSimBuffer, UniformMPMSimBufferMemory, true);
        for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
            Buffer::CleanupBuffer(LDevice, GridBuffers[i], GridBufferMemory[i], false);
            Buffer::CleanupBuffer(LDevice, FsBuffers[i], FsBufferMemory[i], false);
            Buffer::CleanupBuffer(LDevice, ExBuffers[i], ExBufferMemory[i], false);
        }

        vkDestroyPipeline(LDevice, SimulatePipeline_ClearGrid, Allocator);
        vkDestroyPipeline(LDevice, SimulatePipeline_ParticleToGrid, Allocator);
        vkDestroyPipeline(LDevice, SimulatePipeline_UpdateGrid, Allocator);
        vkDestroyPipeline(LDevice, SimulatePipeline_GridToParticle, Allocator);

        vkDestroyPipelineLayout(LDevice, SimulatePipelineLayout, Allocator);
        vkDestroyDescriptorSetLayout(LDevice, SimulateDescriptorSetLayout, Allocator);

        //if (!SimulatingCommandBuffers.empty()) {
        //    vkFreeCommandBuffers(LDevice, CommandPool, static_cast<uint32_t>(SimulatingCommandBuffers.size()), SimulatingCommandBuffers.data());
        //}
    }
};