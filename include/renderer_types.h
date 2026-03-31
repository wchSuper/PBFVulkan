#ifndef RENDERER_TYPES_H
#define RENDERER_TYPES_H
#define GLFW_INCLUDE_VULKAN
#include"GLFW/glfw3.h"
#include"glm/glm.hpp"
#include "tiny_obj_loader.h"

#include<vector>
#include<array>
#include<optional>
 #include<string>
#define VOXEL_RES 100
#define DRAGON_VOXEL_RES 64
#define VOXEL_MARGIN 0.08

struct QueuefamliyIndices{
    std::optional<uint32_t> graphicNcompute;
    std::optional<uint32_t> present;
    bool IsCompleted(){
        return graphicNcompute.has_value()&&present.has_value();
    }
};

struct SurfaceDetails{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentmode;
    
};

enum class TickWindowResult{
    NONE,
    HIDE,
    EXIT,
};

enum class SceneType {
    TABLE,
    STONE,
    SEA,
    PBRTEST,
    FLEX,
    SWIMMINGPOOL
};


// *************************************************************** Vertex Input ********************************************** //
struct AnisotrophicMatrix {
    alignas(16) glm::vec3 aniMtCol0;
    alignas(16) glm::vec3 aniMtCol1;
    alignas(16) glm::vec3 aniMtCol2;
};

struct Particle{
    alignas(16) glm::vec3 Location;
    alignas(16) glm::vec3 Velocity;
    alignas(16) glm::vec3 DeltaLocation;
    alignas(4) float Lambda;
    alignas(4) float Density;
    alignas(4) float Mass;

    alignas(16) glm::vec3 TmpVelocity;

    alignas(4) uint32_t CellHash;
    alignas(4) uint32_t TmpCellHash;
    alignas(4) uint32_t NumNgbrs;

    static std::vector<VkVertexInputBindingDescription> GetBindings() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        
        // particle binding
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(Particle);      
        return bindings;
    } 
    static std::vector<VkVertexInputAttributeDescription> GetAttributes(){
        std::vector<VkVertexInputAttributeDescription> attributes(1);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Particle,Location);
        return attributes;
    }
};

struct DiffuseParticle {
    alignas(16) glm::vec3 Location;
    alignas(16) glm::vec3 Velocity;

    alignas(4) float Mass;
    alignas(4) float DiffuseBuoyancy;
    alignas(4) float LifeTime;
    alignas(4) uint32_t Type;

    static std::vector<VkVertexInputBindingDescription> GetBindings() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(Particle);
        return bindings;
    }
    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(1);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Particle, Location);
        return attributes;
    }
};

struct ParticleAni {
    alignas(16) glm::vec3 Location;
    alignas(16) glm::vec3 Velocity;
    alignas(16) glm::vec3 DeltaLocation;
    alignas(4) float Lambda;
    alignas(4) float Density;
    alignas(4) float Mass;

    alignas(16) glm::vec3 TmpVelocity;

    alignas(4) uint32_t CellHash;
    alignas(4) uint32_t TmpCellHash;

    alignas(4) uint32_t NumNgbrs;

    static std::vector<VkVertexInputBindingDescription> GetBinding() {
        std::vector<VkVertexInputBindingDescription> bindings(2);

        // particle binding - instanced per particle
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        bindings[0].stride = sizeof(Particle);

        // anisotrophic binding - instanced per particle
        bindings[1].binding = 1;
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        bindings[1].stride = sizeof(AnisotrophicMatrix);

        return bindings;
    }
    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(4);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Particle, Location);

        attributes[1].binding = 1;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(AnisotrophicMatrix, aniMtCol0);

        attributes[2].binding = 1;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[2].offset = offsetof(AnisotrophicMatrix, aniMtCol1);

        attributes[3].binding = 1;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[3].offset = offsetof(AnisotrophicMatrix, aniMtCol2);
        return attributes;
    }
};

// 
struct CubeParticle {
    alignas(16) glm::vec3 Location;
    alignas(16) glm::vec3 Velocity;
    alignas(16) glm::vec3 DeltaLocation;
    alignas(4) float Lambda;
    alignas(4) float Density;
    alignas(4) float Mass;

    alignas(16) glm::vec3 TmpVelocity;

    alignas(4) uint32_t CellHash;
    alignas(4) uint32_t TmpCellHash;

    alignas(4) uint32_t NumNgbrs;

    static std::vector<VkVertexInputBindingDescription> GetBinding() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(CubeParticle);
        return bindings;
    }
    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(1);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(CubeParticle, Location);
        return attributes;
    }
};

struct ObjVertex {
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec3 normal;
    alignas(8)  glm::vec2 texCoord;
    alignas(16) glm::vec4 color;

    static std::vector<VkVertexInputBindingDescription> GetBinding() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(ObjVertex);
        return bindings;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(4);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(ObjVertex, pos);

        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(ObjVertex, normal);

        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(ObjVertex, texCoord);

        attributes[3].binding = 0;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[3].offset = offsetof(ObjVertex, color);

        return attributes;
    }
};

// Skybox
struct SkyboxVertex {
    alignas(16) glm::vec3 pos;

    static std::vector<VkVertexInputBindingDescription> GetBinding() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(SkyboxVertex);
        return bindings;
    }
    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(1);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(SkyboxVertex, pos);
        return attributes;
    }
};

// Sphere
struct SphereVertex {
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec3 normal;
    alignas(8)  glm::vec2 texCoord;

    static std::vector<VkVertexInputBindingDescription> GetBinding() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(SphereVertex);
        return bindings;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(3);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(SphereVertex, pos);

        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(SphereVertex, normal);

        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(SphereVertex, texCoord);

        return attributes;
    }
};

// Cloth
struct ClothPoint {
    alignas(16) glm::vec4 pos;
    alignas(16) glm::vec4 vel;
    alignas(16) glm::vec4 uv;
    alignas(16) glm::vec4 normal;
    alignas(16) glm::vec4 deltaPos;

    static VkVertexInputBindingDescription GetBinding() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        binding.stride = sizeof(ClothPoint);
        return binding;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(3);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[0].offset = offsetof(ClothPoint, pos);

        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[1].offset = offsetof(ClothPoint, uv);

        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[2].offset = offsetof(ClothPoint, normal);
        return attributes;
    }
};

// Cylinder
struct CylinderVertex {
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec3 normal;
    alignas(8)  glm::vec2 texCoord;
    alignas(16) glm::vec4 color;

    static std::vector<VkVertexInputBindingDescription> GetBinding() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(CylinderVertex);
        return bindings;
    }

    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(4);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(CylinderVertex, pos);

        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(CylinderVertex, normal);

        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(CylinderVertex, texCoord);

        attributes[3].binding = 0;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[3].offset = offsetof(CylinderVertex, color);

        return attributes;
    }
};


// ************************************** Graphic or Compute shaders binding Struct ****************************************** //
struct Voxel {
    glm::vec3 pos;
    glm::ivec3 idx;
};

struct UniformVoxelObject {
    alignas(16) glm::vec3 boxMin;
    alignas(16) glm::ivec3 resolution;
    alignas(4) float voxelSize;
};


struct ExtraParticle {
    alignas(16) glm::vec3 Location;
    alignas(16) glm::vec3 Velocity;
    alignas(16) glm::vec3 DeltaLocation;

    alignas(4) float Lambda;
    alignas(4) float Density;
    alignas(4) float Mass;

    alignas(16) glm::vec3 TmpVelocity;

    alignas(4) uint32_t CellHash;
    alignas(4) uint32_t TmpCellHash;

    alignas(4) uint32_t NumNgbrs;
    alignas(4) uint32_t type;

    static std::vector<VkVertexInputBindingDescription> GetBinding() {
        std::vector<VkVertexInputBindingDescription> bindings(1);
        bindings[0].binding = 0;
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings[0].stride = sizeof(ExtraParticle);
        return bindings;
    }
    static std::vector<VkVertexInputAttributeDescription> GetAttributes() {
        std::vector<VkVertexInputAttributeDescription> attributes(1);
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(ExtraParticle, Location);
        return attributes;
    }
};

struct ShapeMatchingInfo {
    alignas(16) glm::vec3 CM0;
    alignas(16) glm::vec3 CM;

    alignas(16) glm::vec3 Apq_col0;
    alignas(16) glm::vec3 Apq_col1;
    alignas(16) glm::vec3 Apq_col2;

    alignas(16) glm::vec3 Q_col0;
    alignas(16) glm::vec3 Q_col1;
    alignas(16) glm::vec3 Q_col2;

    alignas(16) glm::vec3 S_col0;
    alignas(16) glm::vec3 S_col1;
    alignas(16) glm::vec3 S_col2;
};

struct DiffuseParticleCount {
    alignas(4) uint32_t surfaceParticleCount;
    alignas(4) uint32_t activeCount;
    alignas(4) uint32_t compactCount;
};

// ==================== 光源结构体 ====================
struct LightSource {
    alignas(16) glm::vec3 position;   
    alignas(16) glm::vec3 color;      
    alignas(4) float intensity;       
    alignas(4) uint32_t type;         
    alignas(4) uint32_t enabled;      
    alignas(4) float _padding;        
    LightSource() : position(2.0f, 3.0f, 2.0f), color(1.0f, 1.0f, 1.0f),
                    intensity(1.0f), type(0), enabled(1), _padding(0.0f) {}
};

struct UniformRenderingObject{
    alignas(4) float zNear;
    alignas(4) float zFar;
    alignas(4) float fovy;
    alignas(4) float aspect;

    alignas(16) glm::mat4 model = glm::mat4(1.0f);
    alignas(16) glm::mat4 view = glm::mat4(1.0f);
    alignas(16) glm::mat4 projection = glm::mat4(1.0f);
    alignas(16) glm::mat4 inv_projection = glm::mat4(1.0f);
    alignas(16) glm::mat4 inv_view = glm::mat4(1.0f);  

    alignas(4) float particleRadius;
    alignas(4) float rigidRadius;
    alignas(4) uint32_t renderType;
    alignas(4) uint32_t fluidType;
    alignas(16) glm::vec4 fluid_color = glm::vec4(0.529f, 0.808f, 0.922f, 0.5);
    alignas(16) glm::vec3 camera_pos;
    alignas(16) glm::vec3 camera_view;
    alignas(16) glm::vec3 camera_up;

    // ==================== 统一光照系统 (最多3个光源) ====================
    alignas(16) LightSource lights[3];                               
    alignas(4) uint32_t numActiveLights = 1;                         
    
    alignas(16) glm::vec3 ambientColor = glm::vec3(0.15f, 0.15f, 0.15f);
    
    alignas(4) float causticsIntensity = 0.15f;      
    alignas(4) float causticsPhotonRadius = 15.0f;   
    
    alignas(4) float refractionIOR = 1.33f;   
    alignas(4) float redIOR = 1.331f;      
    alignas(4) float greenIOR = 1.333f; 
    alignas(4) float blueIOR = 1.336f; 

    alignas(4) bool isFilter;
    alignas(4) uint32_t filterRadius;
};

struct UniformSimulatingObject{
    alignas(4) float dt;
    alignas(4) float accumulated_t;
    alignas(4) float restDensity;
    alignas(4) float sphRadius;

    alignas(4) float coffPoly6;
    alignas(4) float coffSpiky;
    alignas(4) float coffGradSpiky;

    alignas(4) float scorrK;
    alignas(4) float scorrN;
    alignas(4) float scorrQ;

    alignas(4) float spaceSize; 
    alignas(4) float fluidParticleRadius;
    alignas(4) float cubeParticleRadius;
    alignas(4) float rigidParticleRadius;

    alignas(4) uint32_t workGroupCount;
    alignas(4) uint32_t gridCount;
    alignas(4) uint32_t testData1;
    alignas(4) uint32_t numParticles;      // 
    alignas(4) uint32_t maxParticles;      // | ...numParticles... | ... cubeParticleNum ... | ... rigidParticles ... |
    alignas(4) uint32_t maxCubeParticles;  //                   maxParticles             maxCubeParticles
    alignas(4) uint32_t cubeParticleNum;   // 
    alignas(4) uint32_t rigidParticles { 0 };    // 
    alignas(4) uint32_t maxNgbrs;          // 

    alignas(4) float visc;                 // viscosity
    alignas(4) float psi;                  // 
    alignas(4) float relaxation;
    
    // Artificial Viscosity Parameters
    alignas(4) uint32_t enableArtificialViscosity;  // 是否启用粒子间人工粘性 (0=off, 1=on)
    alignas(4) float artificialViscosityAlpha;      // 人工粘性系数 a (推荐: 0.01-0.1)
    alignas(4) float artificialViscosityBeta;       // 人工粘性系数 b (推荐: 0.0)
    
    // Surface Tension Parameters
    alignas(4) uint32_t enableSurfaceTension;       // 是否启用表面张力 (0=off, 1=on)
    alignas(4) float surfaceTensionCoefficient;     // 表面张力系数 (推荐: 0.001-0.1)
    alignas(4) float surfaceTensionThreshold;       // 表面张力阈值 (推荐: 0.1-1.0)
    
    // Anisotropic Kernel Parameters (Yu & Turk 2013)
    alignas(4) float anisotropicBaseRadiusScale;              // 基础半径缩放系数 (推荐: 1.5)
    alignas(4) float anisotropicMaxRatio;                     // 最大各向异性比 (推荐: 3.0-6.0)
    alignas(4) float anisotropicMinScale;                     // 最小半径缩放 (推荐: 0.2-0.5)
    alignas(4) float anisotropicMaxScale { 1.0f };            // 最大半径缩放 (推荐: 1.0)
    alignas(4) float anisotropicNeighborThreshold { 6.0f };   // 邻居数阈值 (推荐: 4-10)
    
    alignas(4)  uint32_t attract;
    alignas(16) glm::vec3 mouse_pos ;
    alignas(4)  float k;                    // external force
    alignas(16) glm::vec3 boundary_min;

    // sdf about 
    alignas(16) glm::vec3 sdfRangeMin;
    alignas(4)  uint32_t voxelResolution;
    alignas(4)  float voxelSize;

    alignas(16) glm::vec3 sdfRangeMinDy;         // dynamic sdf
    alignas(4)  uint32_t voxelResolutionDy;
    alignas(4)  float voxelSizeDy; 

    alignas(4)  bool isSDF;

    // foam about
    alignas(4) uint32_t maxDiffuseParticles;
    alignas(4) float splashThreshold;
    alignas(4) float surfaceThreshold;
    alignas(4) float foamThreshold;
    alignas(4) float lifeTime;
    
    // Dynamic Wave Simulation Parameters (Gerstner Wave + Wind Driven)
    alignas(4) uint32_t waveEnabled;             // 是否启用动态波浪 (0=off, 1=on)
    alignas(4) float waveIntensity;              // 波浪总体强度 (推荐: 0.5-2.0)
    alignas(4) float waveSteepness;              // 波浪陡峭度/Gerstner Q值 (推荐: 0.3-0.8)
    alignas(4) float waveBaseHeight;             // 水面基准高度 (用于判断表面粒子)
    alignas(4) float windDirX;                   // 风向X分量 (归一化)
    alignas(4) float windDirZ;                   // 风向Z分量 (归一化)
    alignas(4) float windStrength;               // 风力强度 (推荐: 0.1-1.0)
    alignas(4) float wavePeriod;                 // 主波浪周期 (推荐: 1.0-3.0秒)
};

struct UniformBoxInfoObject{
    alignas(8) glm::vec2 clampX;
    alignas(8) glm::vec2 clampY;
    alignas(8) glm::vec2 clampZ; 

    alignas(8) glm::vec2 clampX_still;
    alignas(8) glm::vec2 clampY_still;
    alignas(8) glm::vec2 clampZ_still; 
};

struct PushConstantData {
    int layer_num;
};

struct CubeInfo {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 size;
    alignas(16) glm::vec4 rotation;  
    alignas(16) glm::vec3 velocity;
};

struct EllipsoidInfo {
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec3 center;
    alignas(16) glm::vec3 radii;
    alignas(16) glm::vec3 accumulatedForce;
    alignas(16) glm::vec3 accumulatedTorque;
    alignas(16) glm::vec3 velocity;
    alignas(16) glm::vec3 angularVelocity;
    alignas(16) glm::vec4 orientation;
};

// ******************************************** Obj Resource *************************************************** //
struct ObjTransform {
    alignas(16) glm::mat4 objModel = glm::mat4(1.0f);
};

// 1. Obj     from outer .obj model
// 2. Skybox  from outer 6-images
// 3. Sphere  inner math drawing

// Obj Struct   rely on background image
struct ObjResource {
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

    // texture 
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

    VkImage textureImage2 = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory2 = VK_NULL_HANDLE;
    VkImageView textureImageView2 = VK_NULL_HANDLE;
    VkSampler textureSampler2 = VK_NULL_HANDLE;

    // textures for PBR
    std::vector<VkImage> textureImagePBR;
    std::vector<VkDeviceMemory> textureImageMemoryPBR;
    std::vector<VkImageView> textureImageViewPBR;
    std::vector<VkSampler> textureSamplerPBR;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    VkBuffer voxelBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelBufferMemory = VK_NULL_HANDLE;

    UniformVoxelObject voxinfobj{};
    VkBuffer UniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory UniformBufferMemory = VK_NULL_HANDLE;
    void* MappedBuffer = nullptr;

    ObjTransform transformobj{};
    VkBuffer UniformBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory UniformBufferMemory_ = VK_NULL_HANDLE;
    void* MappedBuffer_ = nullptr;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicPipeline = VK_NULL_HANDLE;
};

struct ObjResManagement {
    ObjResource* obj_res = nullptr;
    bool isVox = false;
    bool isTex = false;
    bool isGlb = false;

    bool is2Tex = false;
    bool isPBR = false;
    
    bool isTransform { false };
    bool isCustomEllipsode { false };

    std::string displayName;
};

struct SkyboxResource {
    //std::vector<SkyboxVertex> vertices;    
    //std::vector<uint32_t> indices;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    VkImage skyboxImage = VK_NULL_HANDLE;
    VkDeviceMemory skyboxImageMemory = VK_NULL_HANDLE;
    VkImageView skyboxImageView = VK_NULL_HANDLE;
    VkSampler skyboxSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicPipeline = VK_NULL_HANDLE;
};

struct SphereResource {
    std::vector<SphereVertex> vertices;
    std::vector<uint32_t> indices;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicPipeline = VK_NULL_HANDLE;
};

// ----------------------------------------------------- model viewer --------------------------------------------------------- //
struct UniformModelRendering {
    alignas(4) float zNear;
    alignas(4) float zFar;
    alignas(4) float fovy;
    alignas(4) float aspect;

    alignas(16) glm::mat4 model = glm::mat4(1.0f);
    alignas(16) glm::mat4 view = glm::mat4(1.0f);
    alignas(16) glm::mat4 projection = glm::mat4(1.0f);
    alignas(16) glm::mat4 inv_projection = glm::mat4(1.0f);
};

#endif