#include "sceneset.h"
#include<iostream>
#include<cstring>
#include<cstdio>
#include<exception>
#include<set>
#include<array>
#include<algorithm>

void SceneSet::SetSceneBoundry(const UniformBoxInfoObject& boxObj)
{
	this->boundryMin = glm::vec3(boxObj.clampX.x, boxObj.clampY.x, boxObj.clampZ.x);
	this->boundryMax = glm::vec3(boxObj.clampX.y, boxObj.clampY.y, boxObj.clampZ.y);
}

void SceneSet::CreateObjModelResources(std::string& obj_path, ObjResource& objRes,
	glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useParticle, bool useVoxBox, bool useGlb)
{
	std::string obj_file_path = obj_path;

	// store index and vertex data from .obj or .glb or other ...
	if (!useGlb)
		Scene::CreateObjModelResources_PolygonSupport(obj_file_path, objRes, translation, scale_rate, rotation);
	else
		Scene::LoadGLBModel(obj_file_path, objRes, translation, scale_rate, rotation);

	// pretend to use particle 
	if (useParticle) {
	}

	// compute vox box for voxelization
	if (useVoxBox) {
		objRes.boxMin = glm::vec3(std::numeric_limits<float>::max());
		objRes.boxMax = glm::vec3(std::numeric_limits<float>::lowest());
		for (const auto& vertex : objRes.vertices) {
			objRes.boxMin = glm::min(objRes.boxMin, vertex.pos);
			objRes.boxMax = glm::max(objRes.boxMax, vertex.pos);
		}
		objRes.boxMin = glm::min(objRes.boxMin, boundryMin);
		objRes.boxMax = glm::min(objRes.boxMax, boundryMax);
		objRes.boxMin -= VOXEL_MARGIN;
		objRes.boxMax += VOXEL_MARGIN;
		objRes.voxelSize = glm::max(objRes.boxMax.x - objRes.boxMin.x,
			glm::max(objRes.boxMax.y - objRes.boxMin.y, objRes.boxMax.z - objRes.boxMin.z)) / VOXEL_RES;

		// uniform struct
		objRes.voxinfobj.boxMin = objRes.boxMin;
		objRes.voxinfobj.resolution = glm::ivec3(VOXEL_RES, VOXEL_RES, VOXEL_RES);
		objRes.voxinfobj.voxelSize = objRes.voxelSize;
	}
	Buffer::CreateObjVertexIndexBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue, objRes);
}

// many objs share 1 voxel box
void SceneSet::CreateObjModelResources(std::vector<std::string> obj_paths, std::vector<ObjResource*>& objReses,
	glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useParticle, bool useVoxBox, bool useGlb)
{
	uint32_t objSize = obj_paths.size();
	assert(objSize == objReses.size());

	glm::vec3 boxMin = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 boxMax = glm::vec3(std::numeric_limits<float>::lowest());

	for (uint32_t i = 0; i < objSize; i++) {
		std::string obj_file_path = obj_paths[i];
		ObjResource* objRes = objReses[i];

		if (!useGlb)
			Scene::CreateObjModelResources_PolygonSupport(obj_file_path, *objRes, translation, scale_rate, rotation);
		else
			Scene::LoadGLBModel(obj_file_path, *objRes, translation, scale_rate, rotation);

		if (useVoxBox) {
			for (const auto& vertex : objRes->vertices) {
				boxMin = glm::min(objRes->boxMin, vertex.pos);
				boxMax = glm::max(objRes->boxMax, vertex.pos);
			}
		}
	}
	glm::vec3 voxel_margin = { VOXEL_MARGIN, VOXEL_MARGIN, VOXEL_MARGIN };
	for (uint32_t i = 0; i < objSize; i++) {
		ObjResource* objRes = objReses[i];
		objRes->boxMin = boxMin - voxel_margin;
		objRes->boxMax = boxMax - voxel_margin;
		objRes->voxelSize = glm::max(objRes->boxMax.x - objRes->boxMin.x,
			glm::max(objRes->boxMax.y - objRes->boxMin.y, objRes->boxMax.z - objRes->boxMin.z)) / VOXEL_RES;
		objRes->voxinfobj.boxMin = objRes->boxMin;
		objRes->voxinfobj.resolution = glm::ivec3(VOXEL_RES, VOXEL_RES, VOXEL_RES);
		objRes->voxinfobj.voxelSize = objRes->voxelSize;

		Buffer::CreateObjVertexIndexBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue, *objRes);
	}
}

void SceneSet::CreatePlyModelResources(std::string& ply_path, ObjResource& objRes,
	glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useVoxBox)
{
	std::string ply_file_path = ply_path;
	Scene::LoadPLYModel(ply_file_path, objRes, translation, scale_rate, rotation);

	if (useVoxBox) {
		objRes.boxMin = glm::vec3(std::numeric_limits<float>::max());
		objRes.boxMax = glm::vec3(std::numeric_limits<float>::lowest());
		for (const auto& vertex : objRes.vertices) {
			objRes.boxMin = glm::min(objRes.boxMin, vertex.pos);
			objRes.boxMax = glm::max(objRes.boxMax, vertex.pos);
		}
		objRes.boxMin -= VOXEL_MARGIN;
		objRes.boxMax += VOXEL_MARGIN;
		objRes.voxelSize = glm::max(objRes.boxMax.x - objRes.boxMin.x,
			glm::max(objRes.boxMax.y - objRes.boxMin.y, objRes.boxMax.z - objRes.boxMin.z)) / VOXEL_RES;

		objRes.voxinfobj.boxMin = objRes.boxMin;
		objRes.voxinfobj.resolution = glm::ivec3(VOXEL_RES, VOXEL_RES, VOXEL_RES);
		objRes.voxinfobj.voxelSize = objRes.voxelSize;
	}
	Buffer::CreateObjVertexIndexBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue, objRes);
}

void SceneSet::CreatePlane(ObjResource& objRes, glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation)
{
	Scene::CreatePlane(objRes, translation, scale_rate, rotation);
	Buffer::CreateObjVertexIndexBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue, objRes);
}

void SceneSet::CreateEllipsoid(ObjResource& objRes, glm::vec3 radii, glm::vec3 center)
{
	Scene::CreateEllipsoid(objRes, radii, center);
	Buffer::CreateObjVertexIndexBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue, objRes);
}

void SceneSet::CreateObjTransform(ObjResource& objRes)
{
	VkDeviceSize size = sizeof(ObjTransform);
	Buffer::CreateBuffer(PDevice, LDevice, objRes.UniformBuffer_, objRes.UniformBufferMemory_, size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(LDevice, objRes.UniformBufferMemory_, 0, size, 0, &objRes.MappedBuffer_);
	memcpy(objRes.MappedBuffer_, &objRes.transformobj, size);

	{
		objRes.boxMin = glm::vec3(std::numeric_limits<float>::max());
		objRes.boxMax = glm::vec3(std::numeric_limits<float>::lowest());
		for (const auto& vertex : objRes.vertices) {
			objRes.boxMin = glm::min(objRes.boxMin, vertex.pos);
			objRes.boxMax = glm::max(objRes.boxMax, vertex.pos);
		}
	}
}

void SceneSet::PrepareScenes(std::vector<ObjResManagement>& ORManagements, SceneType type)
{
	if (type == SceneType::STONE) {
		std::string str5 = SAND_PATH_GLB;
		std::string str5_1 = SAND_PATH_TEX;
		glm::vec3 translation5 = glm::vec3(-2.0, -1.0, 0.0);
		glm::vec3 scale_rate5 = glm::vec3(0.4, 0.4, 0.4);
		glm::mat3 rotation5 = Scene::getRotationMatrix(0.0, 0.0, 0.0);
		CreateObjModelResources(str5, sand, translation5, scale_rate5, rotation5, false, false, true);
		Image::CreateTextureResources2(str5_1.c_str(), sand, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

		std::string str6 = WALL_PATH_GLB;
		std::string str6_1 = WALL_PATH_TEX;
		glm::vec3 translation6 = glm::vec3(-2.0, -1.0, 0.0);
		glm::vec3 scale_rate6 = glm::vec3(0.5, 0.5, 0.5);
		glm::mat3 rotation6 = Scene::getRotationMatrix(0.0, -M_PI / 2, 0.0);
		CreateObjModelResources(str6, wall, translation6, scale_rate6, rotation6, false, false, true);
		Image::CreateTextureResources2(str6_1.c_str(), wall, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

		// rocks
		std::string str7 = ROCK1_PATH_GLB;
		std::string str7_1 = ROCK1_PATH_TEX;
		glm::vec3 translation7 = glm::vec3(-1.4, -0.6, 0.0);
		glm::vec3 scale_rate7 = glm::vec3(0.6, 0.6, 0.6);
		glm::mat3 rotation7 = Scene::getRotationMatrix(0.0, 0.0, 0.0);
		CreateObjModelResources(str7, rock1, translation7, scale_rate7, rotation7, false, false, true);
		Image::CreateTextureResources2(str7_1.c_str(), rock1, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

		std::string str8 = ROCK2_PATH_GLB;
		std::string str8_1 = ROCK2_PATH_TEX;
		glm::vec3 translation8 = glm::vec3(-2.5, -0.6, 1.0);
		glm::vec3 scale_rate8 = glm::vec3(0.5, 0.5, 0.5);
		glm::mat3 rotation8 = Scene::getRotationMatrix(0.0, 0.0, 0.0);
		CreateObjModelResources(str8, rock2, translation8, scale_rate8, rotation8, false, false, true); // particle vox glb
		Image::CreateTextureResources2(str8_1.c_str(), rock2, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

		ORManagements.push_back({ &sand,         false, true, true, false, false });   // isVox  isTex  isGlb  is2Tex  isPBR
		ORManagements.push_back({ &wall,         false, true, true, false, false });
		ORManagements.push_back({ &rock1,        false, true, true, false, false });
		ORManagements.push_back({ &rock2,        false, true, true, false, false });

		objResPaths.insert({ &sand,  {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
		objResPaths.insert({ &wall,  {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
		objResPaths.insert({ &rock1, {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
		objResPaths.insert({ &rock2, {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
	}
	else if (type == SceneType::PBRTEST) {
		{
			std::string str = APPLE_PATH_OBJ;
			std::string str_1 = APPLE_PATH_TEX_COLOR;
			std::string str_2 = APPLE_PATH_TEX_NORMALGL;
			std::string str_3 = APPLE_PATH_TEX_AO;
			glm::vec3 translation = glm::vec3(1.0, 0.0, 1.0);
			glm::vec3 scale_rate = glm::vec3(1.2, 1.2, 1.2);
			glm::mat3 rotation = glm::mat3(1.0);
			CreateObjModelResources(str, apple, translation, scale_rate, rotation, false, false, false);
			apple.textureImagePBR.resize(3);
			apple.textureImageMemoryPBR.resize(3);
			apple.textureImageViewPBR.resize(3);
			apple.textureSamplerPBR.resize(3);
			Image::CreateTextureResourcesPBR(str_1.c_str(), apple, 0, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
			Image::CreateTextureResourcesPBR(str_2.c_str(), apple, 1, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
			Image::CreateTextureResourcesPBR(str_3.c_str(), apple, 2, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = HELI_PATH_OBJ;
			std::string str_1 = HELI_PATH_TEX_COLOR;
			std::string str_2 = HELI_PATH_TEX_NORMAL;
			std::string str_3 = HELI_PATH_TEX_METALLICROUGHNESS;
			glm::vec3 translation = glm::vec3(-0.8, 1.9, -0.5);
			glm::vec3 scale_rate = glm::vec3(0.001, 0.001, 0.001);
			glm::mat3 rotation = Scene::getRotationMatrix(-M_PI / 2, 0.0, 0.0);
			CreateObjModelResources(str, helicopter, translation, scale_rate, rotation, false, false, false);
			helicopter.textureImagePBR.resize(3);
			helicopter.textureImageMemoryPBR.resize(3);
			helicopter.textureImageViewPBR.resize(3);
			helicopter.textureSamplerPBR.resize(3);
			Image::CreateTextureResourcesPBR(str_1.c_str(), helicopter, 0, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
			Image::CreateTextureResourcesPBR(str_2.c_str(), helicopter, 1, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
			Image::CreateTextureResourcesPBR(str_3.c_str(), helicopter, 2, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}

		// isVox; isTex; isGlb; is2Tex; isPBR;
		ORManagements.push_back({ &apple,       false, false, false, false, true });  // isVox  isTex  isGlb  is2Tex  isPBR
		ORManagements.push_back({ &helicopter,  false, false, true,  false, true });

		objResPaths.insert({ &apple,      {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_pbr.frag.spv"} });
		objResPaths.insert({ &helicopter, {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_pbr2.frag.spv"} });
	}
	else if (type == SceneType::FLEX) {
		{
			std::string str = CAT_GLB;
			std::string str_ = CAT_BASECOLOR;
			glm::vec3 translation = glm::vec3(0.8, 0.05, 0.4);
			glm::vec3 scale_rate = glm::vec3(1.8, 1.8, 1.8);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreateObjModelResources(str, collision0, translation, scale_rate, rotation, false, true, true);
			Image::CreateTextureResources2(str_.c_str(), collision0, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		/*{
			std::string str = CAT_GLB;
			std::string str_base = CAT_BASECOLOR;
			std::string str_normal = CAT_NORMAL;
			std::string str_roughness = CAT_ROUGHNESS;
			std::string str_metal = CAT_METAL;
			std::string str_ao = CAT_AO;
			std::string str_displace = CAT_DISPLACE;
			std::vector<std::string> str_texs = {
				str_base, str_normal, str_roughness, str_metal, str_ao, str_displace
			};
			glm::vec3 translation = glm::vec3(0.8, 0.0, 0.4);
			glm::vec3 scale_rate = glm::vec3(1.8, 1.8, 1.8);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreateObjModelResources(str, cat, translation, scale_rate, rotation, false, false, true);
			cat.textureImagePBR.resize(6);
			cat.textureImageMemoryPBR.resize(6);
			cat.textureImageViewPBR.resize(6);
			cat.textureSamplerPBR.resize(6);
			for (uint32_t i = 0; i < 6; i++) {
				Image::CreateTextureResourcesPBR(str_texs[i].c_str(), cat, i, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
			}
		}*/
		{
			std::string str = PLANE_TEX;
			//glm::vec3 translation = glm::vec3(1.4, 0.0, 1.4);
			//glm::vec3 scale_rate = glm::vec3(2.8, 1.0, 2.8);
			glm::vec3 translation = glm::vec3(0.8, 0.0, 0.4);
			glm::vec3 scale_rate = glm::vec3(2.8, 1.0, 2.8);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreatePlane(plane, translation, scale_rate, rotation);
			Image::CreateTextureResources2(str.c_str(), plane, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

			/*glm::vec3 translation2 = glm::vec3(0.0, 1.4, 1.4);
			glm::vec3 scale_rate2 = glm::vec3(2.8, 1.0, 2.8);
			glm::mat3 rotation2 = Scene::getRotationMatrix(0.0, 0.0, M_PI / 2);
			CreatePlane(plane_left, translation2, scale_rate2, rotation2);
			Image::CreateTextureResources2(str.c_str(), plane_left, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

			glm::vec3 translation3 = glm::vec3(2.8, 1.4, 1.4);
			glm::vec3 scale_rate3 = glm::vec3(2.8, 1.0, 2.8);
			glm::mat3 rotation3 = Scene::getRotationMatrix(0.0, 0.0, M_PI / 2);
			CreatePlane(plane_right, translation3, scale_rate3, rotation3);
			Image::CreateTextureResources2(str.c_str(), plane_right, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

			glm::vec3 translation4 = glm::vec3(1.4, 1.4, 0.0);
			glm::vec3 scale_rate4 = glm::vec3(2.8, 1.0, 2.8);
			glm::mat3 rotation4 = Scene::getRotationMatrix(M_PI / 2, 0.0, 0.0);
			CreatePlane(plane_front, translation4, scale_rate4, rotation4);
			Image::CreateTextureResources2(str.c_str(), plane_front, PDevice, LDevice, CommandPool, GraphicNComputeQueue);

			glm::vec3 translation5 = glm::vec3(1.4, 1.4, 2.8);
			glm::vec3 scale_rate5 = glm::vec3(2.8, 2.0, 2.8);
			glm::mat3 rotation5 = Scene::getRotationMatrix(M_PI / 2, 0.0, 0.0);
			CreatePlane(plane_back, translation5, scale_rate5, rotation5);
			Image::CreateTextureResources2(str.c_str(), plane_back, PDevice, LDevice, CommandPool, GraphicNComputeQueue);*/
		}
		ORManagements.push_back({ &collision0,  true, true, true, false, false });    // isVox  isTex  isGlb  is2Tex  isPBR
		//ORManagements.push_back({ &cat,         false, false, true, false, true });   // isVox  isTex  isGlb  is2Tex  isPBR
		ORManagements.push_back({ &plane,       false, true, false, false, false });
		//ORManagements.push_back({ &plane_left,  false, true, false, false, false });
		//ORManagements.push_back({ &plane_right, false, true, false, false, false });
		//ORManagements.push_back({ &plane_front, false, true, false, false, false });
		//ORManagements.push_back({ &plane_back,  false, true, false, false, false });
		
		objResPaths.insert({ &collision0,  {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_vox.frag.spv"} });
		//objResPaths.insert({ &cat,         {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_pbr3.frag.spv"} });
		objResPaths.insert({ &plane,       {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
		//objResPaths.insert({ &plane_left,  {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
		//objResPaths.insert({ &plane_right, {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
		//objResPaths.insert({ &plane_front, {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
		//objResPaths.insert({ &plane_back,  {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
	}
	else if (type == SceneType::SWIMMINGPOOL) {
		{
			std::string str = POOL_GLB;
			std::string str_ = POOL_BOTTOM;
			glm::vec3 translation = glm::vec3(1.0, 0.12, 0.6);
			glm::vec3 scale_rate = glm::vec3(0.5, 0.5, 0.5);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreateObjModelResources(str, pool, translation, scale_rate, rotation, false, false, true);
			Image::CreateTextureResources2(str_.c_str(), pool, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = POOL_GROUND_GLB;
			std::string str_ = POOL_GROUND_TEX;
			glm::vec3 translation = glm::vec3(1.0, 0.12, 0.6);
			glm::vec3 scale_rate = glm::vec3(0.5, 0.5, 0.5);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreateObjModelResources(str, pool_ground, translation, scale_rate, rotation, false, false, true);
			Image::CreateTextureResources2(str_.c_str(), pool_ground, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = POOL_LADDER_GLB;
			std::string str_ = POOL_LADDER_TEX;
			glm::vec3 translation = glm::vec3(1.0, -0.09, 0.05);
			glm::vec3 scale_rate = glm::vec3(0.2, 0.2, 0.2);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, M_PI / 2, 0.0);
			CreateObjModelResources(str, ladder1, translation, scale_rate, rotation, false, false, true);
			Image::CreateTextureResources2(str_.c_str(), ladder1, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = POOL_LADDER_GLB;
			std::string str_ = POOL_LADDER_TEX;
			glm::vec3 translation = glm::vec3(1.0, -0.09, 1.15);
			glm::vec3 scale_rate = glm::vec3(0.2, 0.2, 0.2);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, -M_PI / 2, 0.0);
			CreateObjModelResources(str, ladder2, translation, scale_rate, rotation, false, false, true);
			Image::CreateTextureResources2(str_.c_str(), ladder2, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = CHAISE_OBJ;
			std::string str_ = CHAISE_TEX;
			glm::vec3 translation = glm::vec3(1.8, 0.18, -0.5);
			glm::vec3 scale_rate = glm::vec3(0.01, 0.01, 0.01);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, -M_PI / 6, 0.0);
			CreateObjModelResources(str, chaise1, translation, scale_rate, rotation, false, false, false);
			Image::CreateTextureResources2(str_.c_str(), chaise1, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = CHAISE_OBJ;
			std::string str_ = CHAISE_TEX;
			glm::vec3 translation = glm::vec3(1.2, 0.18, -0.5);
			glm::vec3 scale_rate = glm::vec3(0.01, 0.01, 0.01);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, M_PI / 6, 0.0);
			CreateObjModelResources(str, chaise2, translation, scale_rate, rotation, false, false, false);
			Image::CreateTextureResources2(str_.c_str(), chaise2, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = UMBRELLA_OBJ;
			std::string str_ = UMBRELLA_TEX;
			glm::vec3 translation = glm::vec3(1.45, 0.18, -0.5);
			glm::vec3 scale_rate = glm::vec3(0.003, 0.003, 0.003);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreateObjModelResources(str, umbrella, translation, scale_rate, rotation, false, false, false);
			Image::CreateTextureResources2(str_.c_str(), umbrella, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = DUCKFLOAT_GLB;
			std::string str_ = DUCKFLOAT_TEX;
			glm::vec3 translation = glm::vec3(0.18, 0.24, 0.62);
			glm::vec3 scale_rate = glm::vec3(0.5, 0.5, 0.5);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreateObjModelResources(str, duck_float, translation, scale_rate, rotation, false, true, true);
			Image::CreateTextureResources2(str_.c_str(), duck_float, PDevice, LDevice, CommandPool, GraphicNComputeQueue);
		}
		{
			std::string str = SLIDE_GLB;
			glm::vec3 translation = glm::vec3(1.7, 0.04, 1.00);
			glm::vec3 scale_rate = glm::vec3(0.3, 0.3, 0.3);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, -3 * M_PI / 2, 0.0);
			CreateObjModelResources(str, slide, translation, scale_rate, rotation, false, false, true);
		}
		{
			std::string str = WATERWHEEL_GLB;
			glm::vec3 translation = glm::vec3(1.5, 0.2, 0.6);
			glm::vec3 scale_rate = glm::vec3(0.6, 0.6, 0.6);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, 0.0, 0.0);
			CreateObjModelResources(str, water_wheel, translation, scale_rate, rotation, false, false, true);
			CreateObjTransform(water_wheel);
		}
		{
			// ellipsoid
			// glm::vec3 center = glm::vec3(1.0, 0.1, 0.6);
			glm::vec3 center = glm::vec3(0.0, 0.0, 0.0);   // center信息存在model中
			glm::vec3 radii = glm::vec3(0.2, 0.074, 0.1);
			CreateEllipsoid(ellipsoid, radii, center);
		}
		{
			// boat
			std::string str = BOAT_GLB;
			// -0.01, -0.024, 0.0  |  0.99, 0.074, 0.6
			glm::vec3 translation = glm::vec3(-0.01, -0.024, 0.0);
			glm::vec3 scale_rate = glm::vec3(0.01, 0.013, 0.008);
			glm::mat3 rotation = Scene::getRotationMatrix(0.0, M_PI / 2, 0.0);
			CreateObjModelResources(str, boat, translation, scale_rate, rotation, false, false, true);
		}
		// ply model
		//{
		//	std::string str = FORKLIFT_PLY;
		//	glm::vec3 translation = glm::vec3(1.2, 0.1, 0.8);
		//	glm::vec3 scale_rate = glm::vec3(0.25, 0.25, 0.25);
		//	glm::mat3 rotation = Scene::getRotationMatrix(-M_PI / 2, M_PI / 2, 0.0);
		//	CreatePlyModelResources(str, fork_lift, translation, scale_rate, rotation, true);
		//}
		ORManagements.push_back({ &pool,         false, true, true, false, false });    // isVox  isTex  isGlb  is2Tex  isPBR  isTransform
		ORManagements.push_back({ &pool_ground,  false, true, true, false, false });
		ORManagements.push_back({ &ladder1,		 false, true, true, false, false });
		ORManagements.push_back({ &ladder2,		 false, true, true, false, false });
		ORManagements.push_back({ &chaise1,		 false, true, false, false, false });
		ORManagements.push_back({ &chaise2,		 false, true, false, false, false });
		ORManagements.push_back({ &umbrella,	 false, true, false, false, false });
		ORManagements.push_back({ &duck_float,	 true,  true, true, false, false });
		ORManagements.push_back({ &slide,	     false, false, true, false, false });
		//ORManagements.push_back({ &ellipsoid,    false, false, false, false, false, false, true });
		ORManagements.push_back({ &boat,         false, false, true, false, false, false, true });
		ORManagements.push_back({ &water_wheel,	 false, false, true, false, false, true });
		//ORManagements.push_back({ &fork_lift,	 true, false, false, false, false });
		objResPaths.insert({ &pool,         {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
		objResPaths.insert({ &pool_ground,  {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
		objResPaths.insert({ &ladder1,      {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
		objResPaths.insert({ &ladder2,      {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex.frag.spv"} });
		objResPaths.insert({ &chaise1,      {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
		objResPaths.insert({ &chaise2,      {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
		objResPaths.insert({ &umbrella,     {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_origin.frag.spv"} });
		objResPaths.insert({ &duck_float,   {"resources/shaders/spv/obj_unified_tex.vert.spv", "resources/shaders/spv/obj_unified_tex_vox.frag.spv"} });
		objResPaths.insert({ &slide,        {"resources/shaders/spv/obj_unified.vert.spv",	   "resources/shaders/spv/obj_unified.frag.spv"} });
		//objResPaths.insert({ &ellipsoid,    {"resources/shaders/spv/obj_unified_ellipsode.vert.spv", "resources/shaders/spv/obj_unified.frag.spv"} });
		objResPaths.insert({ &boat,         {"resources/shaders/spv/obj_unified_ellipsode.vert.spv", "resources/shaders/spv/obj_unified.frag.spv"} });
		objResPaths.insert({ &water_wheel,  {"resources/shaders/spv/obj_unified_transform.vert.spv", "resources/shaders/spv/obj_unified.frag.spv"} });
		//objResPaths.insert({ &fork_lift,    {"resources/shaders/spv/ply_shader.vert.spv",      "resources/shaders/spv/ply_shader_vox.frag.spv"} });
	}
}

std::map<ObjResource*, std::array<std::string, 2>> SceneSet::GetObjResPathMap() const
{
	return objResPaths;
}

ObjResource* SceneSet::GetObjResource_Table()
{
	return &table;
}

ObjResource* SceneSet::GetObjResource_Collision0()
{
	return &collision0;
}

ObjResource* SceneSet::GetObjResource_Mountain()
{
	return &mountain;
}

ObjResource* SceneSet::GetObjResource_Landscape()
{
	return &landscape;
}

ObjResource* SceneSet::GetObjResource_DuckFloat()
{
	return &duck_float;
}

ObjResource* SceneSet::GetObjResource_ForkLift()
{
	return &fork_lift;
}