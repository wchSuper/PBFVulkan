#ifndef RENDER_H
#define RENDER_H
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include <glm/gtc/quaternion.hpp>  
#include <glm/gtc/matrix_transform.hpp> 

#include "tiny_obj_loader.h"
#include "renderer_types.h"
#include <vector>
#include <random>
#include <vulkan/vulkan.h>
#include <array>
#include <optional>
#include <string>
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
#include "specialparticle.hpp"
#include "rigid.hpp"
#include "MPM.hpp"
#include "MPM2.hpp"
#include "sceneset.h"
#include "cloth.h"
#include "sdf.hpp"


// more assets seen in 'sceneset.h'
const std::string CUP_PATH       = "resources/models/cup2.obj";
const std::string DRAGON_PATH    = "resources/models/dragon.obj";
const std::string CYLINDER_PATH  = "resources/models/cylinder.obj";
const std::string TABLE_PATH     = "resources/models/table.obj";
const std::string TABLE_PATH_GLB = "resources/models/table.glb";
const std::string SAND_PATH_GLB = "resources/models/sand.glb";
const std::string SAND_PATH_TEX = "resources/textures/model_textures/sand.jpg";
const std::string WALL_PATH_GLB = "resources/models/wall.glb";
const std::string WALL_PATH_TEX = "resources/textures/model_textures/wall.jpg";
const std::string ROCK1_PATH_GLB = "resources/models/rock1.glb";
const std::string ROCK1_PATH_TEX = "resources/textures/model_textures/rock1.jpg";
const std::string ROCK2_PATH_GLB = "resources/models/rock2.glb";
const std::string ROCK2_PATH_TEX = "resources/textures/model_textures/rock2.jpg";


static const std::vector<SkyboxVertex> skyboxVertices = {
    {glm::vec3(-1.0f, -1.0f, -1.0f)}, {glm::vec3(1.0f, -1.0f, -1.0f)}, {glm::vec3(1.0f, 1.0f, -1.0f)}, {glm::vec3(-1.0f, 1.0f, -1.0f)},
    {glm::vec3(-1.0f, -1.0f, 1.0f)},  {glm::vec3(1.0f, -1.0f, 1.0f)},  {glm::vec3(1.0f, 1.0f, 1.0f)},  {glm::vec3(-1.0f, 1.0f, 1.0f)}
};

static const std::vector<uint32_t> skyboxIndices = {
    1, 2, 6, 6, 5, 1,  // right (+X)
    0, 4, 7, 7, 3, 0,  // left  (-X)
    2, 3, 7, 7, 6, 2,  // up    (+Y)
    0, 1, 5, 5, 4, 0,  // down  (-Y)
    4, 5, 6, 6, 7, 4,  // front (+Z)
    0, 1, 2, 2, 3, 0   // back  (-Z)
};

class Renderer{
public:
    Renderer(uint32_t w,uint32_t h,bool validation = false);
    virtual ~Renderer();

public:
    TickWindowResult TickWindow(float DeltaTime);
    void Init();
    void Cleanup();
    void BaseCleanup();
    void CleanupBuffer(VkBuffer& buffer, VkDeviceMemory& memory, bool mapped);
public:
    void Simulate();
    void BoxRender(uint32_t dstimage);
    void FluidsRender(uint32_t dstimage);
    void CubesRender(uint32_t dstimage);
    void Draw();
public:
    void WaitIdle();
private:
    void CreateBasicDevices();

    void CreateSupportObjects();
    void CleanupSupportObjects();

    void CreateParticleBuffer();
    void CreateDiffuseParticleBuffer();
    void CreateAnisotrophicBuffer();
    void CreateCubeParticleBuffer();
    void CreateRigidParticleBuffer();
    void CreateSkyboxBuffer();
    void CreateSphereBuffer();
    void CreateShapeMatchingBuffer();
    void CreateEllipsoidBuffer();

    void CreateUniformRenderingBuffer();
    void CreateUniformSimulatingBuffer();
    void CreateUniformBoxInfoBuffer();
    void CreateUniformCubeBuffer();

    void CreateDepthResources();
    void CreateThickResources();
    void CreateCausticsResources();
    void CreateTextureResources(const char*, VkImage&, VkImageView&, VkDeviceMemory&, VkSampler&);
    void CreateCubemapTexture(const std::array<std::string, 6>&);
    void CreateDefaultTextureResources();
    void CreateBackgroundResources();
    void CreateCubeResources();

    // Merge Sort Resources Creation
    void CreateCellInfoBuffer();
    void CreateNgbrCountBuffer();
    void CreateNgbrCpyCountBuffer();
    void CreateParticleInCellBuffer();
    void CreateCellOffsetBuffer();
    void CreateTestPBFBuffer();

    // Surface particles
    void CreateSurfaceParticleBuffer();

    // SDF Buffer
    void CreateSDFBuffer();
    void CreateWCSPHBuffers();

    void CreateSwapChain(); 
    void CleanupSwapChain();
    void RecreateSwapChain();

    void CreateDescriptorSetLayout();
    void CreateDescriptorSet();
    void UpdateDescriptorSet();

    void CreateRenderPass();
    void CreateGraphicPipelineLayout();
    void CreateGraphicPipeline();
    void CreateSkyboxGraphicPipeline();
    void CreateSphereGraphicPipeline();
    void CreateDragonGraphicPipeline();

    void CreateComputePipelineLayout();
    void CreateComputePipeline();

    void CreateFramebuffers();

    void RecordSimulatingCommandBuffers();
    void RecordFluidsRenderingCommandBuffers();
    void RecordRigidRenderingCommandBuffers();
    void RecordBoxRenderingCommandBuffers();

private:
    std::vector<Particle> RetrieveParticlesFromGPU(uint32_t frameIndex);
    std::vector<ExtraParticle> RetrieveRigidParticlesFromGPU(uint32_t frameIndex);
    ShapeMatchingInfo RetrieveUniformFromGPU();

    void InjectWaterColumn();
    void UploadFluidParticlesSubrange(uint32_t startParticle, const std::vector<Particle>& newParticles);

    // Window & mouse & key callback
    static void WindowResizeCallback(GLFWwindow* window, int width, int height);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void MouseMoveCallback(GLFWwindow* window, double xPos, double yPos);
    static void MouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    void MousePressed(GLFWwindow* window, int button, int action, int mods);
    void MouseMoved(GLFWwindow* window, double xPos, double yPos);
    void MouseScroll(GLFWwindow* window, double xOffset, double yOffset);
    void KeyProcess(int key, int action);

    // move & scroll & rotate camera in camera.hpp

    // ImGui
    VkDescriptorPool ImGuiDescriptorPool;
    VkCommandPool ImGuiCommandPool;
    std::vector<VkCommandBuffer> ImGuiCommandBuffers;
    VkRenderPass ImGuiRenderPass;
    std::vector<VkFramebuffer> ImGuiFrameBuffers;

    void CreateImGuiResources();
    void ConfigImGui(uint32_t image_index);
    void CleanupImGui();

private:
    VkImageView CreateImageView(VkImage image,VkFormat format,VkImageAspectFlags aspectMask);
private:
    GLFWwindow* Window = nullptr;
    VkInstance Instance;
    VkDebugUtilsMessengerEXT Messenger;
    VkSurfaceKHR Surface;

    VkPhysicalDevice PDevice;
    VkDevice LDevice;
    VkQueue GraphicNComputeQueue;
    VkQueue PresentQueue;

    VkCommandPool CommandPool;
    VkSwapchainKHR SwapChain;
    VkFormat SwapChainImageFormat;
    VkExtent2D SwapChainImageExtent;
    std::vector<VkImage> SwapChainImages;
    std::vector<VkImageView> SwapChainImageViews;

    VkDescriptorPool DescriptorPool;

    VkRenderPass FluidGraphicRenderPass;
    VkRenderPass FluidGraphicRenderPass2;
    VkRenderPass DiffuseGraphicRenderPass;
    VkDescriptorSetLayout FluidGraphicDescriptorSetLayout;
    VkDescriptorSet FluidGraphicDescriptorSet;
    VkPipelineLayout FluidGraphicPipelineLayout;
    VkPipeline FluidGraphicPipeline;
    VkPipeline FluidGraphicPipeline2;
    VkPipeline FluidGraphicPipeline3;   // for cubeparticles (voxelization)
    VkPipeline DiffuseGraphicPipeline;  // for diffuseparticles (ex. splash)

    VkRenderPass BoxGraphicRenderPass;
    VkDescriptorSetLayout BoxGraphicDescriptorSetLayout;
    VkDescriptorSet BoxGraphicDescriptorSet;
    VkPipelineLayout BoxGraphicPipelineLayout;
    VkPipeline BoxGraphicPipeline;

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
    // generate foam particles
    VkPipeline SimulatePipeline_ResetDiffuse;
    VkPipeline SimulatePipeline_SurfaceDetection;
    VkPipeline SimulatePipeline_FoamGeneration;
    VkPipeline SimulatePipeline_FoamUpd;
    VkPipeline SimulatePipeline_Compact;

    // Anisotrophic 
    VkPipeline SimulatePipeline_Anisotrophic;

    // Rigid Particle Shape Matching
    VkPipeline SimulatePipeline_ComputeCM;
    VkPipeline SimulatePipeline_ComputeCM2;
    VkPipeline SimulatePipeline_ComputeApq;
    VkPipeline SimulatePipeline_ComputeRotate;
    VkPipeline SimulatePipeline_RigidDeltaPosition;
    VkPipeline SimulatePipeline_RigidPositionUpd;

    // Ellipsoid Update
    VkPipeline SimulatePipeline_EllipsoidUpd;

    VkPipeline SimulatePipeline_WCSPH_Euler = VK_NULL_HANDLE;
    VkPipeline SimulatePipeline_WCSPH_Pressure = VK_NULL_HANDLE;
    VkPipeline SimulatePipeline_WCSPH_PressureForce = VK_NULL_HANDLE;
    VkPipeline SimulatePipeline_WCSPH_Advect = VK_NULL_HANDLE;

    VkDescriptorSetLayout FilterDecsriptorSetLayout;
    VkDescriptorSet FilterDescriptorSet;
    VkDescriptorSet FilterDescriptorSet2;
    VkPipelineLayout FilterPipelineLayout;
    VkPipeline FilterPipeline;
    VkPipeline FilterPipeline2; 

    VkDescriptorSetLayout CausticsDescriptorSetLayout;
    VkDescriptorSet CausticsDescriptorSet;
    VkDescriptorSet CausticsBlurDescriptorSet;
    VkDescriptorSet CausticsBlurDescriptorSet2;
    VkPipelineLayout CausticsPipelineLayout;
    VkPipeline CausticsPipeline;
    VkPipeline CausticsBlurPipeline;
    VkPipeline CausticsBlurPipeline2;

    VkDescriptorSetLayout PostprocessDescriptorSetLayout;
    std::vector<VkDescriptorSet> PostprocessDescriptorSets;
    VkPipelineLayout PostprocessPipelineLayout;
    VkPipeline PostprocessPipeline;

    std::vector<VkFence> DrawingFence;
    std::vector<VkSemaphore> ImageAvaliable;
    std::vector<VkSemaphore> FluidsRenderingFinish;
    std::vector<VkSemaphore> BoxRenderingFinish;
    std::vector<VkSemaphore> CubesRenderingFinish;
    std::vector<VkSemaphore> SimulatingFinish;

    // Uniform Buffers
    VkBuffer UniformRenderingBuffer;
    VkDeviceMemory UniformRenderingBufferMemory;
    void* MappedRenderingBuffer;

    VkBuffer UniformSimulatingBuffer;
    VkDeviceMemory UniformSimulatingBufferMemory;
    void* MappedSimulatingBuffer;

    VkBuffer UniformBoxInfoBuffer;
    VkDeviceMemory UniformBoxInfoBufferMemory;
    void* MappedBoxInfoBuffer;

    VkBuffer cubeUniformBuffer;
    VkDeviceMemory cubeUniformBufferMemory;
    void* mappedCubeBuffer;

    // Custom Image
    VkImage ThickImage;
    VkDeviceMemory ThickImageMemory;
    VkImageView ThickImageView;
    VkSampler ThickImageSampler;

    VkImage DepthImage;
    VkDeviceMemory DepthImageMemory;
    VkImageView DepthImageView;
    VkSampler DepthImageSampler;

    VkImage CustomDepthImage;
    VkDeviceMemory CustomDepthImageMemory;
    VkImageView CustomDepthImageView;
    VkSampler CustomDepthImageSampler;

    VkImage FilteredDepthImage;
    VkDeviceMemory FilteredDepthImageMemory;
    VkImageView FilteredDepthImageView;
    VkSampler FilteredDepthImageSampler;

    VkImage DefaultTextureImage;
    VkDeviceMemory DefaultTextureImageMemory;
    VkImageView DefaultTextureImageView;
    VkSampler DefaultTextureImageSampler;

    VkImage BackgroundImage;
    VkDeviceMemory BackgroundImageMemory;
    VkImageView BackgroundImageView;
    VkSampler BackgroundImageSampler;

    VkImage ObjTextureImage;
    VkDeviceMemory ObjTextureImageMemory;
    VkImageView ObjTextureImageView;
    VkSampler ObjTextureImageSampler;

    // extra image for two-phase coupling
    VkImage CustomDepthImage2;
    VkDeviceMemory CustomDepthImageMemory2;
    VkImageView CustomDepthImageView2;
    VkSampler CustomDepthImageSampler2;

    VkImage FilteredDepthImage2;
    VkDeviceMemory FilteredDepthImageMemory2;
    VkImageView FilteredDepthImageView2;
    VkSampler FilteredDepthImageSampler2;

    VkImage ThickImage2;
    VkDeviceMemory ThickImageMemory2;
    VkImageView ThickImageView2;
    VkSampler ThickImageSampler2;

    VkImage CausticsImage;
    VkDeviceMemory CausticsImageMemory;
    VkImageView CausticsImageView;
    VkSampler CausticsImageSampler;

    VkImage CausticsImage2;
    VkDeviceMemory CausticsImageMemory2;
    VkImageView CausticsImageView2;
    VkSampler CausticsImageSampler2;

    VkFramebuffer FluidsFramebuffer;
    VkFramebuffer BoxFramebuffer;
    VkFramebuffer FluidsFramebuffer2;

    std::vector<VkBuffer> ParticleBuffers;
    std::vector<VkDeviceMemory> ParticleBufferMemory;
    std::vector<VkBuffer> DiffuseParticleBuffers;
    std::vector<VkDeviceMemory> DiffuseParticleBufferMemory;
    std::vector<VkBuffer> CubeParticleBuffers;
    std::vector<VkDeviceMemory> CubeParticleBufferMemory;
    std::vector<VkBuffer> RigidParticleBuffers;
    std::vector<VkDeviceMemory> RigidParticleBufferMemory;
    std::vector<VkBuffer> OriginRigidParticleBuffers;
    std::vector<VkDeviceMemory> OriginRigidParticleBufferMemory;
    std::vector<VkBuffer> ShapeMatchingBuffers;
    std::vector<VkDeviceMemory> ShapeMatchingBufferMemory;
    std::vector<VkBuffer> DiffuseParticleCountBuffers;
    std::vector<VkDeviceMemory> DiffuseParticleCountBufferMemory;
    // is anisotrophic ?
    std::vector<VkBuffer> AnisotrophicBuffers;
    std::vector<VkDeviceMemory> AnisotrophicBufferMemory;

    // Merge Sort Buffer
    VkBuffer CellInfoBuffer;
    VkDeviceMemory CellInfoBufferMemory;
    VkBuffer NeighborCountBuffer;
    VkDeviceMemory NeighborCountBufferMemory;
    VkBuffer NeighborCpyCountBuffer;
    VkDeviceMemory NeighborCpyCountBufferMemory;
    VkBuffer ParticleInCellBuffer;
    VkDeviceMemory ParticleInCellBufferMemory;
    VkBuffer CellOffsetBuffer;
    VkDeviceMemory CellOffsetBufferMemory;
    VkBuffer TestPBFBuffer;
    VkDeviceMemory TestPBFBufferMemory;
    VkBuffer SurfaceParticleBuffer;
    VkDeviceMemory SurfaceParticleBufferMemory;

    // SDF Buffer and Dynamic SDF Buffer
    VkBuffer SDFBuffer;
    VkDeviceMemory SDFBufferMemory;
    VkBuffer SDFDyBuffer;
    VkDeviceMemory SDFDyBufferMemory;

    VkBuffer WCSPHPressureBuffer = VK_NULL_HANDLE;
    VkDeviceMemory WCSPHPressureBufferMemory = VK_NULL_HANDLE;
    VkBuffer WCSPHExternalForceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory WCSPHExternalForceBufferMemory = VK_NULL_HANDLE;
    VkBuffer WCSPHParamsBuffer = VK_NULL_HANDLE;
    VkDeviceMemory WCSPHParamsBufferMemory = VK_NULL_HANDLE;
    void* mappedWCSPHParamsBuffer = nullptr;

    // Cubes
    VkBuffer BoxVertexBuffer;
    VkDeviceMemory BoxVertexBufferMemory;

    std::vector<VkCommandBuffer> SimulatingCommandBuffers;
    std::vector<VkCommandBuffer> FluidsRenderingCommandBuffers[2];
    std::vector<VkCommandBuffer> RigidRenderingCommandBuffers[2];
    VkCommandBuffer BoxRenderingCommandBuffer;

    // Ellipsoid
    EllipsoidInfo ellipsoidInfo;
    glm::vec3 ellipCenter = { 1.0, 0.1, 0.6 };
    glm::vec3 ellipRadii = { 0.2, 0.074, 0.1 };
    std::vector<VkBuffer> ellipsoidBuffers;
    std::vector<VkDeviceMemory> ellipsoidBufferMemory;
    
    // Obj / Skybox / Sphere Resources 
    ObjResource obj_resource;
    ObjResource obj_r2;

    std::vector<ObjResManagement> objResManagements;
    SkyboxResource skybox_resource;
    SphereResource sphere_resource;
    
    SceneSet* scene_set;

    void CreateObjModelResources(std::string &obj_path, ObjResource& objRes, glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useParticle, bool useVoxelBox, bool useGlb);
    void CreateUnifiedObjModel();
public:
    void SetRenderingObj(const UniformRenderingObject& robj);
    void SetSimulatingObj(const UniformSimulatingObject& sobj);
    void SetBoxinfoObj(const UniformBoxInfoObject& bobj);
    void SetParticles(const std::vector<Particle>& ps);
    void SetCubeParticles(const std::vector<CubeParticle>& ps);
    void SetRigidParticles(const std::vector<ExtraParticle>& ps);
    void SetShapeMatching(const ShapeMatchingInfo& s);
    void SetCells(const uint32_t& cellCount);
    void SetFPS(float fps);
    void SetValues(const bool& value);
    void SetMPM2(const bool& value);
    void SetMPM2Incompressible(const bool& value);
    void SetWCSPH(const bool& value);
    void SetSPHVulkan(const bool& value);

    // SDF about
    void StartSDF(ObjResource* objRes, std::vector<uint32_t>& voxels, std::vector<float>& sdfValues);
    void SDFUpdateResources(std::vector<float> sdfValues, glm::vec3 sdfRangeMin, float voxelSize);

    // others
    void UpdateUniformSimulatingBuffer();
    void UpdateSimulateDescriptorSetForCubeParticles();

    void GenerateWaterStream(float xStart, float yStart, float zStart);
    void GenerateObjWater();
    void DeleteCubeParticles();

    // to .ply
    void ExportSimRenderData();

    void ExportSDFField(size_t objIndex, const std::string& outDir = "./sdf");

private:
    bool Initialized = false;
    uint32_t Width;
    uint32_t Height;
    std::vector<Particle> particles;                  // fluid particles
    std::vector<DiffuseParticle> diffuseParticles;    // splash
    std::vector<CubeParticle> cubeParticles;          // obj particles
    std::vector<CubeParticle> cubeCpyParticles;       // obj copy particles :  don't create it's buffer, just store data
    std::vector<ExtraParticle> rigidParticles;        // rigid particles
    std::vector<uint32_t> rigidNums;
    std::vector<ExtraParticle> originRigidParticles;  // store rest rigid particles

    std::vector<AnisotrophicMatrix> anisotrophicMatrices;
    uint32_t cellCount;

    UniformRenderingObject renderingobj{};
    UniformSimulatingObject simulatingobj{};
    UniformBoxInfoObject boxinfobj{};
    ShapeMatchingInfo smi{};
    std::vector<ShapeMatchingInfo> smiVec;            // multi rigids
    DiffuseParticleCount dpc{};

    bool bEnableValidation = false;
    uint32_t CurrentFlight = 0;
    uint32_t MAXInFlightRendering = 2;

    uint32_t ONE_GROUP_INVOCATION_COUNT = 256;
    uint32_t WORK_GROUP_COUNT;
    uint32_t WORK_GROUP_COUNT_DIFFUSE;

    uint32_t MAX_NGBR_NUM = 90;
    bool bFramebufferResized = false;

    float fps;

    bool middleMousePressed = false;                           
    bool rightMousePressed = false;
    bool isEmitFluids = true;
    bool isApplyForce = false;

    double lastX = 0.0, lastY = 0.0;                          
    float panSpeed = 0.005f;                                  
    float zoomSpeed = 0.1f;                                   
    float rotateSpeed = 0.1f;                                   

    glm::vec3 mousePos = glm::vec3(0.0f, 0.0f, 0.0f);
    uint32_t attract = 0;

    glm::vec3 cameraPos = glm::vec3(0.8f, 0.7f, 1.7f);
    glm::vec3 cameraTarget = glm::vec3(0.8f, 0.3f, 0.3f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);           

    glm::vec3 cameraPos_ = glm::vec3(0.8f, 0.7f, 1.7f);
    glm::vec3 cameraTarget_ = glm::vec3(0.8f, 0.3f, 0.3f);
    glm::vec3 cameraUp_ = glm::vec3(0.0f, 1.0f, 0.0f);           
    float colorWithAlpha[4];

    int export_sum_times = 1000;
    int current_time = 0;

    // control special particles generating
    bool specialParticleGenerate = false;
    SpecialParticle* specialParticle;

    // voxelize once   sdf once
    bool isVoxelize = false;
    bool isSDF = true;
    bool isSDF2 = true;
    SDF* sdf;
    std::vector<VkCommandBuffer*> sdfCommandBuffers;

    // fluid particles anisotrophic
    bool startAnisotrophic = false;

    // fluid foam or bubbles (white particles)
    bool startDiffuseParticles = false;

    bool injectorEnabled = false;
    int injectorSide = 0;
    float injectorRadius = 0.08f;
    float injectorRate = 4000.0f;
    float injectorSpeed = 2.0f;
    float injectorCenterY = 0.6f;
    float injectorCenterZ = 0.6f;
    float injectorAccumulator = 0.0f;

    // delete cube particles and buffer
    bool isDeleteCubeParticles = false;

    // rigid cube redraw  (particles to cube)
    bool rigidCubeGenerate = false;
    RigidCube* rigidCube;

    // cloth draw
    bool clothGenerate = false;
    Cloth* cloth = nullptr;

    // other algorithms
    // 1. MPM  2. WCSPH  3.PCISPH  4.DFSPH  5. LBM  6. PBD  ...... FEM ... 
    MPMSim* mpm = nullptr;
    MPM2Sim* mpm2 = nullptr;
    bool mpmAlgorithm = true;
    bool mpm2Algorithm = false;
    bool mpm2Incompressible = false;
    bool wcsphAlgorithm = false;
    bool sphVulkanAlgorithm = false;
};
#endif