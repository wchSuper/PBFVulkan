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


class PBFSim {

private:
	// Buffer
    UniformRenderingObject renderobj;
    VkBuffer UniformRenderBuffer;
    VkDeviceMemory UniformRenderingBufferMemory;
    void* MappedRenderingBuffer;

    UniformSimulatingObject simulatingobj;
    VkBuffer UniformSimulatingBuffer;
    VkDeviceMemory UniformSimulatingBufferMemory;
    void* MappedSimulatingBuffer;


    std::vector<VkBuffer> ParticleBuffers;
    std::vector<VkDeviceMemory> ParticleBufferMemory;
    std::vector<VkBuffer> CubeParticleBuffers;
    std::vector<VkDeviceMemory> CubeParticleBufferMemory;
    std::vector<VkBuffer> RigidParticleBuffers;
    std::vector<VkDeviceMemory> RigidParticleBufferMemory;
    std::vector<VkBuffer> OriginRigidParticleBuffers;
    std::vector<VkDeviceMemory> OriginRigidParticleBufferMemory;
    std::vector<VkBuffer> ShapeMatchingBuffers;
    std::vector<VkDeviceMemory> ShapeMatchingBufferMemory;

    // Pipeline
    VkDescriptorSetLayout SimulateDescriptorSetLayout;
    std::vector<VkDescriptorSet> SimulateDescriptorSet;
    VkPipelineLayout SimulatePipelineLayout;
    // MergeSort
    VkPipeline SimulatePipeline_ResetGrid;
    VkPipeline SimulatePipeline_Partition;
    VkPipeline SimulatePipeline_MergeSort1;
    VkPipeline SimulatePipeline_MergeSort2;
    VkPipeline SimulatePipeline_MergeSortAll;
    VkPipeline SimulatePipeline_IndexMapped;

    // Update Position & Velocity
    VkPipeline SimulatePipeline_Euler;
    VkPipeline SimulatePipeline_Mass;
    VkPipeline SimulatePipeline_Lambda;
    VkPipeline SimulatePipeline_ResetRigid;
    VkPipeline SimulatePipeline_DeltaPosition;
    VkPipeline SimulatePipeline_PositionUpd;
    VkPipeline SimulatePipeline_VelocityUpd;
    VkPipeline SimulatePipeline_VelocityCache;
    VkPipeline SimulatePipeline_ViscosityCorr;
    VkPipeline SimulatePipeline_VorticityCorr;

    // Rigid Particle Shape Matching
    VkPipeline SimulatePipeline_ComputeCM;
    VkPipeline SimulatePipeline_ComputeCM2;
    VkPipeline SimulatePipeline_ComputeApq;
    VkPipeline SimulatePipeline_ComputeRotate;
    VkPipeline SimulatePipeline_RigidDeltaPosition;
    VkPipeline SimulatePipeline_RigidPositionUpd;

    uint32_t MAXInFlightRendering = 2;
    uint32_t ONE_GROUP_INVOCATION_COUNT = 256;
    uint32_t WORK_GROUP_COUNT;

    void CreateBuffers()
    {

    }

};