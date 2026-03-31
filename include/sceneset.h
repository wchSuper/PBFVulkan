#pragma once
#include <vector>
#include <map>
#include "vulkan/vulkan.h"
#include "renderer_types.h"
#include "extensionfuncs.h"
#include "tiny_obj_loader.h"
#include <iostream>
#include <variant>
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "utils.hpp"
#include "buffer.hpp"
#include "image.hpp"
#include "scene.hpp"
#include "tiny_gltf.h"
#define Allocator nullptr

#define M_PI 3.1415926

class SceneSet {
	
private:
	VkPhysicalDevice PDevice;
	VkDevice LDevice;
	VkCommandPool CommandPool;
	VkQueue GraphicNComputeQueue;

	glm::vec3 boundryMin;
	glm::vec3 boundryMax;

	const std::string CUP_PATH = "resources/models/cup2.obj";
	const std::string DRAGON_PATH = "resources/models/dragon.obj";
	const std::string CYLINDER_PATH = "resources/models/cylinder.obj";
	const std::string TABLE_PATH = "resources/models/table.obj";
	const std::string TABLE_PATH_GLB = "resources/models/table.glb";
	const std::string SAND_PATH_GLB = "resources/models/sand.glb";
	const std::string SAND_PATH_TEX = "resources/textures/model_textures/sand.jpg";
	const std::string WALL_PATH_GLB = "resources/models/wall.glb";
	const std::string WALL_PATH_TEX = "resources/textures/model_textures/wall.jpg";
	const std::string ROCK1_PATH_GLB = "resources/models/rock1.glb";
	const std::string ROCK1_PATH_TEX = "resources/textures/model_textures/rock1.jpg";
	const std::string ROCK2_PATH_GLB = "resources/models/rock2.glb";
	const std::string ROCK2_PATH_TEX = "resources/textures/model_textures/rock2.jpg";

	// sea
	const std::string GROUND_PATH_GLB = "resources/models/ground.glb";
	const std::string GROUND_PATH_TEX = "resources/textures/model_textures/ground.jpg";
	const std::string GROUND2_PATH_TEX = "resources/textures/model_textures/ground2.jpg";
	const std::string GROUND3_PATH_TEX = "resources/textures/model_textures/ground3.jpg";
	const std::string SEA_MOUNTAIN_PATH_GLB = "resources/models/mountain.glb";
	const std::string SEA_MOUNTAIN_PATH_TEX = "resources/textures/model_textures/mountain.jpg";
	const std::string SEA_LANDSCAPE_PATH_GLB = "resources/models/landscape.glb";
	const std::string SEA_LANDSCAPE_PATH_TEX = "resources/textures/model_textures/landscape.jpg";

	// PBR test
	const std::string APPLE_PATH_OBJ = "resources/models/apple.obj";
	const std::string APPLE_PATH_TEX_COLOR = "resources/textures/model_textures/apple_textures/apple_BaseColor.jpg";
	const std::string APPLE_PATH_TEX_NORMALGL = "resources/textures/model_textures/apple_textures/apple_NormalGL.jpg";
	const std::string APPLE_PATH_TEX_AO = "resources/textures/model_textures/apple_textures/apple_AmbientOcclusion.jpg";
	const std::string HELI_PATH_OBJ = "resources/models/helicopter.obj";
	const std::string HELI_PATH_TEX_COLOR = "resources/textures/model_textures/helicopter_textures/heli_BaseColor.png";
	const std::string HELI_PATH_TEX_NORMAL = "resources/textures/model_textures/helicopter_textures/heli_Normal.png";
	const std::string HELI_PATH_TEX_METALLICROUGHNESS = "resources/textures/model_textures/helicopter_textures/heli_MetallicRoughness.png";

	// FleX scene
	const std::string COLLISION0_GLB = "resources/models/collision0.glb";
	const std::string COLLISION0_TEX = "resources/textures/model_textures/collision0.jpg";
	const std::string PLANE_TEX = "resources/textures/default_texture3.png";

	const std::string CAT_GLB       = "resources/models/cat.glb";
	const std::string CAT_BASECOLOR = "resources/textures/model_textures/concrete_textures/concrete_cat_statue_diff_1k.jpg";
	const std::string CAT_NORMAL    = "resources/textures/model_textures/concrete_textures/concrete_cat_statue_nor_gl_1k.jpg";
	const std::string CAT_ROUGHNESS = "resources/textures/model_textures/concrete_textures/concrete_cat_statue_rough_1k.jpg";
	const std::string CAT_METAL     = "resources/textures/model_textures/concrete_textures/concrete_cat_statue_metal_1k.jpg";
	const std::string CAT_AO        = "resources/textures/model_textures/concrete_textures/concrete_cat_statue_ao_1k.jpg";
	const std::string CAT_DISPLACE  = "resources/textures/model_textures/concrete_textures/concrete_cat_statue_disp_1k.jpg";

	// swimming pool
	const std::string POOL_GLB = "resources/models/pool.glb";
	const std::string POOL_BOTTOM = "resources/textures/model_textures/pool_textures/pool_bottom.jpg";
	const std::string POOL_GROUND_GLB = "resources/models/pool_ground.glb";
	const std::string POOL_GROUND_TEX = "resources/textures/model_textures/pool_textures/pool_ground.jpg";
	const std::string POOL_LADDER_GLB = "resources/models/ladder.glb";
	const std::string POOL_LADDER_TEX = "resources/textures/model_textures/pool_textures/ladder.jpg";
	const std::string CHAISE_OBJ = "resources/models/chaise.obj";
	const std::string CHAISE_TEX = "resources/textures/model_textures/pool_textures/chaise.png";
	const std::string UMBRELLA_OBJ = "resources/models/umbrella.obj";
	const std::string UMBRELLA_TEX = "resources/textures/model_textures/pool_textures/umbrella.png";
	const std::string DUCKFLOAT_GLB = "resources/models/disk.glb";
	const std::string DUCKFLOAT_TEX = "resources/textures/model_textures/disk_textures/WindmillTiles04_4K_BaseColor.png";
	// ply file
	const std::string FORKLIFT_PLY = "resources/models/mesh_integration_sdf.ply";
	// the second obj needed sdf
	const std::string SLIDE_GLB = "resources/models/slide.glb";
	// can move rotate 
	const std::string WATERWHEEL_GLB = "resources/models/waterwheel.glb";
	
	const std::string BOAT_GLB = "resources/models/boat.glb";

	ObjResource obj_resource;
	ObjResource obj_r2;
	ObjResource cylinder;
	ObjResource table;
	ObjResource sand;
	ObjResource wall;
	ObjResource rock1;
	ObjResource rock2;

	// sea
	ObjResource ground;
	ObjResource mountain;
	ObjResource landscape;

	// PBR test
	ObjResource apple;
	ObjResource helicopter;

	// FleX model
	ObjResource collision0;
	ObjResource cat;
	ObjResource plane;
	ObjResource plane_left;
	ObjResource plane_right;
	ObjResource plane_front;
	ObjResource plane_back;

	// swimming pool
	ObjResource pool;
	ObjResource pool_ground;
	ObjResource ladder1;
	ObjResource ladder2;
	ObjResource chaise1;
	ObjResource chaise2;
	ObjResource umbrella;
	ObjResource duck_float;
	ObjResource slide;
	ObjResource fork_lift;
	ObjResource water_wheel;
	ObjResource ellipsoid;
	ObjResource boat;


	// model assets : shader(.vert .frag) paths
	std::map<ObjResource*, std::array<std::string, 2>> objResPaths;

	// .vert .frag paths
	std::string base_path = "resources/shaders/spv/";
	std::vector<std::string> obj_unified_paths = { base_path + "obj_unified.vert.spv", base_path + "obj_unified.frag.spv" };
	std::vector<std::string> obj_unified_tex_paths = { base_path + "obj_unified_tex.vert.spv", base_path + "obj_unified_tex.frag.spv" };
	std::vector<std::string> obj_unified_vox_paths = { base_path + "obj_unified_vox.vert.spv", base_path + "obj_unified_vox.frag.spv" };
	std::vector<std::string> obj_unified_tex_origin_paths = { base_path + "obj_unified_tex.vert.spv", base_path + "obj_unified_tex_origin.frag.spv" };


public:
	SceneSet() {};

	SceneSet(VkPhysicalDevice& pDevice, VkDevice& lDevice, VkCommandPool& commandPool, VkQueue& graphicQueue) :
		PDevice(pDevice), LDevice(lDevice), CommandPool(commandPool), GraphicNComputeQueue(graphicQueue) {
	};

	void SetSceneBoundry(const UniformBoxInfoObject& boxObj);

	void CreateObjModelResources(std::string& obj_path, ObjResource& objRes,
		glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useParticle, bool useVoxBox, bool useGlb);

	void CreateObjModelResources(std::vector<std::string> obj_paths, std::vector<ObjResource*>& objReses, 
		glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useParticle, bool useVoxBox, bool useGlb);

	void CreatePlyModelResources(std::string& ply_path, ObjResource& objRes,
		glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useVoxBox);

	void CreatePlane(ObjResource& objRes, glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation);

	void CreateEllipsoid(ObjResource& objRes, glm::vec3 radii, glm::vec3 center);

	void CreateObjTransform(ObjResource& objRes);

	void PrepareScenes(std::vector<ObjResManagement>& ORManagements, SceneType type);

	std::map<ObjResource*, std::array<std::string, 2>> GetObjResPathMap() const;

	ObjResource* GetObjResource_Table();
	ObjResource* GetObjResource_Collision0();
	ObjResource* GetObjResource_Mountain();
	ObjResource* GetObjResource_Landscape();
	ObjResource* GetObjResource_DuckFloat();
	ObjResource* GetObjResource_ForkLift();

};