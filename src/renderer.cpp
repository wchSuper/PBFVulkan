#include"renderer.h"
#include"extensionfuncs.h"

#define STB_IMAGE_IMPLEMENTATION
#include"stb_image.h"

#include<iostream>
#include<cstring>
#include<cstdio>
#include<exception>
#include<set>
#include<array>
#include<algorithm>
 #include<random>
 #include<cmath>
 #include<filesystem>
 #include<fstream>


#define Allocator nullptr
#define M_PI 3.1415926
static void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

void Renderer::WindowResizeCallback(GLFWwindow* window, int width, int height)
{
	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	renderer->bFramebufferResized = true;
}

void Renderer::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	// Check if ImGui wants to capture mouse input
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return; // Don't process mouse events when interacting with UI
	}

	double xPos, yPos;
	glfwGetCursorPos(window, &xPos, &yPos);

	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));

	renderer->MousePressed(window, button, action, mods);
}

void Renderer::MouseMoveCallback(GLFWwindow* window, double xPos, double yPos)
{
	// Check if ImGui wants to capture mouse input
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return; // Don't process mouse movement when hovering over UI
	}

	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	renderer->MouseMoved(window, xPos, yPos);
}

void Renderer::MouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
	// Check if ImGui wants to capture mouse input
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		return; // Don't process scroll events when scrolling UI elements
	}

	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	renderer->MouseScroll(window, xOffset, yOffset);
}

void Renderer::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Check if ImGui wants to capture keyboard input
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard) {
		return; // Don't process keyboard events when typing in UI
	}

	auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	renderer->KeyProcess(key, action);
}

void Renderer::MousePressed(GLFWwindow* window, int button, int action, int mods)
{
	// Check if mouse is over UI (for safety, though callback should already filter)
	ImGuiIO& io = ImGui::GetIO();
	
	if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
		if (action == GLFW_PRESS) {
			if (!io.WantCaptureMouse) {
				middleMousePressed = true;
				glfwGetCursorPos(window, &lastX, &lastY);
			}
		}
		else if (action == GLFW_RELEASE) {
			middleMousePressed = false;
		}
	}

	else if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			if (!io.WantCaptureMouse) {
				double xPos, yPos;
				glfwGetCursorPos(window, &xPos, &yPos);
				int width, height;
				glfwGetFramebufferSize(window, &width, &height);
				float xNorm = static_cast<float>(xPos) / width;
				float yNorm = static_cast<float>(yPos) / height;
				float worldX = boxinfobj.clampX.x + xNorm * (boxinfobj.clampX.y - boxinfobj.clampX.x);
				float worldY = boxinfobj.clampY.y - yNorm * (boxinfobj.clampY.y - boxinfobj.clampY.x);
				float worldZ = (boxinfobj.clampZ.x + boxinfobj.clampZ.y) * 0.5;
				worldX = std::clamp(worldX, boxinfobj.clampX.x + simulatingobj.sphRadius, boxinfobj.clampX.y - simulatingobj.sphRadius);
				worldY = std::clamp(worldY, boxinfobj.clampY.x + simulatingobj.sphRadius, boxinfobj.clampY.y - simulatingobj.sphRadius);
				worldZ = std::clamp(worldZ, boxinfobj.clampZ.x + simulatingobj.sphRadius, boxinfobj.clampZ.y - simulatingobj.sphRadius);
				mousePos = glm::vec3(worldX, worldY, worldZ);
				simulatingobj.mouse_pos = mousePos;
				if (isEmitFluids) {
					if (!mpmAlgorithm) {
						GenerateWaterStream(worldX, worldY, worldZ);
					}
				}
				else {
					simulatingobj.attract = 1;
				}
			}
		}
		else if (action == GLFW_RELEASE) {
			// Always reset attract state on release, even if over UI
			simulatingobj.attract = 0;
		}
	}

	else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			if (!io.WantCaptureMouse) {
				rightMousePressed = true;
				glfwGetCursorPos(window, &lastX, &lastY);
			}
		}
		else if (action == GLFW_RELEASE) {
			rightMousePressed = false;
		}
	}
}

void Renderer::ExportSDFField(size_t objIndex, const std::string& outDir)
{
	if (objIndex >= objResManagements.size()) {
		throw std::runtime_error("ExportSDFField: objIndex out of range");
	}
	ObjResManagement& m = objResManagements[objIndex];
	if (!m.isVox || m.obj_res == nullptr) {
		throw std::runtime_error("ExportSDFField: selected model has no voxel resources");
	}
	ObjResource& objRes = *m.obj_res;

	std::vector<uint32_t> voxels = Buffer::RetrieveVoxelDataFromGPU(PDevice, LDevice, CommandPool, GraphicNComputeQueue, objRes);

	SDF sdfLocal;
	sdfLocal.MakeSDF(objRes, voxels);
	std::vector<float> sdfValues = sdfLocal.GetSDFValue();

	if (sdfValues.empty()) {
		throw std::runtime_error("ExportSDFField: sdfValues empty");
	}

	std::filesystem::create_directories(outDir);

	std::string baseName;
	if (!m.displayName.empty()) {
		baseName = m.displayName;
	} else {
		baseName = std::string("Obj_") + std::to_string(objIndex);
	}
	for (char& c : baseName) {
		if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*'
			|| c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
			c = '_';
		}
	}

	std::filesystem::path rawPath = std::filesystem::path(outDir) / (baseName + ".raw");
	std::filesystem::path mhdPath = std::filesystem::path(outDir) / (baseName + ".mhd");

	{
		std::ofstream raw(rawPath, std::ios::binary);
		if (!raw.is_open()) {
			throw std::runtime_error("ExportSDFField: failed to open raw output file");
		}
		raw.write(reinterpret_cast<const char*>(sdfValues.data()), static_cast<std::streamsize>(sizeof(float) * sdfValues.size()));
	}

	const uint32_t dimX = static_cast<uint32_t>(objRes.voxinfobj.resolution.x);
	const uint32_t dimY = static_cast<uint32_t>(objRes.voxinfobj.resolution.y);
	const uint32_t dimZ = static_cast<uint32_t>(objRes.voxinfobj.resolution.z);
	const float spacing = objRes.voxinfobj.voxelSize;
	const glm::vec3 origin = objRes.voxinfobj.boxMin;
	{
		std::ofstream mhdFile(mhdPath);
		if (!mhdFile.is_open()) {
			throw std::runtime_error("ExportSDFField: failed to open mhd output file");
		}
		mhdFile << "ObjectType = Image\n";
		mhdFile << "NDims = 3\n";
		mhdFile << "BinaryData = True\n";
		mhdFile << "BinaryDataByteOrderMSB = False\n";
		mhdFile << "CompressedData = False\n";
		mhdFile << "DimSize = " << dimX << " " << dimY << " " << dimZ << "\n";
		mhdFile << "ElementType = MET_FLOAT\n";
		mhdFile << "ElementSpacing = " << spacing << " " << spacing << " " << spacing << "\n";
		mhdFile << "Offset = " << origin.x << " " << origin.y << " " << origin.z << "\n";
		mhdFile << "ElementDataFile = " << rawPath.filename().string() << "\n";
	}

	std::cout << "SDF exported: " << mhdPath.string() << std::endl;
}

void Renderer::MouseMoved(GLFWwindow* window, double xPos, double yPos)
{
	// If mouse moved over UI while dragging, stop camera movement
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse) {
		middleMousePressed = false;
		rightMousePressed = false;
		return;
	}

	if (middleMousePressed) {
		float dx = static_cast<float>(xPos - lastX);
		float dy = static_cast<float>(yPos - lastY);
		lastX = xPos;
		lastY = yPos;	
		Camera::PanCamera(dx, dy, panSpeed, cameraPos, cameraTarget, cameraUp, renderingobj);
	}
	else if (rightMousePressed) {
		float dx = static_cast<float>(xPos - lastX);
		float dy = static_cast<float>(yPos - lastY);
		lastX = xPos;
		lastY = yPos;
		Camera::RotateCamera(dx, dy, rotateSpeed, cameraPos, cameraTarget, cameraUp, renderingobj);
	}
	SetRenderingObj(renderingobj);
}

void Renderer::MouseScroll(GLFWwindow* window, double xOffset, double yOffset)
{
	Camera::ZoomCamera(static_cast<float>(yOffset), zoomSpeed, cameraPos, cameraTarget, cameraUp, renderingobj);
	SetRenderingObj(renderingobj);
}


void Renderer::KeyProcess(int key, int action)
{
	if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_E:
			{
				if (isEmitFluids) {
					isEmitFluids = false;
					isApplyForce = true;
				}
				else {
					isEmitFluids = true;
					isApplyForce = false;
					simulatingobj.attract = 0;
				}
				break;
			}

			case GLFW_KEY_D:
				if (isVoxelize) {
					GenerateObjWater();
				}
				break;

			case GLFW_KEY_G:
				if (!isDeleteCubeParticles) {
					isDeleteCubeParticles = true;
					DeleteCubeParticles();
				}
		}
	}
}

// ********************************************************************************************************************* // 
void Renderer::UpdateUniformSimulatingBuffer()
{
	vkQueueWaitIdle(GraphicNComputeQueue);
	auto pObj = reinterpret_cast<UniformSimulatingObject*>(MappedSimulatingBuffer);
	pObj->dt = simulatingobj.dt;
	pObj->accumulated_t = simulatingobj.accumulated_t;  // 更新累计时间
	pObj->numParticles = simulatingobj.numParticles;
	pObj->visc = simulatingobj.visc;
	pObj->attract = simulatingobj.attract;
	pObj->mouse_pos = simulatingobj.mouse_pos;
	pObj->k = simulatingobj.k;
	pObj->relaxation = simulatingobj.relaxation;
	pObj->scorrK = simulatingobj.scorrK;
	
	// Artificial Viscosity Parameters
	pObj->enableArtificialViscosity = simulatingobj.enableArtificialViscosity;
	pObj->artificialViscosityAlpha = simulatingobj.artificialViscosityAlpha;
	pObj->artificialViscosityBeta = simulatingobj.artificialViscosityBeta;
	
	// Surface Tension Parameters
	pObj->enableSurfaceTension = simulatingobj.enableSurfaceTension;
	pObj->surfaceTensionCoefficient = simulatingobj.surfaceTensionCoefficient;
	pObj->surfaceTensionThreshold = simulatingobj.surfaceTensionThreshold;
	
	// Anisotropic Kernel Parameters
	pObj->anisotropicBaseRadiusScale = simulatingobj.anisotropicBaseRadiusScale;
	pObj->anisotropicMaxRatio = simulatingobj.anisotropicMaxRatio;
	pObj->anisotropicMinScale = simulatingobj.anisotropicMinScale;
	pObj->anisotropicMaxScale = simulatingobj.anisotropicMaxScale;
	pObj->anisotropicNeighborThreshold = simulatingobj.anisotropicNeighborThreshold;
	
	// Dynamic Wave Simulation Parameters
	pObj->waveEnabled = simulatingobj.waveEnabled;
	pObj->waveIntensity = simulatingobj.waveIntensity;
	pObj->waveSteepness = simulatingobj.waveSteepness;
	pObj->waveBaseHeight = simulatingobj.waveBaseHeight;
	pObj->windDirX = simulatingobj.windDirX;
	pObj->windDirZ = simulatingobj.windDirZ;
	pObj->windStrength = simulatingobj.windStrength;
	pObj->wavePeriod = simulatingobj.wavePeriod;
	
	pObj->scorrQ = simulatingobj.scorrQ;
	pObj->scorrN = simulatingobj.scorrN;
	//std::cout << pObj->attract << "\n";

	auto rObj = reinterpret_cast<UniformRenderingObject*>(MappedRenderingBuffer);
	rObj->fluid_color.x = colorWithAlpha[0];
	rObj->fluid_color.y = colorWithAlpha[1];
	rObj->fluid_color.z = colorWithAlpha[2];
	rObj->fluid_color.w = colorWithAlpha[3];
	rObj->isFilter = renderingobj.isFilter;

	if (current_time < export_sum_times) {
		// ExportSimRenderData();
	}

	if (specialParticleGenerate && specialParticle) {
		specialParticle->UpdateUniforms(renderingobj);
	}
	if (rigidCubeGenerate && rigidCube) {
		rigidCube->UpdateUniforms(renderingobj);
	}
	if (clothGenerate && cloth) {
		cloth->UpdateUniforms(renderingobj);
	}

	// obj transform  the transformed obj is the last in objResManagements
	static float animTime = 0.0f;
	animTime += 0.005f;  
	uint32_t objResManageSize = objResManagements.size();
	if (objResManageSize == 0) {
		return;
	}
	ObjResource* transformObj = objResManagements[objResManageSize - 1].obj_res;
	if (transformObj == nullptr || transformObj->MappedBuffer_ == nullptr) {
		return;
	}
	auto tObj = reinterpret_cast<ObjTransform*>(transformObj->MappedBuffer_);
	//tObj->objModel = glm::rotate(glm::mat4(1.0f), animTime, glm::vec3(0.0f, 0.0f, 1.0f));
	glm::vec3 objCenter = (transformObj->boxMin + transformObj->boxMax) * 0.5f;

	// 围绕自身中心旋转
	glm::mat4 translateToOrigin = glm::translate(glm::mat4(1.0f), -objCenter);
	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), animTime, glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 translateBack = glm::translate(glm::mat4(1.0f), objCenter);

	tObj->objModel = translateBack * rotation * translateToOrigin;
}

void Renderer::StartSDF(ObjResource* objRes, std::vector<uint32_t>& voxels, std::vector<float>& sdfValues)
{
	sdf->MakeSDF(*objRes, voxels);
	sdfValues = sdf->GetSDFValue();
	SDFUpdateResources(sdfValues, objRes->voxinfobj.boxMin, objRes->voxinfobj.voxelSize);
}

void Renderer::SDFUpdateResources(std::vector<float> sdfValues, glm::vec3 sdfRangeMin, float voxelSize)
{
	// 1. UniformSimulatingData
	vkQueueWaitIdle(GraphicNComputeQueue);
	auto pObj = reinterpret_cast<UniformSimulatingObject*>(MappedSimulatingBuffer);
	pObj->sdfRangeMin = sdfRangeMin;
	pObj->voxelResolution = VOXEL_RES;
	pObj->voxelSize = voxelSize;
	pObj->isSDF = true;

	// 2. SDFBuffer recreate
	CleanupBuffer(SDFBuffer, SDFBufferMemory, false);
	{
		if (sdfValues.empty()) {
			std::cout << "Warning: sdfValues is empty, skipping.\n";
			return;
		}
		VkDeviceSize size = sizeof(float) * sdfValues.size();

		Buffer::CreateBuffer(PDevice, LDevice, SDFBuffer, SDFBufferMemory, size,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue,
			sdfValues.data(), SDFBuffer);
		std::cout << "SDF buffer created and data uploaded!\n";
	}
}

void Renderer::ExportSimRenderData()
{
	 std::vector<Particle> exportParticles = RetrieveParticlesFromGPU(0);
	 std::vector<ExtraParticle> exportParticles2 = RetrieveRigidParticlesFromGPU(0);
	 std::string filename = "./particles/particle_";
	 std::string id = std::to_string(current_time) + ".txt";
	 filename = filename + id;
	 const char* file = filename.c_str();
	 Util::WriteParticles(file, exportParticles, exportParticles2);
	 current_time++;
	 std::cout << current_time << std::endl;
}

std::vector<Particle> Renderer::RetrieveParticlesFromGPU(uint32_t frameIndex)
{
	const uint32_t particleCount = std::min(simulatingobj.numParticles, simulatingobj.maxParticles);
	if (particleCount == 0) {
		return {};
	}
	VkDeviceSize size = particleCount * sizeof(Particle);
	std::vector<Particle> currentParticles(particleCount);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingMemory, size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkCommandBuffer cb = Util::CreateCommandBuffer(LDevice, CommandPool);
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	vkCmdCopyBuffer(cb, ParticleBuffers[frameIndex], stagingBuffer, 1, &copyRegion);
	VkSubmitInfo submitInfo{};
	Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);

	void* data;
	vkMapMemory(LDevice, stagingMemory, 0, size, 0, &data);
	memcpy(currentParticles.data(), data, size);
	vkUnmapMemory(LDevice, stagingMemory);

	CleanupBuffer(stagingBuffer, stagingMemory, false);

	return currentParticles;
}

std::vector<ExtraParticle> Renderer::RetrieveRigidParticlesFromGPU(uint32_t frameIndex)
{
	if (rigidParticles.empty()) {
		return {};
	}
	VkDeviceSize size = rigidParticles.size() * sizeof(ExtraParticle);
	std::vector<ExtraParticle> currentParticles(rigidParticles.size());

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingMemory, size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkCommandBuffer cb = Util::CreateCommandBuffer(LDevice, CommandPool);
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	vkCmdCopyBuffer(cb, RigidParticleBuffers[frameIndex], stagingBuffer, 1, &copyRegion);
	VkSubmitInfo submitInfo{};
	Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);

	void* data;
	vkMapMemory(LDevice, stagingMemory, 0, size, 0, &data);
	memcpy(currentParticles.data(), data, size);
	vkUnmapMemory(LDevice, stagingMemory);

	CleanupBuffer(stagingBuffer, stagingMemory, false);

	return currentParticles;
}

ShapeMatchingInfo Renderer::RetrieveUniformFromGPU()
{
	VkDeviceSize size = sizeof(ShapeMatchingInfo);
	ShapeMatchingInfo smi_;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingMemory, size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkCommandBuffer cb = Util::CreateCommandBuffer(LDevice, CommandPool);
	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	vkCmdCopyBuffer(cb, ShapeMatchingBuffers[0], stagingBuffer, 1, &copyRegion);

	VkSubmitInfo submitInfo{};
	Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
	void* data;
	vkMapMemory(LDevice, stagingMemory, 0, size, 0, &data);
	memcpy(&smi_, data, size);
	vkUnmapMemory(LDevice, stagingMemory);

	CleanupBuffer(stagingBuffer, stagingMemory, false);
	return smi_;
}

void Renderer::GenerateWaterStream(float xStart, float yStart, float zStart)
{
	std::vector<Particle> currentParticles = RetrieveParticlesFromGPU(CurrentFlight);
	float radius = simulatingobj.sphRadius / 4;
	float diam = 2 * radius;
	float speed = 0.5f;
	std::vector<Particle> newParticles;
	int streamSize = 5;
	for (int i = 0; i < 2 * streamSize; ++i) {
		for (int j = 0; j < 5 * streamSize; ++j) {
			for (int k = 0; k < 2 * streamSize; ++k) {
				float xPos = xStart + i * diam;
				float yPos = yStart + j * diam;
				float zPos = zStart + k * diam;
				if (xPos >= boxinfobj.clampX.x && xPos <= boxinfobj.clampX.y &&
					yPos >= boxinfobj.clampY.x && yPos <= boxinfobj.clampY.y &&
					zPos >= boxinfobj.clampZ.x && zPos <= boxinfobj.clampZ.y) {
					Particle particle{};
					particle.Location = glm::vec3(xPos, yPos, zPos);
					particle.Velocity = glm::vec3(0.0f, speed, 0.0f);
					particle.Mass = 1.0f;
					particle.NumNgbrs = 0;
					newParticles.push_back(particle);
				}
			}
		}
	}
	currentParticles.insert(currentParticles.end(), newParticles.begin(), newParticles.end());
	size_t totalParticles = currentParticles.size();
	if (totalParticles > simulatingobj.maxParticles) {
		currentParticles.resize(simulatingobj.maxParticles);
		totalParticles = simulatingobj.maxParticles;
	}
	particles = currentParticles;
	simulatingobj.numParticles = static_cast<uint32_t>(particles.size());
	SetSimulatingObj(simulatingobj);
	for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
		CleanupBuffer(ParticleBuffers[i], ParticleBufferMemory[i], false);
	}
	CreateParticleBuffer();

	CleanupBuffer(CellInfoBuffer, CellInfoBufferMemory, false);
	CleanupBuffer(NeighborCountBuffer, NeighborCountBufferMemory, false);
	CleanupBuffer(NeighborCpyCountBuffer, NeighborCpyCountBufferMemory, false);
	CleanupBuffer(ParticleInCellBuffer, ParticleInCellBufferMemory, false);
	CleanupBuffer(CellOffsetBuffer, CellOffsetBufferMemory, false);
	CleanupBuffer(TestPBFBuffer, TestPBFBufferMemory, false);
	if (startAnisotrophic) {
		CleanupBuffer(AnisotrophicBuffers[0], AnisotrophicBufferMemory[0], false);
		CreateAnisotrophicBuffer();
	}
	CreateCellInfoBuffer();
	CreateNgbrCountBuffer();
	CreateNgbrCpyCountBuffer();
	CreateParticleInCellBuffer();
	CreateCellOffsetBuffer();
	CreateTestPBFBuffer();
	CreateDescriptorSet();
	RecordSimulatingCommandBuffers();
	RecordFluidsRenderingCommandBuffers();
}

void Renderer::GenerateObjWater()
{
	// add water particles from cube particles
	std::vector<Particle> currentParticles = RetrieveParticlesFromGPU(CurrentFlight);
	std::vector<Particle> newParticles{};
	for (auto cp : cubeCpyParticles) {
		Particle p{};
		p.Location = cp.Location;
		p.Velocity = glm::vec3(0.0, 0.0, 0.0);
		p.Mass = 1.0;
		newParticles.push_back(p);
	}
	currentParticles.insert(currentParticles.end(), newParticles.begin(), newParticles.end());
	size_t totalParticles = currentParticles.size();
	simulatingobj.numParticles = static_cast<uint32_t>(particles.size());
	if (totalParticles > simulatingobj.maxParticles) {
		currentParticles.resize(simulatingobj.maxParticles);
		totalParticles = simulatingobj.maxParticles;
	}
	particles = currentParticles;
	simulatingobj.numParticles = static_cast<uint32_t>(particles.size());
	SetSimulatingObj(simulatingobj);
	for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
		CleanupBuffer(ParticleBuffers[i], ParticleBufferMemory[i], false);
	}
	CreateParticleBuffer();

	CleanupBuffer(CellInfoBuffer, CellInfoBufferMemory, false);
	CleanupBuffer(NeighborCountBuffer, NeighborCountBufferMemory, false);
	CleanupBuffer(NeighborCpyCountBuffer, NeighborCpyCountBufferMemory, false);
	CleanupBuffer(ParticleInCellBuffer, ParticleInCellBufferMemory, false);
	CleanupBuffer(CellOffsetBuffer, CellOffsetBufferMemory, false);
	CleanupBuffer(TestPBFBuffer, TestPBFBufferMemory, false);
	if (startAnisotrophic) {
		CleanupBuffer(AnisotrophicBuffers[0], AnisotrophicBufferMemory[0], false);
		CreateAnisotrophicBuffer();
	}
	CreateCellInfoBuffer();
	CreateNgbrCountBuffer();
	CreateNgbrCpyCountBuffer();
	CreateParticleInCellBuffer();
	CreateCellOffsetBuffer();
	CreateTestPBFBuffer();
	CreateDescriptorSet();
	RecordSimulatingCommandBuffers();
	RecordFluidsRenderingCommandBuffers();
}

void Renderer::CleanupBuffer(VkBuffer& buffer, VkDeviceMemory& memory, bool mapped)
{
	if (mapped)
		vkUnmapMemory(LDevice, memory);
	vkDestroyBuffer(LDevice, buffer, Allocator);
	vkFreeMemory(LDevice, memory, Allocator);
}

// not completely delete the cube particles and buffer
void Renderer::DeleteCubeParticles()
{
	vkDeviceWaitIdle(LDevice);
	cubeParticles.resize(1);
	for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
		CleanupBuffer(CubeParticleBuffers[i], CubeParticleBufferMemory[i], false);
	}
	cubeParticles[0].Location = glm::vec3(-10.0, -10.0, -10.0);
	simulatingobj.cubeParticleNum = cubeParticles.size();
	SetSimulatingObj(simulatingobj);
	CreateCubeParticleBuffer();
	CreateDescriptorSet();
	RecordSimulatingCommandBuffers();
	RecordFluidsRenderingCommandBuffers();
}


void Renderer::UpdateDescriptorSet()
{
	for (uint32_t i = 0; i < PostprocessDescriptorSets.size(); ++i) {
		VkDescriptorImageInfo thickimageinfo      { ThickImageSampler,         ThickImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo depthimageinfo      { FilteredDepthImageSampler, FilteredDepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo dstimageinfo        { nullptr,                   SwapChainImageViews[i], VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo backgroundimageinfo { BackgroundImageSampler,    BackgroundImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo thickimageinfo2     { ThickImageSampler2,        ThickImageView2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo depthimageinfo2     { CustomDepthImageSampler2,  CustomDepthImageView2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo boxdepthimageinfo   { DepthImageSampler,         DepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo skyboxcubemapinfo   { skybox_resource.skyboxSampler, skybox_resource.skyboxImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };  // Cubemap for reflection
		VkDescriptorImageInfo causticsimageinfo   { CausticsImageSampler,      CausticsImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorBufferInfo renderingbufferinfo{ UniformRenderingBuffer,    0, sizeof(UniformRenderingObject) };
		
		std::vector<VkWriteDescriptorSet> writes(10);
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo,    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{depthimageinfo,         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{thickimageinfo,         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{backgroundimageinfo,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{thickimageinfo2,        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{depthimageinfo2,        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{dstimageinfo,           VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
			{boxdepthimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{skyboxcubemapinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{causticsimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, PostprocessDescriptorSets[i]);
	}
	{
		VkDescriptorBufferInfo renderingbufferinfo { UniformRenderingBuffer, 0, sizeof(UniformRenderingObject) };
		VkDescriptorImageInfo depthimageinfo       { FilteredDepthImageSampler, FilteredDepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo thickimageinfo       { ThickImageSampler, ThickImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo boxdepthimageinfo    { DepthImageSampler, DepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo causticsinputinfo    { BackgroundImageSampler, BackgroundImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo causticsoutinfo      { nullptr, CausticsImageView, VK_IMAGE_LAYOUT_GENERAL };

		std::vector<VkWriteDescriptorSet> writes(6);
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{depthimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{thickimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{boxdepthimageinfo,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{causticsinputinfo,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{causticsoutinfo,     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, CausticsDescriptorSet);
	}
	{
		VkDescriptorBufferInfo renderingbufferinfo { UniformRenderingBuffer, 0, sizeof(UniformRenderingObject) };
		VkDescriptorImageInfo depthimageinfo       { FilteredDepthImageSampler, FilteredDepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo thickimageinfo       { ThickImageSampler, ThickImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo boxdepthimageinfo    { DepthImageSampler, DepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo causticsinputinfo    { CausticsImageSampler, CausticsImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo causticsoutinfo      { nullptr, CausticsImageView2, VK_IMAGE_LAYOUT_GENERAL };

		std::vector<VkWriteDescriptorSet> writes(6);
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{depthimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{thickimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{boxdepthimageinfo,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{causticsinputinfo,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{causticsoutinfo,     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, CausticsBlurDescriptorSet);
	}
	{
		VkDescriptorBufferInfo renderingbufferinfo { UniformRenderingBuffer, 0, sizeof(UniformRenderingObject) };
		VkDescriptorImageInfo depthimageinfo       { FilteredDepthImageSampler, FilteredDepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo thickimageinfo       { ThickImageSampler, ThickImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo boxdepthimageinfo    { DepthImageSampler, DepthImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo causticsinputinfo    { CausticsImageSampler2, CausticsImageView2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo causticsoutinfo      { nullptr, CausticsImageView, VK_IMAGE_LAYOUT_GENERAL };

		std::vector<VkWriteDescriptorSet> writes(6);
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{depthimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{thickimageinfo,      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{boxdepthimageinfo,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{causticsinputinfo,   VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{causticsoutinfo,     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, CausticsBlurDescriptorSet2);
	}
	{
		// FilterDescriptorSet: X方向滤波 (CustomDepthImage -> FilteredDepthImage2)
		VkDescriptorBufferInfo renderingbufferinfo  { UniformRenderingBuffer,  0,  sizeof(UniformRenderingObject) };
		VkDescriptorImageInfo customdepthimageinfo  { CustomDepthImageSampler, CustomDepthImageView,    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo filtereddepthimageinfo2{ nullptr,                FilteredDepthImageView2, VK_IMAGE_LAYOUT_GENERAL };
		
		std::vector<VkWriteDescriptorSet> writes(3);
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo,            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{customdepthimageinfo,           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{filtereddepthimageinfo2,        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, FilterDescriptorSet);
	}
	{
		// FilterDescriptorSet2: Y方向滤波 (FilteredDepthImage2 -> FilteredDepthImage)
		VkDescriptorBufferInfo renderingbufferinfo  { UniformRenderingBuffer,  0,  sizeof(UniformRenderingObject) };
		VkDescriptorImageInfo filtereddepthimageinfo2{ FilteredDepthImageSampler2, FilteredDepthImageView2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo filtereddepthimageinfo { nullptr,                    FilteredDepthImageView,  VK_IMAGE_LAYOUT_GENERAL };
		
		std::vector<VkWriteDescriptorSet> writes2(3);
		std::vector<WriteDesc> writeInfo2 = {
			{renderingbufferinfo,            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{filtereddepthimageinfo2,        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
			{filtereddepthimageinfo,         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
		};
		Base::DescriptorWrite(LDevice, writeInfo2, writes2, FilterDescriptorSet2);
	}
}

VkImageView Renderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
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

void Renderer::SetRenderingObj(const UniformRenderingObject& robj)
{
	if (Initialized) {
		vkQueueWaitIdle(GraphicNComputeQueue);
		memcpy(MappedRenderingBuffer, &robj, sizeof(UniformRenderingObject));
	}
	else {
		renderingobj = robj;
	}

}

void Renderer::SetSimulatingObj(const UniformSimulatingObject& sobj)
{
    if (Initialized) {
        vkQueueWaitIdle(GraphicNComputeQueue);
        UniformSimulatingObject next = sobj;
        if (next.numParticles == 0 && simulatingobj.numParticles != 0) {
            next.numParticles = simulatingobj.numParticles;
        }
        if (next.cubeParticleNum == 0 && simulatingobj.cubeParticleNum != 0) {
            next.cubeParticleNum = simulatingobj.cubeParticleNum;
        }
        if (next.rigidParticles == 0 && simulatingobj.rigidParticles != 0) {
            next.rigidParticles = simulatingobj.rigidParticles;
        }
        simulatingobj = next;
        memcpy(MappedSimulatingBuffer, &simulatingobj, sizeof(UniformSimulatingObject));
    }
    else {
        simulatingobj = sobj;
    }
}

void Renderer::SetBoxinfoObj(const UniformBoxInfoObject& bobj)
{
	if (Initialized) {
		vkQueueWaitIdle(GraphicNComputeQueue);
		memcpy(MappedBoxInfoBuffer, &bobj, sizeof(UniformBoxInfoObject));
	}
	else {
		boxinfobj = bobj;
	}
}

void Renderer::SetParticles(const std::vector<Particle>& ps)
{
	if (Initialized) {
		throw std::runtime_error("you should not set particles after vulkan initialized!");
	}
	else if (ps.size() >= ONE_GROUP_INVOCATION_COUNT * ONE_GROUP_INVOCATION_COUNT * ONE_GROUP_INVOCATION_COUNT) {
		throw std::runtime_error("num of particles is too big!");
	}
	else {
		particles.assign(ps.begin(), ps.end());
	}
}

void Renderer::SetCubeParticles(const std::vector<CubeParticle>& ps)
{
	if (Initialized) {
		throw std::runtime_error("you should not set cube particles after vulkan initialized!");
	}
	else {
		cubeParticles.assign(ps.begin(), ps.end());
	}
}

void Renderer::SetRigidParticles(const std::vector<ExtraParticle>& ps)
{
	if (Initialized) {
		throw std::runtime_error("you should not set rigid particles after vulkan initialized!");
	}
	else {
		rigidParticles.assign(ps.begin(), ps.end());
		originRigidParticles = rigidParticles;
	}
}

void Renderer::SetShapeMatching(const ShapeMatchingInfo& s)
{
	smi = s;
	smiVec.push_back(smi);
}

void Renderer::SetCells(const uint32_t& cellCount)
{
	this->cellCount = cellCount;
}

void Renderer::SetFPS(float fps) {
	this->fps = fps;
}

void Renderer::SetValues(const bool& value) {
	this->mpmAlgorithm = value;
}

void Renderer::SetMPM2(const bool& value) {
	this->mpm2Algorithm = value;
}

void Renderer::SetMPM2Incompressible(const bool& value) {
	this->mpm2Incompressible = value;
}

void Renderer::SetWCSPH(const bool& value) {
	this->wcsphAlgorithm = value;
	if (value) {
		this->sphVulkanAlgorithm = false;
		this->mpmAlgorithm = false;
	}
}

void Renderer::SetSPHVulkan(const bool& value) {
	this->sphVulkanAlgorithm = value;
	if (value) {
		this->startDiffuseParticles = false;
		this->wcsphAlgorithm = true;
		this->mpmAlgorithm = false;
	}
}

Renderer::Renderer(uint32_t w, uint32_t h, bool validation)
{
	Width = w;
	Height = h;
	bEnableValidation = validation;
	// simulatingobj 的各向异性参数由其构造函数自动初始化
}

Renderer::~Renderer()
{
	if (specialParticleGenerate)
		delete specialParticle;
	if (rigidCubeGenerate)
		delete rigidCube;
	if (isSDF2)
		delete sdf;
	if (clothGenerate)
		delete cloth;
	if (mpmAlgorithm) {
		if (mpm2Algorithm) {
			delete mpm2;
		}
		else {
			delete mpm;
		}
	}
	delete scene_set;
}

void Renderer::Init()
{	
	// 0.529f, 0.808f, 0.922f, 0.5f  origin fluid color
	colorWithAlpha[0] = 0.529f;
	colorWithAlpha[1] = 0.808f;
	colorWithAlpha[2] = 0.922f;
	colorWithAlpha[3] = 0.5f;
	glfwInit();
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	Window = glfwCreateWindow(Width, Height, "fluids engine", nullptr, nullptr);

	glfwSetWindowUserPointer       (Window, this);
	glfwSetFramebufferSizeCallback (Window, &Renderer::WindowResizeCallback);
	glfwSetMouseButtonCallback     (Window, &Renderer::MouseButtonCallback);
	glfwSetCursorPosCallback       (Window, &Renderer::MouseMoveCallback);
	glfwSetScrollCallback          (Window, &Renderer::MouseScrollCallback);
	glfwSetKeyCallback			   (Window, &Renderer::KeyCallback);

	CreateBasicDevices();

	scene_set = new SceneSet(PDevice, LDevice, CommandPool, GraphicNComputeQueue);
	CreateUnifiedObjModel();
	CreateCubeParticleBuffer();
	CreateRigidParticleBuffer();
	CreateParticleBuffer();
	if (startAnisotrophic) {
		CreateAnisotrophicBuffer();
	}
	if (startDiffuseParticles) {
		CreateDiffuseParticleBuffer();
		CreateSurfaceParticleBuffer();
	}
	CreateSkyboxBuffer();
	CreateSphereBuffer();
	CreateShapeMatchingBuffer();
	CreateEllipsoidBuffer();

	CreateCellInfoBuffer();
	CreateNgbrCountBuffer();
	CreateNgbrCpyCountBuffer();
	CreateParticleInCellBuffer();
	CreateCellOffsetBuffer();
	CreateTestPBFBuffer();

	CreateSDFBuffer();
	CreateWCSPHBuffers();

	CreateUniformRenderingBuffer();
	CreateUniformSimulatingBuffer();
	CreateUniformBoxInfoBuffer();
	CreateUniformCubeBuffer();

	CreateDepthResources();
	CreateThickResources();
	CreateCausticsResources();
	CreateDefaultTextureResources();
	CreateBackgroundResources();
	CreateCubeResources();

	CreateDescriptorSetLayout();
	CreateDescriptorSet();
	CreateRenderPass();
	CreateGraphicPipelineLayout();
	CreateGraphicPipeline();
	CreateSkyboxGraphicPipeline();
	CreateSphereGraphicPipeline();
	CreateDragonGraphicPipeline();

	CreateComputePipelineLayout();
	CreateComputePipeline();

	CreateFramebuffers();

	if (mpmAlgorithm) {
		if (mpm2Algorithm) {
			// MPM2 Algorithm
			const glm::vec3 worldMin = glm::vec3(boxinfobj.clampX.x, boxinfobj.clampY.x, boxinfobj.clampZ.x);
			const glm::vec3 worldMax = glm::vec3(boxinfobj.clampX.y, boxinfobj.clampY.y, boxinfobj.clampZ.y);
			mpm2 = new MPM2Sim();
			mpm2->PreCreateResources(PDevice, LDevice, CommandPool, GraphicNComputeQueue, DescriptorPool,
				ParticleBuffers, particles, simulatingobj.dt, worldMin, worldMax, mpm2Incompressible);
			SimulatingCommandBuffers = mpm2->GetSimCmdBuffer();
		}
		else {
			// MPM Algorithm
			const glm::vec3 worldMin = glm::vec3(boxinfobj.clampX.x, boxinfobj.clampY.x, boxinfobj.clampZ.x);
			const glm::vec3 worldMax = glm::vec3(boxinfobj.clampX.y, boxinfobj.clampY.y, boxinfobj.clampZ.y);
			mpm = new MPMSim();
			mpm->PreCreateResources(PDevice, LDevice, CommandPool, GraphicNComputeQueue, DescriptorPool,
				ParticleBuffers, particles, renderingobj, worldMin, worldMax);
			SimulatingCommandBuffers = mpm->GetSimCmdBuffer();
		}
	}
	else {
		// PBF Algorithm
		RecordSimulatingCommandBuffers();
	}
	RecordFluidsRenderingCommandBuffers();
	RecordRigidRenderingCommandBuffers();
	RecordBoxRenderingCommandBuffers();
	
	// initialize ImGui
	CreateImGuiResources();

	// special particles
	if (specialParticleGenerate) {
		specialParticle = new SpecialParticle();
		specialParticle->PreCreateResources(
			PDevice, LDevice, GraphicNComputeQueue, SwapChainImageFormat, SwapChainImageExtent, CommandPool,
			DescriptorPool, &renderingobj, Window, SwapChainImageViews
		);
	}
	// cloth 
	if (clothGenerate) {
		cloth = new Cloth();
		cloth->Init(PDevice, LDevice, CommandPool, GraphicNComputeQueue, SwapChainImageExtent, SwapChainImageFormat,
			SwapChainImageViews, DescriptorPool, &renderingobj, Window);
	}
	Initialized = true;
}
void Renderer::Cleanup()
{
	vkDeviceWaitIdle(LDevice);

	// commandbuffers - clean
	vkFreeCommandBuffers(LDevice, CommandPool, MAXInFlightRendering, SimulatingCommandBuffers.data());
	for (uint32_t i = 0; i < 2; ++i)
		vkFreeCommandBuffers(LDevice, CommandPool, SwapChainImages.size(), FluidsRenderingCommandBuffers[i].data());
	for (uint32_t i = 0; i < 2; ++i)
		vkFreeCommandBuffers(LDevice, CommandPool, SwapChainImages.size(), RigidRenderingCommandBuffers[i].data());
	vkFreeCommandBuffers(LDevice, CommandPool, 1, &BoxRenderingCommandBuffer);

	// pipelines - clean
	std::vector<VkPipeline* > simulatePipelines = {
		&SimulatePipeline_ResetGrid, &SimulatePipeline_Partition, &SimulatePipeline_MergeSort1, &SimulatePipeline_MergeSort2,
		&SimulatePipeline_MergeSortAll, &SimulatePipeline_IndexMapped,
		&SimulatePipeline_Euler, &SimulatePipeline_Mass, &SimulatePipeline_Lambda, &SimulatePipeline_ResetRigid, &SimulatePipeline_DeltaPosition,
		&SimulatePipeline_PositionUpd, &SimulatePipeline_VelocityUpd, &SimulatePipeline_VelocityCache,
		&SimulatePipeline_ViscosityCorr, &SimulatePipeline_VorticityCorr,
		&SimulatePipeline_ComputeCM, &SimulatePipeline_ComputeCM2, &SimulatePipeline_ComputeApq, &SimulatePipeline_ComputeRotate,
		&SimulatePipeline_RigidDeltaPosition, &SimulatePipeline_RigidPositionUpd, &SimulatePipeline_EllipsoidUpd,
		&SimulatePipeline_WCSPH_Euler, &SimulatePipeline_WCSPH_Pressure, &SimulatePipeline_WCSPH_PressureForce,
		&SimulatePipeline_WCSPH_Advect
	};
	std::vector<VkPipeline* > graphicPipelines = {
		&FilterPipeline, &FilterPipeline2, &PostprocessPipeline, &CausticsPipeline, &CausticsBlurPipeline, &CausticsBlurPipeline2, &FluidGraphicPipeline, &FluidGraphicPipeline2, &FluidGraphicPipeline3,
		&BoxGraphicPipeline, &sphere_resource.graphicPipeline
	};
	if (startDiffuseParticles) {
		graphicPipelines.push_back(&DiffuseGraphicPipeline);
	}
	std::vector<VkPipelineLayout* > pipelineLayouts = {
		&SimulatePipelineLayout, &PostprocessPipelineLayout, &FilterPipelineLayout, &CausticsPipelineLayout, &FluidGraphicPipelineLayout,
		&BoxGraphicPipelineLayout, &sphere_resource.pipelineLayout
	};

	if (startAnisotrophic) {
		simulatePipelines.push_back(&SimulatePipeline_Anisotrophic);
	}
	if (startDiffuseParticles) {
		simulatePipelines.push_back(&SimulatePipeline_ResetDiffuse);
		simulatePipelines.push_back(&SimulatePipeline_SurfaceDetection);
		simulatePipelines.push_back(&SimulatePipeline_FoamGeneration);
		simulatePipelines.push_back(&SimulatePipeline_FoamUpd);
		simulatePipelines.push_back(&SimulatePipeline_Compact);
	}

	for (auto& simulatePipeline : simulatePipelines) {
		if (*simulatePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(LDevice, *simulatePipeline, Allocator);
			*simulatePipeline = VK_NULL_HANDLE;
		}
	}
	for (auto& graphicPipeline : graphicPipelines) {
		if (*graphicPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(LDevice, *graphicPipeline, Allocator);
			*graphicPipeline = VK_NULL_HANDLE;
		}
	}
	for (auto& pipelinelayout : pipelineLayouts) {
		if (*pipelinelayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(LDevice, *pipelinelayout, Allocator);
			*pipelinelayout = VK_NULL_HANDLE;
		}
	}

	// 销毁DescriptorPool
	vkDestroyDescriptorPool(LDevice, DescriptorPool, Allocator);

	// renderpass - clean
	vkDestroyRenderPass(LDevice, FluidGraphicRenderPass, Allocator);
	vkDestroyRenderPass(LDevice, FluidGraphicRenderPass2, Allocator);
	vkDestroyRenderPass(LDevice, BoxGraphicRenderPass, Allocator);

	// desciptorlayouts - clean
	std::vector<VkDescriptorSetLayout* > descriptorLayouts = {
		&BoxGraphicDescriptorSetLayout, &FluidGraphicDescriptorSetLayout, &sphere_resource.descriptorSetLayout,
		&SimulateDescriptorSetLayout, &FilterDecsriptorSetLayout, &PostprocessDescriptorSetLayout, &CausticsDescriptorSetLayout
	};
	for (auto& descriptorlayout : descriptorLayouts) {
		if (*descriptorlayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(LDevice, *descriptorlayout, Allocator);
			*descriptorlayout = VK_NULL_HANDLE;
		}
	}

	// framebuffer - clean
	vkDestroyFramebuffer(LDevice, FluidsFramebuffer, Allocator);
	vkDestroyFramebuffer(LDevice, BoxFramebuffer, Allocator);
	vkDestroyFramebuffer(LDevice, FluidsFramebuffer2, Allocator);

	// image | imageview | imagememory | imagesampler - clean
	vkDestroySampler(LDevice, DepthImageSampler, Allocator);
	vkDestroyImageView(LDevice, DepthImageView, Allocator);
	vkDestroyImage(LDevice, DepthImage, Allocator);
	vkFreeMemory(LDevice, DepthImageMemory, Allocator);

	vkDestroySampler(LDevice, CustomDepthImageSampler, Allocator);
	vkDestroyImageView(LDevice, CustomDepthImageView, Allocator);
	vkDestroyImage(LDevice, CustomDepthImage, Allocator);
	vkFreeMemory(LDevice, CustomDepthImageMemory, Allocator);

	vkDestroySampler(LDevice, CustomDepthImageSampler2, Allocator);
	vkDestroyImageView(LDevice, CustomDepthImageView2, Allocator);
	vkDestroyImage(LDevice, CustomDepthImage2, Allocator);
	vkFreeMemory(LDevice, CustomDepthImageMemory2, Allocator);

	vkDestroySampler(LDevice, FilteredDepthImageSampler, Allocator);
	vkDestroyImageView(LDevice, FilteredDepthImageView, Allocator);
	vkDestroyImage(LDevice, FilteredDepthImage, Allocator);
	vkFreeMemory(LDevice, FilteredDepthImageMemory, Allocator);

	vkDestroySampler(LDevice, FilteredDepthImageSampler2, Allocator);
	vkDestroyImageView(LDevice, FilteredDepthImageView2, Allocator);
	vkDestroyImage(LDevice, FilteredDepthImage2, Allocator);
	vkFreeMemory(LDevice, FilteredDepthImageMemory2, Allocator);

	vkDestroySampler(LDevice, ThickImageSampler, Allocator);
	vkDestroyImageView(LDevice, ThickImageView, Allocator);
	vkDestroyImage(LDevice, ThickImage, Allocator);
	vkFreeMemory(LDevice, ThickImageMemory, Allocator);

	vkDestroySampler(LDevice, ThickImageSampler2, Allocator);
	vkDestroyImageView(LDevice, ThickImageView2, Allocator);
	vkDestroyImage(LDevice, ThickImage2, Allocator);
	vkFreeMemory(LDevice, ThickImageMemory2, Allocator);

	vkDestroySampler(LDevice, CausticsImageSampler, Allocator);
	vkDestroyImageView(LDevice, CausticsImageView, Allocator);
	vkDestroyImage(LDevice, CausticsImage, Allocator);
	vkFreeMemory(LDevice, CausticsImageMemory, Allocator);

	vkDestroySampler(LDevice, CausticsImageSampler2, Allocator);
	vkDestroyImageView(LDevice, CausticsImageView2, Allocator);
	vkDestroyImage(LDevice, CausticsImage2, Allocator);
	vkFreeMemory(LDevice, CausticsImageMemory2, Allocator);

	vkDestroySampler(LDevice, DefaultTextureImageSampler, Allocator);
	vkDestroyImageView(LDevice, DefaultTextureImageView, Allocator);
	vkDestroyImage(LDevice, DefaultTextureImage, Allocator);
	vkFreeMemory(LDevice, DefaultTextureImageMemory, Allocator);

	vkDestroySampler(LDevice, ObjTextureImageSampler, Allocator);
	vkDestroyImageView(LDevice, ObjTextureImageView, Allocator);
	vkDestroyImage(LDevice, ObjTextureImage, Allocator);
	vkFreeMemory(LDevice, ObjTextureImageMemory, Allocator);

	vkDestroySampler(LDevice, BackgroundImageSampler, Allocator);
	vkDestroyImageView(LDevice, BackgroundImageView, Allocator);
	vkDestroyImage(LDevice, BackgroundImage, Allocator);
	vkFreeMemory(LDevice, BackgroundImageMemory, Allocator);

	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		CleanupBuffer(ParticleBuffers[i], ParticleBufferMemory[i], false);
		CleanupBuffer(CubeParticleBuffers[i], CubeParticleBufferMemory[i], false);
		CleanupBuffer(RigidParticleBuffers[i], RigidParticleBufferMemory[i], false);
		CleanupBuffer(OriginRigidParticleBuffers[i], OriginRigidParticleBufferMemory[i], false);
		CleanupBuffer(ShapeMatchingBuffers[i], ShapeMatchingBufferMemory[i], false);
	}
	if (!ellipsoidBuffers.empty()) {
		for (uint32_t i = 0; i < MAXInFlightRendering && i < ellipsoidBuffers.size(); ++i) {
			if (ellipsoidBuffers[i] != VK_NULL_HANDLE) {
				CleanupBuffer(ellipsoidBuffers[i], ellipsoidBufferMemory[i], false);
			}
		}
	} 
	if (startDiffuseParticles) {
		for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
			CleanupBuffer(DiffuseParticleBuffers[i], DiffuseParticleBufferMemory[i], false);
		}
		for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
			CleanupBuffer(DiffuseParticleCountBuffers[i], DiffuseParticleCountBufferMemory[i], false);
		}
		CleanupBuffer(SurfaceParticleBuffer, SurfaceParticleBufferMemory, false);
	}
	CleanupBuffer(UniformRenderingBuffer, UniformRenderingBufferMemory, true);
	CleanupBuffer(UniformSimulatingBuffer, UniformSimulatingBufferMemory, true);
	CleanupBuffer(UniformBoxInfoBuffer, UniformBoxInfoBufferMemory, true);
	CleanupBuffer(cubeUniformBuffer, cubeUniformBufferMemory, true);

	CleanupBuffer(CellInfoBuffer, CellInfoBufferMemory, false);
	CleanupBuffer(NeighborCountBuffer, NeighborCountBufferMemory, false);
	CleanupBuffer(NeighborCpyCountBuffer, NeighborCpyCountBufferMemory, false);
	CleanupBuffer(ParticleInCellBuffer, ParticleInCellBufferMemory, false);
	CleanupBuffer(CellOffsetBuffer, CellOffsetBufferMemory, false);
	CleanupBuffer(TestPBFBuffer, TestPBFBufferMemory, false);
	
	CleanupBuffer(SDFBuffer, SDFBufferMemory, false);
	CleanupBuffer(SDFDyBuffer, SDFDyBufferMemory, false);

	CleanupBuffer(WCSPHPressureBuffer, WCSPHPressureBufferMemory, false);
	CleanupBuffer(WCSPHExternalForceBuffer, WCSPHExternalForceBufferMemory, false);
	CleanupBuffer(WCSPHParamsBuffer, WCSPHParamsBufferMemory, true);

	if (startAnisotrophic) {
		CleanupBuffer(AnisotrophicBuffers[0], AnisotrophicBufferMemory[0], false);
	}

	CleanupBuffer(sphere_resource.vertexBuffer, sphere_resource.vertexBufferMemory, false);
	CleanupBuffer(sphere_resource.indexBuffer, sphere_resource.indexBufferMemory, false);

	Scene::CleanupSkybox(PDevice, LDevice, skybox_resource);
	Scene::CleanupObj(PDevice, LDevice, obj_r2,       true,  false, false, false, false);
	Scene::CleanupObj(PDevice, LDevice, obj_resource, false, false, false, false, false);
	for (int i = 2; i < objResManagements.size(); i++) {
		Scene::CleanupObj(PDevice, LDevice, *objResManagements[i].obj_res, objResManagements[i].isVox, objResManagements[i].isTex, objResManagements[i].is2Tex, objResManagements[i].isPBR, objResManagements[i].isTransform);
	}
	if (specialParticleGenerate) {
		specialParticle->Cleanup(LDevice, CommandPool);
	}
	if (rigidCubeGenerate) {
		rigidCube->Cleanup(LDevice);
	}
	if (mpmAlgorithm) {
		if (mpm2Algorithm && mpm2) {
			mpm2->Cleanup(LDevice);
		}
		if (!mpm2Algorithm && mpm) {
			mpm->Cleanup(LDevice, CommandPool);
		}
	}
	if (clothGenerate && cloth) {
		cloth->Cleanup(LDevice, CommandPool);
	}
	if (sdf && isSDF2) {
		sdf->Cleanup(LDevice);
	}

	// basic resources - clean
	BaseCleanup();
}

void Renderer::BaseCleanup()
{
	CleanupSwapChain();
	CleanupImGui();
	// DescriptorPool已在Cleanup()中销毁

	vkDestroyCommandPool(LDevice, CommandPool, Allocator);
	CleanupSupportObjects();

	vkDestroyDevice(LDevice, Allocator);
	vkDestroySurfaceKHR(Instance, Surface, Allocator);
	ExtensionFuncs::vkDestroyDebugUtilsMessengerEXT(Instance, Messenger, Allocator);
	vkDestroyInstance(Instance, Allocator);

	glfwDestroyWindow(Window);
	glfwTerminate();
}

void Renderer::CreateSupportObjects()
{
	DrawingFence.resize(MAXInFlightRendering);
	ImageAvaliable.resize(MAXInFlightRendering);
	FluidsRenderingFinish.resize(MAXInFlightRendering);
	BoxRenderingFinish.resize(MAXInFlightRendering);
	SimulatingFinish.resize(MAXInFlightRendering);
	CubesRenderingFinish.resize(MAXInFlightRendering);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.flags = VK_SEMAPHORE_TYPE_BINARY;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAXInFlightRendering; i++) {
		if (vkCreateSemaphore(LDevice, &semaphoreInfo, Allocator, &ImageAvaliable[i]) != VK_SUCCESS ||
			vkCreateSemaphore(LDevice, &semaphoreInfo, Allocator, &FluidsRenderingFinish[i]) != VK_SUCCESS ||
			vkCreateSemaphore(LDevice, &semaphoreInfo, Allocator, &BoxRenderingFinish[i]) != VK_SUCCESS ||
			vkCreateSemaphore(LDevice, &semaphoreInfo, Allocator, &SimulatingFinish[i]) != VK_SUCCESS ||
			vkCreateSemaphore(LDevice, &semaphoreInfo, Allocator, &CubesRenderingFinish[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics objects!");
		}

		if (vkCreateFence(LDevice, &fenceInfo, Allocator, &DrawingFence[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create drawing fence!");
		}
	}
}
void Renderer::CleanupSupportObjects()
{
	for (size_t i = 0; i < MAXInFlightRendering; i++) {
		vkDestroySemaphore(LDevice, ImageAvaliable[i], Allocator);
		vkDestroySemaphore(LDevice, FluidsRenderingFinish[i], Allocator);
		vkDestroySemaphore(LDevice, BoxRenderingFinish[i], Allocator);
		vkDestroySemaphore(LDevice, SimulatingFinish[i], Allocator);
		vkDestroySemaphore(LDevice, CubesRenderingFinish[i], Allocator);
		vkDestroyFence(LDevice, DrawingFence[i], Allocator);
	}
}

void Renderer::CreateBasicDevices()
{
	// instance & surface & physical device & logical device
	Base::CreateInstance(bEnableValidation, &Instance);

	Base::CreateDebugMessenger(Instance, Messenger);

	if (glfwCreateWindowSurface(Instance, Window, Allocator, &Surface) != VK_SUCCESS) {
		throw std::runtime_error("faile to create surface!");
	}
	Base::PickPhysicalDevice(Instance, PDevice, Surface);

	Base::CreateLogicalDevice(PDevice, Surface, GraphicNComputeQueue, PresentQueue, LDevice);

	// command pool and descriptor pool 
	Base::CreateCommandPool(PDevice, Surface, LDevice, CommandPool);
	Base::CreateDescriptorPool(LDevice, DescriptorPool);

	// swapchain
	Base::CreateSwapChain(PDevice, Surface, LDevice, GraphicNComputeQueue, PresentQueue, SwapChain, SwapChainImageFormat, SwapChainImageExtent,
		SwapChainImages, SwapChainImageViews);

	// semaphores and fences
	CreateSupportObjects();
}

void Renderer::CreateParticleBuffer()
{
	int maxParticleNums = simulatingobj.maxParticles + simulatingobj.maxCubeParticles + rigidParticles.size();

	if (maxParticleNums % ONE_GROUP_INVOCATION_COUNT != 0) {
		WORK_GROUP_COUNT = maxParticleNums / ONE_GROUP_INVOCATION_COUNT + 1;
	}
	else {
		WORK_GROUP_COUNT = maxParticleNums / ONE_GROUP_INVOCATION_COUNT;
	}

	simulatingobj.numParticles = static_cast<uint32_t>(particles.size());

	ParticleBufferMemory.resize(MAXInFlightRendering);
	ParticleBuffers.resize(MAXInFlightRendering);
	VkDeviceSize size = static_cast<VkDeviceSize>(simulatingobj.maxParticles) * sizeof(Particle);
	std::vector<Particle> initParticles(static_cast<size_t>(simulatingobj.maxParticles));
	const size_t copyCount = std::min(initParticles.size(), particles.size());
	if (copyCount > 0) {
		memcpy(initParticles.data(), particles.data(), copyCount * sizeof(Particle));
	}

	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, ParticleBuffers[i], ParticleBufferMemory[i], size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, initParticles.data(), ParticleBuffers[i]);
	}
}

void Renderer::UploadFluidParticlesSubrange(uint32_t startParticle, const std::vector<Particle>& newParticles)
{
	if (newParticles.empty()) {
		return;
	}
	if (startParticle >= simulatingobj.maxParticles) {
		return;
	}

	const uint32_t lastFrame = (CurrentFlight + MAXInFlightRendering - 1) % MAXInFlightRendering;
	const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(newParticles.size()) * sizeof(Particle);
	const VkDeviceSize dstOffset = static_cast<VkDeviceSize>(startParticle) * sizeof(Particle);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingMemory, uploadSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* data;
	vkMapMemory(LDevice, stagingMemory, 0, uploadSize, 0, &data);
	memcpy(data, newParticles.data(), static_cast<size_t>(uploadSize));
	vkUnmapMemory(LDevice, stagingMemory);

	VkCommandBuffer cb = Util::CreateCommandBuffer(LDevice, CommandPool);
	VkBufferCopy copyRegion{};
	copyRegion.size = uploadSize;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = dstOffset;
	vkCmdCopyBuffer(cb, stagingBuffer, ParticleBuffers[lastFrame], 1, &copyRegion);

	VkSubmitInfo submitInfo{};
	Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitInfo, VK_NULL_HANDLE, GraphicNComputeQueue);
	CleanupBuffer(stagingBuffer, stagingMemory, false);
}

void Renderer::InjectWaterColumn()
{
	if (!injectorEnabled) {
		return;
	}
	if (injectorRate <= 0.0f || injectorRadius <= 0.0f) {
		return;
	}
	if (simulatingobj.numParticles >= simulatingobj.maxParticles) {
		return;
	}

	injectorAccumulator += injectorRate * simulatingobj.dt;
	uint32_t spawnCount = static_cast<uint32_t>(injectorAccumulator);
	const uint32_t available = simulatingobj.maxParticles - simulatingobj.numParticles;
	if (spawnCount > available) {
		spawnCount = available;
	}
	if (spawnCount == 0) {
		return;
	}
	injectorAccumulator -= static_cast<float>(spawnCount);

	const float spacing = std::max(simulatingobj.sphRadius * 0.5f, simulatingobj.fluidParticleRadius * 2.0f);
	const float margin = std::max(spacing * 2.0f, simulatingobj.fluidParticleRadius * 2.0f);
	const float xBase = (injectorSide == 0)
		? (boxinfobj.clampX.x + margin)
		: (boxinfobj.clampX.y - margin);
	const glm::vec3 baseVel = (injectorSide == 0)
		? glm::vec3(injectorSpeed, 0.0f, 0.0f)
		: glm::vec3(-injectorSpeed, 0.0f, 0.0f);

	static thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> uni01(0.0f, 1.0f);

	std::vector<Particle> newParticles;
	newParticles.reserve(spawnCount);
	for (uint32_t i = 0; i < spawnCount; ++i) {
		float u1 = uni01(rng);
		float u2 = uni01(rng);
		float r = injectorRadius * std::sqrt(u1);
		float theta = static_cast<float>(2.0 * M_PI) * u2;

		float y = injectorCenterY + r * std::cos(theta);
		float z = injectorCenterZ + r * std::sin(theta);
		float x = xBase + (uni01(rng) - 0.5f) * spacing * 0.25f;

		x = std::clamp(x, boxinfobj.clampX.x + margin, boxinfobj.clampX.y - margin);
		y = std::clamp(y, boxinfobj.clampY.x + margin, boxinfobj.clampY.y - margin);
		z = std::clamp(z, boxinfobj.clampZ.x + margin, boxinfobj.clampZ.y - margin);

		Particle p{};
		p.Location = glm::vec3(x, y, z);
		p.Velocity = baseVel;
		p.Mass = particles.empty() ? 1.0f : particles[0].Mass;
		p.NumNgbrs = 0;
		newParticles.push_back(p);
	}

	const uint32_t startIndex = simulatingobj.numParticles;
	UploadFluidParticlesSubrange(startIndex, newParticles);
	simulatingobj.numParticles += static_cast<uint32_t>(newParticles.size());
	auto pObj = reinterpret_cast<UniformSimulatingObject*>(MappedSimulatingBuffer);
	pObj->numParticles = simulatingobj.numParticles;
}

void Renderer::CreateDiffuseParticleBuffer()
{
	// diffuse particles
	DiffuseParticleBufferMemory.resize(MAXInFlightRendering);
	DiffuseParticleCountBufferMemory.resize(MAXInFlightRendering);
	DiffuseParticleBuffers.resize(MAXInFlightRendering);
	DiffuseParticleCountBuffers.resize(MAXInFlightRendering);
	int maxDiffuseParticleNums = simulatingobj.maxDiffuseParticles;
	diffuseParticles.resize(maxDiffuseParticleNums);
	dpc.activeCount = 0;
	dpc.surfaceParticleCount = 0;
	dpc.compactCount = 0;
	WORK_GROUP_COUNT_DIFFUSE = maxDiffuseParticleNums % ONE_GROUP_INVOCATION_COUNT == 0 ?
		maxDiffuseParticleNums / ONE_GROUP_INVOCATION_COUNT : maxDiffuseParticleNums / ONE_GROUP_INVOCATION_COUNT + 1;

	VkDeviceSize size_ = diffuseParticles.size() * sizeof(DiffuseParticle);
	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, DiffuseParticleBuffers[i], DiffuseParticleBufferMemory[i], size_,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size_, GraphicNComputeQueue, diffuseParticles.data(), DiffuseParticleBuffers[i]);
	}

	size_ = sizeof(DiffuseParticleCount);
	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, DiffuseParticleCountBuffers[i], DiffuseParticleCountBufferMemory[i],
			size_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size_, GraphicNComputeQueue, &dpc, DiffuseParticleCountBuffers[i]);
	}
}

void Renderer::CreateAnisotrophicBuffer()
{
	uint32_t fluidParticleCount = simulatingobj.maxParticles;
	anisotrophicMatrices.resize(fluidParticleCount);
	AnisotrophicBuffers.resize(1);
	AnisotrophicBufferMemory.resize(1);
	VkDeviceSize aniSize = fluidParticleCount * sizeof(AnisotrophicMatrix);
	for (uint32_t i = 0; i < 1; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, AnisotrophicBuffers[i], AnisotrophicBufferMemory[i], aniSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, aniSize, GraphicNComputeQueue, anisotrophicMatrices.data(), AnisotrophicBuffers[i]);
	}	
}

void Renderer::CreateCubeParticleBuffer()
{
	int cubeParticleNums = static_cast<int>(cubeParticles.size());
	VkDeviceSize size = static_cast<VkDeviceSize>(cubeParticleNums) * sizeof(CubeParticle);
	CubeParticle dummy{};
	const void* uploadSrc = cubeParticles.data();
	if (cubeParticleNums == 0) {
		cubeParticleNums = 1;
		size = sizeof(CubeParticle);
		uploadSrc = &dummy;
	}
	
	CubeParticleBufferMemory.resize(MAXInFlightRendering);
	CubeParticleBuffers.resize(MAXInFlightRendering);

	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, CubeParticleBuffers[i], CubeParticleBufferMemory[i], size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, uploadSrc, CubeParticleBuffers[i]);
	}
}

void Renderer::CreateRigidParticleBuffer()
{
	int cubeParticleNums = static_cast<int>(rigidParticles.size());
	VkDeviceSize size = static_cast<VkDeviceSize>(cubeParticleNums) * sizeof(ExtraParticle);
	ExtraParticle dummy{};
	const void* uploadSrc = rigidParticles.data();
	const void* uploadSrcOrigin = originRigidParticles.size() == rigidParticles.size() ? originRigidParticles.data() : rigidParticles.data();
	if (cubeParticleNums == 0) {
		cubeParticleNums = 1;
		size = sizeof(ExtraParticle);
		uploadSrc = &dummy;
		uploadSrcOrigin = &dummy;
	}

	RigidParticleBufferMemory.resize(MAXInFlightRendering);
	RigidParticleBuffers.resize(MAXInFlightRendering);
	OriginRigidParticleBufferMemory.resize(MAXInFlightRendering);
	OriginRigidParticleBuffers.resize(MAXInFlightRendering);

	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, RigidParticleBuffers[i], RigidParticleBufferMemory[i], size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, uploadSrc, RigidParticleBuffers[i]);
	}

	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, OriginRigidParticleBuffers[i], OriginRigidParticleBufferMemory[i], size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, uploadSrcOrigin, OriginRigidParticleBuffers[i]);
	}
}

void Renderer::CreateSkyboxBuffer()
{
	VkDeviceSize vertexBufferSize = sizeof(SkyboxVertex) * skyboxVertices.size();
	Buffer::CreateBuffer(PDevice, LDevice, skybox_resource.vertexBuffer, skybox_resource.vertexBufferMemory, vertexBufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* data;
	vkMapMemory(LDevice, skybox_resource.vertexBufferMemory, 0, vertexBufferSize, 0, &data);
	memcpy(data, skyboxVertices.data(), vertexBufferSize);
	vkUnmapMemory(LDevice, skybox_resource.vertexBufferMemory);

	VkDeviceSize indexBufferSize = sizeof(uint32_t) * skyboxIndices.size();
	Buffer::CreateBuffer(PDevice, LDevice, skybox_resource.indexBuffer, skybox_resource.indexBufferMemory, indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(LDevice, skybox_resource.indexBufferMemory, 0, indexBufferSize, 0, &data);
	memcpy(data, skyboxIndices.data(), indexBufferSize);
	vkUnmapMemory(LDevice, skybox_resource.indexBufferMemory);
}

void Renderer::CreateSphereBuffer()
{
	glm::vec3 translation = glm::vec3(40.0, 0.3, -2.0);
	Scene::CreateSphere(sphere_resource, translation, 0.6f);

	Buffer::CreateSphereVertexIndexBuffer(
		PDevice, LDevice, CommandPool, GraphicNComputeQueue, sphere_resource
	);
}

void Renderer::CreateShapeMatchingBuffer()
{
	VkDeviceSize size = sizeof(ShapeMatchingInfo);

	ShapeMatchingBuffers.resize(MAXInFlightRendering);
	ShapeMatchingBufferMemory.resize(MAXInFlightRendering);
	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, ShapeMatchingBuffers[i], ShapeMatchingBufferMemory[i], size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, &smi, ShapeMatchingBuffers[i]);
	}
}

void Renderer::CreateEllipsoidBuffer()
{
	VkDeviceSize size = sizeof(EllipsoidInfo);

	ellipsoidInfo.center = ellipCenter;
	ellipsoidInfo.radii = ellipRadii;
	ellipsoidInfo.model = glm::translate(glm::mat4(1.0f), ellipCenter);
	ellipsoidInfo.accumulatedForce = glm::vec3(0.0f);
	ellipsoidInfo.accumulatedTorque = glm::vec3(0.0f);
	ellipsoidInfo.velocity = glm::vec3(0.0f);
	ellipsoidInfo.angularVelocity = glm::vec3(0.0f);
	ellipsoidInfo.orientation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	ellipsoidBuffers.resize(MAXInFlightRendering);
	ellipsoidBufferMemory.resize(MAXInFlightRendering);

	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Buffer::CreateBuffer(PDevice, LDevice, ellipsoidBuffers[i], ellipsoidBufferMemory[i], size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer::CreateBufferWithStage(PDevice, LDevice, CommandPool, size, GraphicNComputeQueue, &ellipsoidInfo, ellipsoidBuffers[i]);
	}
}

//***********************************Create Uniform Buffers**************************************************************************
void Renderer::CreateUniformRenderingBuffer()
{
	VkDeviceSize size = sizeof(UniformRenderingObject);
	Buffer::CreateBuffer(PDevice, LDevice, UniformRenderingBuffer, UniformRenderingBufferMemory, size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(LDevice, UniformRenderingBufferMemory, 0, size, 0, &MappedRenderingBuffer);
	memcpy(MappedRenderingBuffer, &renderingobj, size);
}

void Renderer::CreateUniformSimulatingBuffer()
{
	VkDeviceSize size = sizeof(UniformSimulatingObject);
	Buffer::CreateBuffer(PDevice, LDevice, UniformSimulatingBuffer, UniformSimulatingBufferMemory, size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(LDevice, UniformSimulatingBufferMemory, 0, size, 0, &MappedSimulatingBuffer);
	memcpy(MappedSimulatingBuffer, &simulatingobj, size);
}

void Renderer::CreateUniformBoxInfoBuffer()
{
	VkDeviceSize size = sizeof(UniformBoxInfoObject);
	Buffer::CreateBuffer(PDevice, LDevice, UniformBoxInfoBuffer, UniformBoxInfoBufferMemory, size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(LDevice, UniformBoxInfoBufferMemory, 0, size, 0, &MappedBoxInfoBuffer);
	memcpy(MappedBoxInfoBuffer, &boxinfobj, size);
}

void Renderer::CreateUniformCubeBuffer()
{
	VkDeviceSize bufferSize = sizeof(CubeInfo);
	Buffer::CreateBuffer(PDevice, LDevice, cubeUniformBuffer, cubeUniformBufferMemory, bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(LDevice, cubeUniformBufferMemory, 0, bufferSize, 0, &mappedCubeBuffer);

	// 初始化立方体
	CubeInfo cubeInfo;
	//cubeInfo.position = glm::vec3(0.75f, 0.5f, 0.0f);
	//cubeInfo.size = glm::vec3(0.2f, 0.2f, 0.2f);
	cubeInfo.rotation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	cubeInfo.velocity = glm::vec3(0.0f);

	cubeInfo.position = glm::vec3(1.0f, 0.2f, 0.8f); 
	cubeInfo.size = glm::vec3(0.2f, 0.2f, 0.2f);     

	memcpy(mappedCubeBuffer, &cubeInfo, bufferSize);
}

// ***********************************************Merge Sort Resource  (0-5)*******************************************************************
void Renderer::CreateCellInfoBuffer()
{
	{
		VkDeviceSize size = sizeof(uint32_t) * (simulatingobj.maxParticles + simulatingobj.maxCubeParticles + rigidParticles.size());
		Buffer::CreateBuffer(PDevice, LDevice, CellInfoBuffer, CellInfoBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}
	//{
	//	VkDeviceSize size = sizeof(uint32_t) * cellCount;
	//	Buffer::CreateBuffer(PDevice, LDevice, NeighborCountBuffer, NeighborCountBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	//}
}

void Renderer::CreateNgbrCountBuffer()
{
	VkDeviceSize size = sizeof(uint32_t) * cellCount;
	Buffer::CreateBuffer(PDevice, LDevice, NeighborCountBuffer, NeighborCountBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void Renderer::CreateNgbrCpyCountBuffer()
{
	VkDeviceSize size = sizeof(uint32_t) * cellCount;
	Buffer::CreateBuffer(PDevice, LDevice, NeighborCpyCountBuffer, NeighborCpyCountBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void Renderer::CreateParticleInCellBuffer()
{
	VkDeviceSize size = sizeof(uint32_t) * (simulatingobj.maxParticles + simulatingobj.maxCubeParticles + rigidParticles.size());
	Buffer::CreateBuffer(PDevice, LDevice, ParticleInCellBuffer, ParticleInCellBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void Renderer::CreateCellOffsetBuffer()
{
	VkDeviceSize size = sizeof(uint32_t) * cellCount;
	Buffer::CreateBuffer(PDevice, LDevice, CellOffsetBuffer, CellOffsetBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void Renderer::CreateTestPBFBuffer()
{
	VkDeviceSize size = sizeof(uint32_t) * (simulatingobj.maxParticles + simulatingobj.maxCubeParticles + rigidParticles.size());
	Buffer::CreateBuffer(PDevice, LDevice, TestPBFBuffer, TestPBFBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void Renderer::CreateSurfaceParticleBuffer()
{
	VkDeviceSize size = sizeof(uint32_t) * simulatingobj.maxParticles;
	Buffer::CreateBuffer(PDevice, LDevice, SurfaceParticleBuffer, SurfaceParticleBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

//********************************************end <Merge Sort> resource creation***********************************************************

//************************************************** SDF Buffer ****************************************************************************
void Renderer::CreateSDFBuffer()
{
	VkDeviceSize size = sizeof(float) * VOXEL_RES * VOXEL_RES * VOXEL_RES;
	Buffer::CreateBuffer(PDevice, LDevice, SDFBuffer, SDFBufferMemory,     size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	Buffer::CreateBuffer(PDevice, LDevice, SDFDyBuffer, SDFDyBufferMemory, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void Renderer::CreateWCSPHBuffers()
{
	VkDeviceSize pressureSize = sizeof(float) * simulatingobj.maxParticles;
	Buffer::CreateBuffer(PDevice, LDevice, WCSPHPressureBuffer, WCSPHPressureBufferMemory, pressureSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDeviceSize externalForceSize = sizeof(glm::vec4) * simulatingobj.maxParticles;
	Buffer::CreateBuffer(PDevice, LDevice, WCSPHExternalForceBuffer, WCSPHExternalForceBufferMemory, externalForceSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDeviceSize paramsSize = sizeof(glm::vec4);
	Buffer::CreateBuffer(PDevice, LDevice, WCSPHParamsBuffer, WCSPHParamsBufferMemory, paramsSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkMapMemory(LDevice, WCSPHParamsBufferMemory, 0, paramsSize, 0, &mappedWCSPHParamsBuffer);

	glm::vec4 params(50.0f, 5.0f, 1.0f, 3000.0f);
	if (sphVulkanAlgorithm) {
		params = glm::vec4(50.0f, 0.0f, 3.0f, 800.0f);
	}
	memcpy(mappedWCSPHParamsBuffer, &params, paramsSize);
}

//
void Renderer::CreateDepthResources()
{
	VkExtent3D extent = { SwapChainImageExtent.width,SwapChainImageExtent.height,1 };
	Image::CreateImage(PDevice, LDevice, DepthImage, DepthImageMemory, extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
	DepthImageView = CreateImageView(DepthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
	Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue, 
		DepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	Image::CreateImage(PDevice, LDevice, CustomDepthImage, CustomDepthImageMemory, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
	CustomDepthImageView = CreateImageView(CustomDepthImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue, 
		CustomDepthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	Image::CreateImage(PDevice, LDevice, FilteredDepthImage, FilteredDepthImageMemory, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT);
	FilteredDepthImageView = CreateImageView(FilteredDepthImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

	Image::CreateImage(PDevice, LDevice, FilteredDepthImage2, FilteredDepthImageMemory2, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT);
	FilteredDepthImageView2 = CreateImageView(FilteredDepthImage2, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

	Image::CreateImage(PDevice, LDevice, CustomDepthImage2, CustomDepthImageMemory2, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
	CustomDepthImageView2 = CreateImageView(CustomDepthImage2, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue, 
		CustomDepthImage2, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerinfo{};
	samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo.anisotropyEnable = VK_FALSE;
	samplerinfo.minFilter = VK_FILTER_LINEAR;
	samplerinfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &CustomDepthImageSampler);
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &FilteredDepthImageSampler);
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &FilteredDepthImageSampler2);
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &CustomDepthImageSampler2);
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &DepthImageSampler);
}

void Renderer::CreateThickResources()
{
	VkExtent3D extent = { SwapChainImageExtent.width,SwapChainImageExtent.height,1 };
	Image::CreateImage(PDevice, LDevice, ThickImage, ThickImageMemory, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
	ThickImageView = CreateImageView(ThickImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue, 
		ThickImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerinfo{};
	samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo.anisotropyEnable = VK_FALSE;
	samplerinfo.minFilter = VK_FILTER_LINEAR;
	samplerinfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &ThickImageSampler);

	Image::CreateImage(PDevice, LDevice, ThickImage2, ThickImageMemory2, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
	ThickImageView2 = CreateImageView(ThickImage2, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue, 
		ThickImage2, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerinfo2{};
	samplerinfo2.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo2.anisotropyEnable = VK_FALSE;
	samplerinfo2.minFilter = VK_FILTER_LINEAR;
	samplerinfo2.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(LDevice, &samplerinfo2, Allocator, &ThickImageSampler2);
}

void Renderer::CreateCausticsResources()
{
	VkExtent3D extent = { SwapChainImageExtent.width,SwapChainImageExtent.height,1 };
	Image::CreateImage(PDevice, LDevice, CausticsImage, CausticsImageMemory, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT);
	CausticsImageView = CreateImageView(CausticsImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkSamplerCreateInfo samplerinfo{};
	samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo.anisotropyEnable = VK_FALSE;
	samplerinfo.minFilter = VK_FILTER_LINEAR;
	samplerinfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &CausticsImageSampler);

	Image::CreateImage(PDevice, LDevice, CausticsImage2, CausticsImageMemory2, extent, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT);
	CausticsImageView2 = CreateImageView(CausticsImage2, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	VkSamplerCreateInfo samplerinfo2{};
	samplerinfo2.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo2.anisotropyEnable = VK_FALSE;
	samplerinfo2.minFilter = VK_FILTER_LINEAR;
	samplerinfo2.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(LDevice, &samplerinfo2, Allocator, &CausticsImageSampler2);

	{
		auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		barrier.image = CausticsImage;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		barrier.image = CausticsImage2;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkSubmitInfo submitinfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);
	}
}

void Renderer::CreateObjModelResources(std::string& obj_path, ObjResource& objRes, 
	glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation, bool useParticle, bool useVoxBox, bool useGlb)
{
	std::string obj_file_path = obj_path;

	// store index and vertex data from .obj or .glb or other ...
	if (!useGlb)
		Scene::CreateObjModelResources_PolygonSupport(obj_file_path, objRes, translation, scale_rate, rotation);
	else
		Scene::LoadGLBModel(obj_file_path, objRes, translation, scale_rate, rotation);

	if (objRes.vertices.empty() || objRes.indices.empty()) {
		objRes.vertexBuffer = VK_NULL_HANDLE;
		objRes.vertexBufferMemory = VK_NULL_HANDLE;
		objRes.indexBuffer = VK_NULL_HANDLE;
		objRes.indexBufferMemory = VK_NULL_HANDLE;
		return;
	}

	// pretend to use particle 
	if (useParticle)
	{
		glm::vec3 startPos = glm::vec3(0.0, 1.0, 0.0);
		float radius = renderingobj.particleRadius;
		uint32_t num_x = 10, num_y = 10, num_z = 10;
		for (uint32_t ix = 0; ix < num_x; ix++) {
			for (uint32_t iy = 0; iy < num_y; iy++) {
				for (uint32_t iz = 0; iz < num_z; iz++) {
					CubeParticle particle{};
					particle.Location = glm::vec3(
						ix * 2 * radius,
						iy * 2 * radius,
						iz * 2 * radius) + startPos;
					particle.Velocity = glm::vec3(0.0, 0.0, 0.0);
					cubeParticles.push_back(particle);
				}
			}
		}
		simulatingobj.cubeParticleNum = cubeParticles.size();
		SetSimulatingObj(simulatingobj);
	}

	// compute vox box for voxelization
	if (useVoxBox)
	{		
		objRes.boxMin = glm::vec3(std::numeric_limits<float>::max());
		objRes.boxMax = glm::vec3(std::numeric_limits<float>::lowest());
		for (const auto& vertex : objRes.vertices) {
			objRes.boxMin = glm::min(objRes.boxMin, vertex.pos);
			objRes.boxMax = glm::max(objRes.boxMax, vertex.pos);
		}
		objRes.voxelSize = glm::max(objRes.boxMax.x - objRes.boxMin.x,
			glm::max(objRes.boxMax.y - objRes.boxMin.y, objRes.boxMax.z - objRes.boxMin.z)) / VOXEL_RES;

		// uniform struct
		objRes.voxinfobj.boxMin = objRes.boxMin;
		objRes.voxinfobj.resolution = glm::ivec3(VOXEL_RES, VOXEL_RES, VOXEL_RES);
		objRes.voxinfobj.voxelSize = objRes.voxelSize;
	}
	
	Buffer::CreateObjVertexIndexBuffer(PDevice, LDevice, CommandPool, GraphicNComputeQueue, objRes);
}

void Renderer::CreateUnifiedObjModel()
{
	// two models with voxel test
	std::string str1 = CUP_PATH, str2 = DRAGON_PATH;
	// can be for imgui parameters
	glm::vec3 translation1 = glm::vec3(1.3, 1.0, 0.2);
	glm::vec3 scale_rate1 = glm::vec3(1.0, 1.0, 1.0);
	glm::mat3 rotation1 = glm::mat3(1.0);
	CreateObjModelResources(str1, obj_resource, translation1, scale_rate1, rotation1, false, false, false);
	CreateObjModelResources(str2, obj_r2, translation1, scale_rate1, rotation1, true, true, false);

	// which obj is need to be voxelized
	Scene::VoxelizeObj(PDevice, LDevice, obj_r2, false);
	// store these objs address
	// @parameter: &obj_resource | isVox | isTex | isGlb
	objResManagements.push_back({ &obj_resource, false, false, false });
	objResManagements.back().displayName = "Cup";
	objResManagements.push_back({ &obj_r2,       true,  false, false });
	objResManagements.back().displayName = "Dragon (Vox)";

	scene_set->SetSceneBoundry(boxinfobj);
	scene_set->PrepareScenes(objResManagements, SceneType::SWIMMINGPOOL);

	ObjResource* duckFloat = scene_set->GetObjResource_DuckFloat();
	Scene::VoxelizeObj(PDevice, LDevice, *duckFloat, true);
	sdf = new SDF();
	sdf->PreCreateResources(PDevice, LDevice, CommandPool, GraphicNComputeQueue, DescriptorPool, *duckFloat, renderingobj);
	VkCommandBuffer* sdfCb = sdf->GetSimCommandBuffer();
	sdfCommandBuffers.push_back(sdfCb);
}

void Renderer::CreateTextureResources(const char* image_path, VkImage& textureImage, VkImageView& textureImageView, VkDeviceMemory& textureImageMemory, VkSampler& textureImageSampler)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(image_path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	uint32_t size = texWidth * texHeight * 4;
	VkBuffer stagingbuffer;
	VkDeviceMemory stagingbuffermemory;
	Buffer::CreateBuffer(PDevice, LDevice, stagingbuffer, stagingbuffermemory, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	void* data;
	vkMapMemory(LDevice, stagingbuffermemory, 0, size, 0, &data);
	memcpy(data, pixels, size);
	STBI_FREE(pixels);

	VkExtent3D extent = { static_cast<uint32_t>(texWidth),static_cast<uint32_t>(texHeight),1 };
	Image::CreateImage(PDevice, LDevice, textureImage, textureImageMemory, extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT);

	auto cb = Util::CreateCommandBuffer(LDevice, CommandPool);
	VkImageMemoryBarrier imagebarrier{};
	imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imagebarrier.subresourceRange.baseArrayLayer = 0;
	imagebarrier.subresourceRange.baseMipLevel = 0;
	imagebarrier.subresourceRange.layerCount = 1;
	imagebarrier.subresourceRange.levelCount = 1;
	imagebarrier.image = textureImage;

	imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imagebarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imagebarrier.srcAccessMask = 0;
	imagebarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageExtent = extent;
	region.imageOffset = { 0,0,0 };
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.mipLevel = 0;
	vkCmdCopyBufferToImage(cb, stagingbuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	imagebarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imagebarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

	VkSubmitInfo submitinfo{};
	Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

	vkQueueWaitIdle(GraphicNComputeQueue);

	CleanupBuffer(stagingbuffer, stagingbuffermemory, true);

	textureImageView = CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	VkSamplerCreateInfo samplerinfo{};
	samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerinfo.anisotropyEnable = VK_FALSE;

	samplerinfo.magFilter = VK_FILTER_LINEAR;
	samplerinfo.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &textureImageSampler);
}

void Renderer::CreateCubemapTexture(const std::array<std::string, 6>& faces)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixel0 = stbi_load(faces[0].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkExtent3D extent = { static_cast<uint32_t>(texWidth),static_cast<uint32_t>(texHeight),1 };
	Image::CreateSpecialImage(
		PDevice, LDevice,
		skybox_resource.skyboxImage, skybox_resource.skyboxImageMemory, extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT);

	for (uint32_t i = 0; i < 6; ++i) {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(faces[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pixels) {
			throw std::runtime_error("failed to load skybox texture image: " + faces[i]);
		}
		VkDeviceSize imageSize = texWidth * texHeight * 4;
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		Buffer::CreateBuffer(PDevice, LDevice, stagingBuffer, stagingBufferMemory, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* data;
		vkMapMemory(LDevice, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(LDevice, stagingBufferMemory);
		stbi_image_free(pixels);

		VkCommandBuffer cb = Util::CreateCommandBuffer(LDevice, CommandPool);
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = skybox_resource.skyboxImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = i;           // 
		barrier.subresourceRange.layerCount = 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = i;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = extent;
		vkCmdCopyBufferToImage(cb, stagingBuffer, skybox_resource.skyboxImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkSubmitInfo submitinfo{};
		Util::SubmitCommandBuffer(LDevice, CommandPool, cb, submitinfo, VK_NULL_HANDLE, GraphicNComputeQueue);

		vkQueueWaitIdle(GraphicNComputeQueue);

		CleanupBuffer(stagingBuffer, stagingBufferMemory, false);
	}
	/*skybox_resource.skyboxImageView = CreateImageView(skybox_resource.skyboxImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);*/
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = skybox_resource.skyboxImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE; 
		viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 6; 

		if (vkCreateImageView(LDevice, &viewInfo, nullptr, &skybox_resource.skyboxImageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create cubemap image view!");
		}
	}
	VkSamplerCreateInfo samplerinfo = {};
	samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo.magFilter = VK_FILTER_LINEAR;
	samplerinfo.minFilter = VK_FILTER_LINEAR;
	samplerinfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerinfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerinfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerinfo.anisotropyEnable = VK_FALSE;
	samplerinfo.maxAnisotropy = 1.0f;
	samplerinfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerinfo.unnormalizedCoordinates = VK_FALSE;
	samplerinfo.compareEnable = VK_FALSE;
	samplerinfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerinfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerinfo.mipLodBias = 0.0f;
	samplerinfo.minLod = 0.0f;
	samplerinfo.maxLod = 0.0f;

	if (vkCreateSampler(LDevice, &samplerinfo, nullptr, &skybox_resource.skyboxSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create skybox sampler!");
	}
}

void Renderer::CreateDefaultTextureResources()
{
	{
		CreateTextureResources(
			"resources/textures/pexels-pixabay-220152.jpg",
			DefaultTextureImage,
			DefaultTextureImageView,
			DefaultTextureImageMemory,
			DefaultTextureImageSampler
		);
	}
	{
		CreateTextureResources(
			"resources/textures/pexels-pixabay-220152.jpg",
			ObjTextureImage,
			ObjTextureImageView,
			ObjTextureImageMemory,
			ObjTextureImageSampler
		);
	}
	{
		// 天空盒的纹理导入 - 调整后的顺序
		const std::array<std::string, 6> skyboxFaces = {
			"resources/textures/skybox3/left.jpg",    // +X
			"resources/textures/skybox3/right.jpg",   // -X
			"resources/textures/skybox3/top.jpg",     // +Y
			"resources/textures/skybox3/bottom.jpg",  // -Y
			"resources/textures/skybox3/front.jpg",   // +Z
			"resources/textures/skybox3/back.jpg"     // -Z
		};
		CreateCubemapTexture(skyboxFaces);
	}
}

void Renderer::CreateBackgroundResources()
{
	VkExtent3D extent = { SwapChainImageExtent.width,SwapChainImageExtent.height,1 };
	Image::CreateImage(PDevice, LDevice, BackgroundImage, BackgroundImageMemory, extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
	BackgroundImageView = CreateImageView(BackgroundImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	Image::ImageLayoutTransition(PDevice, LDevice, CommandPool, GraphicNComputeQueue, BackgroundImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerinfo{};
	samplerinfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerinfo.anisotropyEnable = VK_FALSE;
	samplerinfo.minFilter = VK_FILTER_LINEAR;
	samplerinfo.magFilter = VK_FILTER_LINEAR;
	vkCreateSampler(LDevice, &samplerinfo, Allocator, &BackgroundImageSampler);
}

void Renderer::CreateCubeResources()
{
}

//************************************************** above about image resources **********************************************************

void Renderer::CreateSwapChain()
{
	Base::CreateSwapChain(PDevice, Surface, LDevice, GraphicNComputeQueue, PresentQueue, SwapChain, SwapChainImageFormat, SwapChainImageExtent,
		SwapChainImages, SwapChainImageViews);
}
void Renderer::CleanupSwapChain()
{
	for (auto& imageview : SwapChainImageViews) {
		vkDestroyImageView(LDevice, imageview, Allocator);
	}
	vkDestroySwapchainKHR(LDevice, SwapChain, Allocator);
}
void Renderer::RecreateSwapChain()
{
	vkDeviceWaitIdle(LDevice);
	bFramebufferResized = false;
	vkDestroyFramebuffer(LDevice, FluidsFramebuffer, Allocator);
	vkDestroyFramebuffer(LDevice, BoxFramebuffer, Allocator);
	vkDestroyFramebuffer(LDevice, FluidsFramebuffer2, Allocator);

	vkDestroyImageView(LDevice, DepthImageView, Allocator);
	vkDestroyImage(LDevice, DepthImage, Allocator);
	vkFreeMemory(LDevice, DepthImageMemory, Allocator);

	vkDestroySampler(LDevice, ThickImageSampler, Allocator);
	vkDestroyImageView(LDevice, ThickImageView, Allocator);
	vkDestroyImage(LDevice, ThickImage, Allocator);
	vkFreeMemory(LDevice, ThickImageMemory, Allocator);

	vkDestroySampler(LDevice, CustomDepthImageSampler, Allocator);
	vkDestroyImageView(LDevice, CustomDepthImageView, Allocator);
	vkDestroyImage(LDevice, CustomDepthImage, Allocator);
	vkFreeMemory(LDevice, CustomDepthImageMemory, Allocator);

	vkDestroySampler(LDevice, FilteredDepthImageSampler, Allocator);
	vkDestroyImageView(LDevice, FilteredDepthImageView, Allocator);
	vkDestroyImage(LDevice, FilteredDepthImage, Allocator);
	vkFreeMemory(LDevice, FilteredDepthImageMemory, Allocator);

	vkDestroySampler(LDevice, BackgroundImageSampler, Allocator);
	vkDestroyImageView(LDevice, BackgroundImageView, Allocator);
	vkDestroyImage(LDevice, BackgroundImage, Allocator);
	vkFreeMemory(LDevice, BackgroundImageMemory, Allocator);

	vkDestroySampler(LDevice, CausticsImageSampler, Allocator);
	vkDestroyImageView(LDevice, CausticsImageView, Allocator);
	vkDestroyImage(LDevice, CausticsImage, Allocator);
	vkFreeMemory(LDevice, CausticsImageMemory, Allocator);

	vkDestroySampler(LDevice, CausticsImageSampler2, Allocator);
	vkDestroyImageView(LDevice, CausticsImageView2, Allocator);
	vkDestroyImage(LDevice, CausticsImage2, Allocator);
	vkFreeMemory(LDevice, CausticsImageMemory2, Allocator);

	CleanupSwapChain();
	CreateSwapChain();
	CreateDepthResources();
	CreateThickResources();
	CreateCausticsResources();
	CreateBackgroundResources();
	CreateFramebuffers();

	UpdateDescriptorSet();
}
void Renderer::CreateDescriptorSetLayout()
{
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT }
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, FluidGraphicDescriptorSetLayout);
	}
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
			{ 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, BoxGraphicDescriptorSetLayout);
	}
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, skybox_resource.descriptorSetLayout);
	}
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, sphere_resource.descriptorSetLayout);
	}
	// obj resources
	{
		// cup(废弃) | dragon | cylinder | table | sand | ground | mountain ...
		for (int i = 0; i < objResManagements.size(); i++) {
			ObjResource* objR = objResManagements[i].obj_res;
			if (objR == nullptr) {
				continue;
			}
			std::vector<BindingDesc> descs =
			{ { 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT } };
			int bindId = 1;
			if (objResManagements[i].isVox) {
				BindingDesc desc1 = { bindId++, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
				BindingDesc desc2 = { bindId++, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
				descs.push_back(desc1);
				descs.push_back(desc2);
			}
			if (objResManagements[i].isPBR) {
				uint32_t pbrImageCount = objR->textureImagePBR.size();
				for (uint32_t k = 0; k < pbrImageCount; k++) {
					BindingDesc desc = { bindId++, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
					descs.push_back(desc);
				}
			}
			if (objResManagements[i].isTex) {
				BindingDesc desc1 = { bindId++, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
				descs.push_back(desc1);
				if (objResManagements[i].is2Tex) {
					BindingDesc desc2 = { bindId++, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
					descs.push_back(desc2);
				}
			}
			if (objResManagements[i].isTransform) {
				BindingDesc desc1 = { bindId++, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
				descs.push_back(desc1);
			}
			if (objResManagements[i].isCustomEllipsode) {
				BindingDesc desc1 = { bindId++, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT };
				descs.push_back(desc1);
			}
			Base::descriptorSetLayoutBinding(LDevice, descs, objR->descriptorSetLayout);
		}
	}
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 3, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 5, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 6, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 7, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 8, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 9, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 10, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 11, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 12, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 13, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 14, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},    // sdf
			{ 15, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},    // dynamic sdf
			{ 16, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},    // wcsph pressure
			{ 18, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},    // wcsph external force
			{ 22, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},    // wcsph params
			{ 23, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},    // ellipsoid info
		};
		if (startAnisotrophic) {
			BindingDesc aniDesc = { 17, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT };
			descs.push_back(aniDesc);
		}
		if (startDiffuseParticles) {
			BindingDesc diffuseDesc1 = { 19, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT };
			BindingDesc diffuseDesc2 = { 20, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT };
			BindingDesc diffuseDesc3 = { 21, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT };
			descs.push_back(diffuseDesc1);
			descs.push_back(diffuseDesc2);
			descs.push_back(diffuseDesc3);
		}
		Base::descriptorSetLayoutBinding(LDevice, descs, SimulateDescriptorSetLayout);
	}
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 4, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 5, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 6, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 7, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 8, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},  // Cubemap for reflection
			{ 9, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, PostprocessDescriptorSetLayout);
	}
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 4, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 5, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, CausticsDescriptorSetLayout);
	}
	{
		std::vector<BindingDesc> descs = {
			{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
			{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT}
		};
		Base::descriptorSetLayoutBinding(LDevice, descs, FilterDecsriptorSetLayout);
	}
}

void Renderer::CreateDescriptorSet()
{
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, FluidGraphicDescriptorSetLayout, FluidGraphicDescriptorSet);

		VkDescriptorBufferInfo renderingbufferinfo{ UniformRenderingBuffer, 0, sizeof(UniformRenderingObject) };
		VkDescriptorBufferInfo simulatingbufferinfo{ UniformSimulatingBuffer, 0, sizeof(UniformSimulatingObject) };
		
		std::vector<VkWriteDescriptorSet> writes(2);
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{simulatingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, FluidGraphicDescriptorSet);
	}
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, BoxGraphicDescriptorSetLayout, BoxGraphicDescriptorSet);

		std::vector<VkWriteDescriptorSet> writes(3);
		VkDescriptorBufferInfo renderingbufferinfo     { UniformRenderingBuffer, 0, sizeof(UniformRenderingObject) };
		VkDescriptorBufferInfo boxinfobufferinfo       { UniformBoxInfoBuffer  , 0, sizeof(UniformBoxInfoObject)   };
		VkDescriptorImageInfo  defaultTextureimageinfo { DefaultTextureImageSampler, DefaultTextureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{boxinfobufferinfo,   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{defaultTextureimageinfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, BoxGraphicDescriptorSet);
	}
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, skybox_resource.descriptorSetLayout, skybox_resource.descriptorSet);

		std::vector<VkWriteDescriptorSet> writes(2);
		VkDescriptorBufferInfo renderingbufferinfo { UniformRenderingBuffer, 0, sizeof(UniformRenderingObject) };
		VkDescriptorImageInfo  textureimageinfo    { skybox_resource.skyboxSampler, skybox_resource.skyboxImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{textureimageinfo,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, skybox_resource.descriptorSet);
	}
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, sphere_resource.descriptorSetLayout, sphere_resource.descriptorSet);

		std::vector<VkWriteDescriptorSet> writes(2);
		VkDescriptorBufferInfo renderingbufferinfo { UniformRenderingBuffer, 0, sizeof(UniformRenderingObject) };
		VkDescriptorImageInfo textureimageinfo     { skybox_resource.skyboxSampler, skybox_resource.skyboxImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		std::vector<WriteDesc> writeInfo = {
			{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
			{textureimageinfo,    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}
		};
		Base::DescriptorWrite(LDevice, writeInfo, writes, sphere_resource.descriptorSet);
	}
	// Obj DescriptorSets
	{
		for (int i = 0; i < objResManagements.size(); i++) {
			ObjResource* objR = objResManagements[i].obj_res;
			if (objR == nullptr || objR->descriptorSetLayout == VK_NULL_HANDLE) {
				continue;
			}
			if (objResManagements[i].isVox && (objR->UniformBuffer == VK_NULL_HANDLE || objR->voxelBuffer == VK_NULL_HANDLE)) {
				objR->descriptorSet = VK_NULL_HANDLE;
				continue;
			}
			if (objResManagements[i].isTransform && objR->UniformBuffer_ == VK_NULL_HANDLE) {
				objR->descriptorSet = VK_NULL_HANDLE;
				continue;
			}
			Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, objR->descriptorSetLayout, objR->descriptorSet);
			VkDescriptorBufferInfo renderingbufferinfo{};
			renderingbufferinfo.buffer = UniformRenderingBuffer;
			renderingbufferinfo.offset = 0;
			renderingbufferinfo.range = sizeof(UniformRenderingObject);
			VkDescriptorBufferInfo voxelbufferinfo1{};
			VkDescriptorBufferInfo voxelbufferinfo2{};
			VkDescriptorBufferInfo objTransformInfo{};
			VkDescriptorImageInfo  textureImageInfo{};
			VkDescriptorImageInfo  textureImageInfo2{};

			VkDescriptorBufferInfo ellipsodebufferinfo{};
			std::vector<WriteDesc> writeInfo = {
				{renderingbufferinfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER}
			};
			std::vector<VkWriteDescriptorSet> writes(1);
			if (objResManagements[i].isVox) {
				voxelbufferinfo1.buffer = objR->UniformBuffer;
				voxelbufferinfo1.offset = 0;
				voxelbufferinfo1.range  = sizeof(UniformVoxelObject);
				voxelbufferinfo2.buffer = objR->voxelBuffer;
				voxelbufferinfo2.offset = 0;
				voxelbufferinfo2.range  = sizeof(uint32_t) * VOXEL_RES * VOXEL_RES * VOXEL_RES;
				writeInfo.push_back({ voxelbufferinfo1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER });
				writeInfo.push_back({ voxelbufferinfo2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
			}
			if (objResManagements[i].isPBR) {
				// isPBR = true => isTex = false
				int image_nums = objR->textureImagePBR.size();
				for (int k = 0; k < image_nums; k++) {
					VkDescriptorImageInfo textureImagePBRInfo{};
					textureImagePBRInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					textureImagePBRInfo.imageView = objR->textureImageViewPBR[k] != VK_NULL_HANDLE ? objR->textureImageViewPBR[k] : DefaultTextureImageView;
					textureImagePBRInfo.sampler   = objR->textureSamplerPBR[k] != VK_NULL_HANDLE ? objR->textureSamplerPBR[k] : DefaultTextureImageSampler;
					writeInfo.push_back({ textureImagePBRInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER });
				}
			}
			if (objResManagements[i].isTex) {
				textureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				textureImageInfo.imageView   = objR->textureImageView != VK_NULL_HANDLE ? objR->textureImageView : DefaultTextureImageView;
				textureImageInfo.sampler     = objR->textureSampler != VK_NULL_HANDLE ? objR->textureSampler : DefaultTextureImageSampler;
				writeInfo.push_back({ textureImageInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER });
				if (objResManagements[i].is2Tex) {
					textureImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					textureImageInfo2.imageView   = objR->textureImageView2 != VK_NULL_HANDLE ? objR->textureImageView2 : DefaultTextureImageView;
					textureImageInfo2.sampler     = objR->textureSampler2 != VK_NULL_HANDLE ? objR->textureSampler2 : DefaultTextureImageSampler;
					writeInfo.push_back({ textureImageInfo2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER });
				}
			}
			if (objResManagements[i].isTransform) {
				objTransformInfo.buffer = objR->UniformBuffer_;
				objTransformInfo.offset = 0;
				objTransformInfo.range = sizeof(ObjTransform);
				writeInfo.push_back({ objTransformInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER });
			}
			if (objResManagements[i].isCustomEllipsode) {
				ellipsodebufferinfo.buffer = ellipsoidBuffers[0];
				ellipsodebufferinfo.offset = 0;
				ellipsodebufferinfo.range = sizeof(EllipsoidInfo);
				writeInfo.push_back({ ellipsodebufferinfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });
			}
			int write_size = writeInfo.size();
			writes.resize(write_size);
			Base::DescriptorWrite(LDevice, writeInfo, writes, objR->descriptorSet);
		}
	}
	// SimulateDescriptorSet
	{
		SimulateDescriptorSet.resize(MAXInFlightRendering);
		for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
			Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, SimulateDescriptorSetLayout, SimulateDescriptorSet[i]);
		}
		std::vector<VkWriteDescriptorSet> writes(20);
		if (startAnisotrophic) {
			uint32_t currentSize = writes.size();
			writes.resize(currentSize + 1);
		}
		if (startDiffuseParticles) {
			uint32_t currentSize = writes.size();
			currentSize += 3;
			//if (!startAnisotrophic) currentSize += 1;
			writes.resize(currentSize);
		}
		uint32_t allParticleRange = sizeof(uint32_t) * (simulatingobj.maxParticles + simulatingobj.maxCubeParticles + rigidParticles.size());
		for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
			const VkDeviceSize cubeParticleRange = sizeof(CubeParticle) * (cubeParticles.empty() ? 1ull : cubeParticles.size());
			const VkDeviceSize rigidParticleRange = sizeof(ExtraParticle) * (rigidParticles.empty() ? 1ull : rigidParticles.size());
			const VkDeviceSize originRigidParticleRange = sizeof(ExtraParticle) * (originRigidParticles.empty() ? 1ull : originRigidParticles.size());
			VkDescriptorBufferInfo simulatingbufferinfo          { UniformSimulatingBuffer,                         0, sizeof(UniformSimulatingObject)                     };
			VkDescriptorBufferInfo boxbufferinfo                 { UniformBoxInfoBuffer,                            0, sizeof(UniformBoxInfoObject)                        };
			// 6 bufferinfos for merge sort
			VkDescriptorBufferInfo cellbufferinfo                { CellInfoBuffer,                                  0, allParticleRange                                    };
			VkDescriptorBufferInfo ngbrcntbufferinfo             { NeighborCountBuffer,                             0, sizeof(uint32_t) * cellCount                        };
			VkDescriptorBufferInfo ngbrcntcpybufferinfo          { NeighborCpyCountBuffer,                          0, sizeof(uint32_t) * cellCount                        };
			VkDescriptorBufferInfo particleincellbufferinfo      { ParticleInCellBuffer,                            0, allParticleRange                                    };
			VkDescriptorBufferInfo celloffsetbufferinfo          { CellOffsetBuffer,                                0, sizeof(uint32_t) * cellCount                        };
			VkDescriptorBufferInfo testpbfbufferinfo             { TestPBFBuffer,                                   0, allParticleRange                                    };
			// others
			VkDescriptorBufferInfo particlebufferinfo_thisframe  { ParticleBuffers[i],                              0, sizeof(Particle) * simulatingobj.maxParticles       };
			VkDescriptorBufferInfo particlebufferinfo_lastframe  { ParticleBuffers[(i - 1) % MAXInFlightRendering], 0, sizeof(Particle) * simulatingobj.maxParticles       };
			VkDescriptorBufferInfo cubeparticlebufferinfo        { CubeParticleBuffers[i],                          0, cubeParticleRange                                    };
			VkDescriptorBufferInfo rigidparticlebufferinfo       { RigidParticleBuffers[i],                         0, rigidParticleRange                                   };
			VkDescriptorBufferInfo originrigidparticlebufferinfo { OriginRigidParticleBuffers[i],                   0, originRigidParticleRange                             };
			VkDescriptorBufferInfo shapematchingbufferinfo       { ShapeMatchingBuffers[i],                         0, sizeof(ShapeMatchingInfo)                           };
			VkDescriptorBufferInfo sdfbufferinfo   { SDFBuffer,   0, sizeof(float) * VOXEL_RES * VOXEL_RES * VOXEL_RES };
			VkDescriptorBufferInfo sdfdybufferinfo { SDFDyBuffer, 0, sizeof(float) * VOXEL_RES * VOXEL_RES * VOXEL_RES };
			VkDescriptorBufferInfo wcsphpressurebufferinfo      { WCSPHPressureBuffer,       0, sizeof(float) * simulatingobj.maxParticles     };
			VkDescriptorBufferInfo wcsphforcebufferinfo         { WCSPHExternalForceBuffer,  0, sizeof(glm::vec4) * simulatingobj.maxParticles };
			VkDescriptorBufferInfo wcsphparamsbufferinfo        { WCSPHParamsBuffer,         0, sizeof(glm::vec4)                             };
			VkDescriptorBufferInfo anisotrophicinfo;
			VkDescriptorBufferInfo diffuseparticleinfo;
			VkDescriptorBufferInfo diffusepaticlecountinfo;
			VkDescriptorBufferInfo surfaceparticlebufferinfo;

			// ellipsoid 
			VkDescriptorBufferInfo ellipsoidbufferinfo{ ellipsoidBuffers[0], 0, sizeof(EllipsoidInfo) };

			std::vector<WriteDesc> writeInfo = {
				{simulatingbufferinfo,            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0},
				{particlebufferinfo_lastframe,    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
				{particlebufferinfo_thisframe,    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
				{boxbufferinfo,					  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
				{cellbufferinfo,				  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
				{ngbrcntbufferinfo,				  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5},
				{ngbrcntcpybufferinfo,			  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6},
				{particleincellbufferinfo,		  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7},
				{celloffsetbufferinfo,			  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8},
				{testpbfbufferinfo,				  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9},
				{cubeparticlebufferinfo,		  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
				{rigidparticlebufferinfo,		  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 11},
				{shapematchingbufferinfo,		  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 12},
				{originrigidparticlebufferinfo,	  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 13},
				{sdfbufferinfo,                   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 14},
				{sdfdybufferinfo,                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 15},
				{wcsphpressurebufferinfo,	        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
				{wcsphforcebufferinfo,	            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 18},
				{wcsphparamsbufferinfo,            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 22},
				{ellipsoidbufferinfo,	            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 23}
			};
			if (startAnisotrophic) {
				anisotrophicinfo = { AnisotrophicBuffers[0], 0, sizeof(AnisotrophicMatrix) * simulatingobj.maxParticles };
				WriteDesc writeAniDesc = { anisotrophicinfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 17 };
				writeInfo.push_back(writeAniDesc);
			}
			if (startDiffuseParticles) {
				diffuseparticleinfo           = { DiffuseParticleBuffers[i], 0, sizeof(DiffuseParticle)* diffuseParticles.size() };
				diffusepaticlecountinfo       = { DiffuseParticleCountBuffers[0], 0, sizeof(DiffuseParticleCount) };
				surfaceparticlebufferinfo     = { SurfaceParticleBuffer, 0, sizeof(uint32_t) * simulatingobj.maxParticles };
				WriteDesc writeDesc1 = { diffuseparticleinfo,           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 19 };
				WriteDesc writeDesc2 = { diffusepaticlecountinfo,       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20 };
				WriteDesc writeDesc3 = { surfaceparticlebufferinfo,     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 21 };
				writeInfo.push_back(writeDesc1);
				writeInfo.push_back(writeDesc2);
				writeInfo.push_back(writeDesc3);
			}
			Base::DescriptorWrite(LDevice, writeInfo, writes, SimulateDescriptorSet[i]);
		}
	}
	{
		PostprocessDescriptorSets.resize(SwapChainImages.size());
		for (uint32_t i = 0; i < SwapChainImages.size(); ++i) {
			Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, PostprocessDescriptorSetLayout, PostprocessDescriptorSets[i]);
		}
	}
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, CausticsDescriptorSetLayout, CausticsDescriptorSet);
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, CausticsDescriptorSetLayout, CausticsBlurDescriptorSet);
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, CausticsDescriptorSetLayout, CausticsBlurDescriptorSet2);
	}
	{
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, FilterDecsriptorSetLayout, FilterDescriptorSet);
		Base::CreateGraphicDescriptorSet(LDevice, DescriptorPool, FilterDecsriptorSetLayout, FilterDescriptorSet2);
	}
	// postprocessdescriptorset and filterdescriptorset write update
	UpdateDescriptorSet();
}

void Renderer::UpdateSimulateDescriptorSetForCubeParticles()
{
	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		VkDescriptorBufferInfo cubeparticlebufferinfo{};
		cubeparticlebufferinfo.buffer = CubeParticleBuffers[i];
		cubeparticlebufferinfo.offset = 0;
		cubeparticlebufferinfo.range = sizeof(CubeParticle) * (cubeParticles.empty() ? 1ull : cubeParticles.size());

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = SimulateDescriptorSet[i];
		write.dstBinding = 10;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &cubeparticlebufferinfo;

		vkUpdateDescriptorSets(LDevice, 1, &write, 0, nullptr);
	}
}

void Renderer::CreateRenderPass()
{
	VkAttachmentDescription boxcolorattachement{};
	boxcolorattachement.format = VK_FORMAT_R8G8B8A8_SRGB;
	boxcolorattachement.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	boxcolorattachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	boxcolorattachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	boxcolorattachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	boxcolorattachement.samples = VK_SAMPLE_COUNT_1_BIT;
	boxcolorattachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	boxcolorattachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentDescription depthattachement{};
	depthattachement.format = VK_FORMAT_D32_SFLOAT;
	depthattachement.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthattachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthattachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthattachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthattachement.samples = VK_SAMPLE_COUNT_1_BIT;
	depthattachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthattachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentDescription thickattachment{};
	thickattachment.format = VK_FORMAT_R32_SFLOAT;
	thickattachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	thickattachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	thickattachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	thickattachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	thickattachment.samples = VK_SAMPLE_COUNT_1_BIT;
	thickattachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	thickattachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentDescription customdepthattachement{};
	customdepthattachement.format = VK_FORMAT_R32_SFLOAT;
	customdepthattachement.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	customdepthattachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	customdepthattachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	customdepthattachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	customdepthattachement.samples = VK_SAMPLE_COUNT_1_BIT;
	customdepthattachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	customdepthattachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	{
		std::array<VkAttachmentDescription, 2> attachments = { thickattachment,customdepthattachement };
		VkAttachmentReference thickattachment_ref{};
		thickattachment_ref.attachment = 0;
		thickattachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference customdepthattachment_ref{};
		customdepthattachment_ref.attachment = 1;
		customdepthattachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		std::array<VkAttachmentReference, 2> colorattachment_ref = { customdepthattachment_ref,thickattachment_ref };
		std::array<VkSubpassDescription, 1> subpasses{};
		subpasses[0].colorAttachmentCount = static_cast<uint32_t>(colorattachment_ref.size());
		subpasses[0].pColorAttachments = colorattachment_ref.data();
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		VkRenderPassCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createinfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createinfo.pAttachments = attachments.data();
		createinfo.subpassCount = static_cast<uint32_t>(subpasses.size());
		createinfo.pSubpasses = subpasses.data();

		if (vkCreateRenderPass(LDevice, &createinfo, Allocator, &FluidGraphicRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create fluid graphic renderpass!");
		}

		if (vkCreateRenderPass(LDevice, &createinfo, Allocator, &FluidGraphicRenderPass2) != VK_SUCCESS) {
			throw std::runtime_error("failed to create fluid graphic renderpass2!");
		}
	}
	{
		std::array<VkAttachmentDescription, 2> attachments = { boxcolorattachement,depthattachement };
		VkAttachmentReference depthattachement_ref{};
		depthattachement_ref.attachment = 1;
		depthattachement_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkAttachmentReference boxcolorattachement_ref{};
		boxcolorattachement_ref.attachment = 0;
		boxcolorattachement_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		std::array<VkSubpassDescription, 1> subpasses{};
		subpasses[0].colorAttachmentCount = 1;
		subpasses[0].pColorAttachments = &boxcolorattachement_ref;
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].pDepthStencilAttachment = &depthattachement_ref;

		VkRenderPassCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createinfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createinfo.pAttachments = attachments.data();
		createinfo.subpassCount = static_cast<uint32_t>(subpasses.size());
		createinfo.pSubpasses = subpasses.data();

		if (vkCreateRenderPass(LDevice, &createinfo, Allocator, &BoxGraphicRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create box graphic renderpass!");
		}
	}
}
void Renderer::CreateGraphicPipelineLayout()
{
	Base::CreateGraphicPipelineLayout(LDevice, FluidGraphicDescriptorSetLayout, FluidGraphicPipelineLayout);
	Base::CreateGraphicPipelineLayout(LDevice, BoxGraphicDescriptorSetLayout, BoxGraphicPipelineLayout);
	Base::CreateGraphicPipelineLayout(LDevice, skybox_resource.descriptorSetLayout, skybox_resource.pipelineLayout);
	Base::CreateGraphicPipelineLayout(LDevice, sphere_resource.descriptorSetLayout, sphere_resource.pipelineLayout);

	// obj's graphic pipelines creating
	for (size_t obj_i = 0; obj_i < objResManagements.size(); obj_i++) {
		ObjResource* objR = objResManagements[obj_i].obj_res;
		if (objR == nullptr || objR->descriptorSetLayout == VK_NULL_HANDLE) {
			continue;
		}
		Base::CreateGraphicPipelineLayout(LDevice, objR->descriptorSetLayout, objR->pipelineLayout);
	}
}

void Renderer::CreateGraphicPipeline()
{
	VkPipelineColorBlendAttachmentState depthblendattachment{};
	depthblendattachment.blendEnable = VK_TRUE;
	depthblendattachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	depthblendattachment.colorBlendOp = VK_BLEND_OP_MIN;
	depthblendattachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	depthblendattachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

	VkPipelineColorBlendAttachmentState thickblendattachment{};
	thickblendattachment.blendEnable = VK_TRUE;
	thickblendattachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	thickblendattachment.colorBlendOp = VK_BLEND_OP_ADD;
	thickblendattachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	thickblendattachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	thickblendattachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	thickblendattachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

	std::vector<VkPipelineColorBlendAttachmentState> fluidcolorblendattachments = { depthblendattachment, thickblendattachment };

	VkPipelineColorBlendAttachmentState boxcolorblendattachment{};
	boxcolorblendattachment.blendEnable = VK_FALSE;
	boxcolorblendattachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

	std::vector<VkPipelineColorBlendAttachmentState> boxcolorblendattachments = { boxcolorblendattachment };

	VkPipelineColorBlendAttachmentState cubecolorblendattachment{};
	cubecolorblendattachment.blendEnable = VK_FALSE;
	cubecolorblendattachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	
	std::vector<VkPipelineColorBlendAttachmentState> cubecolorblendattachments = { cubecolorblendattachment };

	// anisotrophic fluid particles - Billboard rendering
	if (startAnisotrophic) {
		Base::CreateGraphicPipeline(
			LDevice, FluidGraphicPipelineLayout, FluidGraphicRenderPass, &FluidGraphicPipeline,
			"resources/shaders/spv/fluidshader_ani.vert.spv", "resources/shaders/spv/fluidshader_ani.frag.spv",
			true, 7,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,  // Billboard quad渲染
			VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
			VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, fluidcolorblendattachments
		);
	}
	else {
		Base::CreateGraphicPipeline(
			LDevice, FluidGraphicPipelineLayout, FluidGraphicRenderPass, &FluidGraphicPipeline,
			"resources/shaders/spv/fluidshader.vert.spv", "resources/shaders/spv/fluidshader.frag.spv",
			true, 0,
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
			VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, fluidcolorblendattachments
		);
	}
	
	Base::CreateGraphicPipeline(
		LDevice, FluidGraphicPipelineLayout, FluidGraphicRenderPass2, &FluidGraphicPipeline2,
		"resources/shaders/spv/rigidshader.vert.spv", "resources/shaders/spv/rigidshader.frag.spv",
		true, 1,
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
		VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, fluidcolorblendattachments
	);

	Base::CreateGraphicPipeline(
		LDevice, FluidGraphicPipelineLayout, FluidGraphicRenderPass, &FluidGraphicPipeline3,
		"resources/shaders/spv/cubeshader.vert.spv", "resources/shaders/spv/cubeshader.frag.spv",
		true, 3,
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
		VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
		VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, fluidcolorblendattachments
	);

	// DiffuseParticle
	if (startDiffuseParticles) {
		Base::CreateGraphicPipeline(
			LDevice, FluidGraphicPipelineLayout, FluidGraphicRenderPass, &DiffuseGraphicPipeline,
			"resources/shaders/spv/diffuseshader.vert.spv", "resources/shaders/spv/diffuseshader.frag.spv",
			true, 6,  
			VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
			VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, fluidcolorblendattachments
		);
	}
		
	Base::CreateGraphicPipeline(
		LDevice, BoxGraphicPipelineLayout, BoxGraphicRenderPass, &BoxGraphicPipeline,
		"resources/shaders/spv/boxshader.vert.spv", "resources/shaders/spv/boxshader.frag.spv",
		false, 0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
		VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, boxcolorblendattachments
	);
	
	Base::CreateGraphicPipeline(
		LDevice, obj_resource.pipelineLayout, BoxGraphicRenderPass, &obj_resource.graphicPipeline,
		"resources/shaders/spv/objshader.vert.spv", "resources/shaders/spv/objshader.frag.spv",
		true, 2,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
		VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, cubecolorblendattachments
	);	
}
void Renderer::CreateSkyboxGraphicPipeline()
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = { colorBlendAttachment };

	Base::CreateGraphicPipeline(
		LDevice, skybox_resource.pipelineLayout, BoxGraphicRenderPass, &skybox_resource.graphicPipeline,
		"resources/shaders/spv/skybox.vert.spv", "resources/shaders/spv/skybox.frag.spv",
		true, 4,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
		VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, colorBlendAttachments
	);
}

void Renderer::CreateSphereGraphicPipeline()
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = { colorBlendAttachment };

	auto objResPaths = scene_set->GetObjResPathMap();

	Base::CreateGraphicPipeline(
		LDevice, sphere_resource.pipelineLayout, BoxGraphicRenderPass, &sphere_resource.graphicPipeline,
		"resources/shaders/spv/sphereshader.vert.spv", "resources/shaders/spv/sphereshader.frag.spv",
		true, 5,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
		VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, colorBlendAttachments
	);

	for (int obj_idx = 2; obj_idx < objResManagements.size(); obj_idx++) {
		ObjResource* objR = objResManagements[obj_idx].obj_res;
		if (objR == nullptr || objR->pipelineLayout == VK_NULL_HANDLE) {
			continue;
		}
		auto it = objResPaths.find(objR);
		if (it == objResPaths.end()) {
			continue;
		}
		auto vert_path = it->second[0];
		auto frag_path = it->second[1];
		if (vert_path.empty() || frag_path.empty()) {
			continue;
		}

		Base::CreateGraphicPipeline(
			LDevice, objR->pipelineLayout, BoxGraphicRenderPass, &objR->graphicPipeline,
			vert_path.c_str(), frag_path.c_str(),
			true, 2,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
			VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, colorBlendAttachments
		);
	}
}

void Renderer::CreateDragonGraphicPipeline()
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	// colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.colorWriteMask = 0;      // stop the dragon obj presenting!
	colorBlendAttachment.blendEnable = VK_FALSE;
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = { colorBlendAttachment };

	Base::CreateGraphicPipeline(
		LDevice, obj_r2.pipelineLayout, BoxGraphicRenderPass, &obj_r2.graphicPipeline,
		"resources/shaders/spv/obj_dragon.vert.spv", "resources/shaders/spv/obj_dragon.frag.spv",
		true, 2,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL,
		VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, colorBlendAttachments
	);
}

void Renderer::CreateComputePipelineLayout()
{
	// push constant for layer_num
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PushConstantData);

	VkPipelineLayoutCreateInfo simulatecreateinfo{};
	simulatecreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	simulatecreateinfo.pSetLayouts = &SimulateDescriptorSetLayout;
	simulatecreateinfo.setLayoutCount = 1;
	simulatecreateinfo.pushConstantRangeCount = 1;
	simulatecreateinfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(LDevice, &simulatecreateinfo, Allocator, &SimulatePipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create simulate pipeline layout!");
	}
	VkPipelineLayoutCreateInfo postprocesscreateinfo{};
	postprocesscreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	postprocesscreateinfo.pSetLayouts = &PostprocessDescriptorSetLayout;
	postprocesscreateinfo.setLayoutCount = 1;
	if (vkCreatePipelineLayout(LDevice, &postprocesscreateinfo, Allocator, &PostprocessPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create postprocessing compute pipeline layout!");
	}
	VkPipelineLayoutCreateInfo filtercreateinfo{};
	filtercreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	filtercreateinfo.pSetLayouts = &FilterDecsriptorSetLayout;
	filtercreateinfo.setLayoutCount = 1;
	if (vkCreatePipelineLayout(LDevice, &filtercreateinfo, Allocator, &FilterPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create filtering compute pipeline layout!");
	}
	VkPipelineLayoutCreateInfo causticscreateinfo{};
	causticscreateinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	causticscreateinfo.pSetLayouts = &CausticsDescriptorSetLayout;
	causticscreateinfo.setLayoutCount = 1;
	if (vkCreatePipelineLayout(LDevice, &causticscreateinfo, Allocator, &CausticsPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create caustics compute pipeline layout!");
	}
}
void Renderer::CreateComputePipeline()
{
	{
		// MERGE SORT
		auto computershadermodule_resetgrid = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_resetgrid.comp.spv");
		auto computershadermodule_partition = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_partition.comp.spv");
		auto computershadermodule_mergesort1 = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_mergesort1.comp.spv");
		auto computershadermodule_mergesort2 = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_mergesort2.comp.spv");
		auto computershadermodule_mergesortall = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_mergesortall.comp.spv");
		auto computershadermodule_indexmapped = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_indexmapped.comp.spv");

		std::vector<VkShaderModule> shadermodules = { computershadermodule_resetgrid, computershadermodule_partition,
		computershadermodule_mergesort1, computershadermodule_mergesort2, computershadermodule_mergesortall,
		computershadermodule_indexmapped };

		std::vector<VkPipeline*> pcomputepipelines = { &SimulatePipeline_ResetGrid, &SimulatePipeline_Partition,
		&SimulatePipeline_MergeSort1,&SimulatePipeline_MergeSort2, &SimulatePipeline_MergeSortAll,
		&SimulatePipeline_IndexMapped };

		for (uint32_t i = 0; i < shadermodules.size(); i++)
		{
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

	if (wcsphAlgorithm) {
		// SIMULATING PIPELINES WCSPH
		auto computershadermodule_sph_euler = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_sph_euler.comp.spv");
		auto computershadermodule_sph_pressure = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_sph_pressure.comp.spv");
		auto computershadermodule_sph_pressureforce = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_sph_pressureforce.comp.spv");
		auto computershadermodule_sph_advect = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_sph_advect.comp.spv");

		std::vector<VkShaderModule> shadermodules = {
			computershadermodule_sph_euler,
			computershadermodule_sph_pressure,
			computershadermodule_sph_pressureforce,
			computershadermodule_sph_advect
		};
		std::vector<VkPipeline*> pcomputepipelines = {
			&SimulatePipeline_WCSPH_Euler,
			&SimulatePipeline_WCSPH_Pressure,
			&SimulatePipeline_WCSPH_PressureForce,
			&SimulatePipeline_WCSPH_Advect
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
				throw std::runtime_error("failed to create wcsph compute pipeline!");
			}
		}
		for (auto& computershadermodule : shadermodules) {
			vkDestroyShaderModule(LDevice, computershadermodule, Allocator);
		}
	}

	{
		auto computershadermodule_ellipsoidupd = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_ellipsoidupd.comp.spv");
		
		VkPipelineShaderStageCreateInfo stageinfo{};
		stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageinfo.pName = "main";
		stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stageinfo.module = computershadermodule_ellipsoidupd;
		
		VkComputePipelineCreateInfo createinfo{};
		createinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		createinfo.layout = SimulatePipelineLayout;
		createinfo.stage = stageinfo;
		
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &createinfo, Allocator, &SimulatePipeline_EllipsoidUpd) != VK_SUCCESS) {
			throw std::runtime_error("failed to create ellipsoid update compute pipeline!");
		}
		vkDestroyShaderModule(LDevice, computershadermodule_ellipsoidupd, Allocator);
	}

	{
		//SIMULATING PIPELINES PBF
		auto computershadermodule_euler = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_euler.comp.spv");
		auto computershadermodule_mass = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_masscompute.comp.spv");
		auto computershadermodule_lambda = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_lambda.comp.spv");
		auto computershadermodule_resetrigid = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_resetrigidparticle.comp.spv");
		auto computershadermodule_deltaposition = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_deltaposition.comp.spv");
		auto computershadermodule_positionupd = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_positionupd.comp.spv");
		auto computershadermodule_velocityupd = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_velocityupd.comp.spv");
		auto computershadermodule_velocitycache = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_velocitycache.comp.spv");
		auto computershadermodule_viscositycorr = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_viscositycorr.comp.spv");
		auto computershadermodule_vorticitycorr = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_vorticitycorr.comp.spv");
		auto computershadermodule_anisotrophic = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_anisotrophic.comp.spv");

		std::vector<VkShaderModule> shadermodules = { computershadermodule_euler,computershadermodule_mass,computershadermodule_lambda,computershadermodule_resetrigid,
		computershadermodule_deltaposition,
		computershadermodule_positionupd,computershadermodule_velocityupd,computershadermodule_velocitycache,
		computershadermodule_viscositycorr,computershadermodule_vorticitycorr };
		std::vector<VkPipeline*> pcomputepipelines = { &SimulatePipeline_Euler, &SimulatePipeline_Mass, &SimulatePipeline_Lambda,&SimulatePipeline_ResetRigid,&SimulatePipeline_DeltaPosition,
		&SimulatePipeline_PositionUpd,&SimulatePipeline_VelocityUpd,
		&SimulatePipeline_VelocityCache,&SimulatePipeline_ViscosityCorr, &SimulatePipeline_VorticityCorr };
		if (startAnisotrophic) {
			shadermodules.push_back(computershadermodule_anisotrophic);
			pcomputepipelines.push_back(&SimulatePipeline_Anisotrophic);
		}
		else {
			vkDestroyShaderModule(LDevice, computershadermodule_anisotrophic, Allocator);
		}

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
	if (startDiffuseParticles)
	{
		// FOAM PARTICLES
		auto computershadermodule_resetdiffuse = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_resetdiffuseparticle.comp.spv");
		auto computershadermodule_surface = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_surfacedetection.comp.spv");
		auto computershadermodule_foamgeneration = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_foamgeneration.comp.spv");
		auto computershadermodule_foamupd = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_foamupd.comp.spv");
		auto computershadermodule_compact = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_diffuseparticle_compact.comp.spv");

		std::vector<VkShaderModule> shadermodules = { computershadermodule_resetdiffuse, computershadermodule_surface,
														computershadermodule_foamgeneration, computershadermodule_foamupd, computershadermodule_compact };

		std::vector<VkPipeline*> pcomputepipelines = { &SimulatePipeline_ResetDiffuse, &SimulatePipeline_SurfaceDetection, 
														&SimulatePipeline_FoamGeneration, &SimulatePipeline_FoamUpd, &SimulatePipeline_Compact };

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
				throw std::runtime_error("failed to create rigid shape compute pipeline!");
			}
		}
		for (auto& computershadermodule : shadermodules) {
			vkDestroyShaderModule(LDevice, computershadermodule, Allocator);
		}
	}
	{
		// RIGID SHAPE MATCHING
		auto computershadermodule_cm = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_computecm.comp.spv");
		auto computershadermodule_cm2 = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_computecm2.comp.spv");
		auto computershadermodule_apq = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_computeapq.comp.spv");
		auto computershadermodule_rotate = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_computerotate.comp.spv");
		auto computershadermodule_rigiddeltaposition = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_rigiddeltaposition.comp.spv");
		auto computershadermodule_rigidpositionupd = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_rigidpositionupd.comp.spv");

		std::vector<VkShaderModule> shadermodules = { computershadermodule_cm, computershadermodule_cm2, computershadermodule_apq, computershadermodule_rotate,
		computershadermodule_rigiddeltaposition, computershadermodule_rigidpositionupd };

		std::vector<VkPipeline*> pcomputepipelines = { &SimulatePipeline_ComputeCM, &SimulatePipeline_ComputeCM2, &SimulatePipeline_ComputeApq,
		&SimulatePipeline_ComputeRotate, &SimulatePipeline_RigidDeltaPosition, &SimulatePipeline_RigidPositionUpd };

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
				throw std::runtime_error("failed to create rigid shape compute pipeline!");
			}
		}
		for (auto& computershadermodule : shadermodules) {
			vkDestroyShaderModule(LDevice, computershadermodule, Allocator);
		}
	}
	{
		//POSTPROCESSING PIPELINES
		auto computershadermodule_postprocessing = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_postprocessing.comp.spv");
		auto computershadermodule_filtering_x = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_filtering_x.comp.spv");
		auto computershadermodule_filtering_y = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_filtering_y.comp.spv");
		auto computershadermodule_caustics = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_caustics_generate.comp.spv");
		auto computershadermodule_caustics_blur_x = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_caustics_blur_x.comp.spv");
		auto computershadermodule_caustics_blur_y = Base::MakeShaderModule(LDevice, "resources/shaders/spv/compshader_caustics_blur_y.comp.spv");

		VkPipelineShaderStageCreateInfo postprecessing_stageinfo{};
		postprecessing_stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		postprecessing_stageinfo.pName = "main";
		postprecessing_stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		postprecessing_stageinfo.module = computershadermodule_postprocessing;
		VkComputePipelineCreateInfo rcreateinfo{};
		rcreateinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		rcreateinfo.layout = PostprocessPipelineLayout;
		rcreateinfo.stage = postprecessing_stageinfo;
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &rcreateinfo, Allocator, &PostprocessPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create postprocess pipeline!");
		}

		// X方向滤波 pipeline
		VkPipelineShaderStageCreateInfo filtering_x_stageinfo{};
		filtering_x_stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		filtering_x_stageinfo.pName = "main";
		filtering_x_stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		filtering_x_stageinfo.module = computershadermodule_filtering_x;
		VkComputePipelineCreateInfo fcreateinfo{};
		fcreateinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		fcreateinfo.layout = FilterPipelineLayout;
		fcreateinfo.stage = filtering_x_stageinfo;
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &fcreateinfo, Allocator, &FilterPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create filter X pipeline!");
		}

		// Y方向滤波 pipeline
		VkPipelineShaderStageCreateInfo filtering_y_stageinfo{};
		filtering_y_stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		filtering_y_stageinfo.pName = "main";
		filtering_y_stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		filtering_y_stageinfo.module = computershadermodule_filtering_y;

		fcreateinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		fcreateinfo.layout = FilterPipelineLayout;
		fcreateinfo.stage = filtering_y_stageinfo;
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &fcreateinfo, Allocator, &FilterPipeline2) != VK_SUCCESS) {
			throw std::runtime_error("failed to create filter Y pipeline!");
		}

		VkPipelineShaderStageCreateInfo caustics_stageinfo{};
		caustics_stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		caustics_stageinfo.pName = "main";
		caustics_stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		caustics_stageinfo.module = computershadermodule_caustics;
		VkComputePipelineCreateInfo ccreateinfo{};
		ccreateinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		ccreateinfo.layout = CausticsPipelineLayout;
		ccreateinfo.stage = caustics_stageinfo;
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &ccreateinfo, Allocator, &CausticsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create caustics pipeline!");
		}

		VkPipelineShaderStageCreateInfo caustics_blur_x_stageinfo{};
		caustics_blur_x_stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		caustics_blur_x_stageinfo.pName = "main";
		caustics_blur_x_stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		caustics_blur_x_stageinfo.module = computershadermodule_caustics_blur_x;
		ccreateinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		ccreateinfo.layout = CausticsPipelineLayout;
		ccreateinfo.stage = caustics_blur_x_stageinfo;
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &ccreateinfo, Allocator, &CausticsBlurPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create caustics blur X pipeline!");
		}

		VkPipelineShaderStageCreateInfo caustics_blur_y_stageinfo{};
		caustics_blur_y_stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		caustics_blur_y_stageinfo.pName = "main";
		caustics_blur_y_stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		caustics_blur_y_stageinfo.module = computershadermodule_caustics_blur_y;
		ccreateinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		ccreateinfo.layout = CausticsPipelineLayout;
		ccreateinfo.stage = caustics_blur_y_stageinfo;
		if (vkCreateComputePipelines(LDevice, VK_NULL_HANDLE, 1, &ccreateinfo, Allocator, &CausticsBlurPipeline2) != VK_SUCCESS) {
			throw std::runtime_error("failed to create caustics blur Y pipeline!");
		}

		vkDestroyShaderModule(LDevice, computershadermodule_postprocessing, Allocator);
		vkDestroyShaderModule(LDevice, computershadermodule_filtering_x, Allocator);
		vkDestroyShaderModule(LDevice, computershadermodule_filtering_y, Allocator);
		vkDestroyShaderModule(LDevice, computershadermodule_caustics, Allocator);
		vkDestroyShaderModule(LDevice, computershadermodule_caustics_blur_x, Allocator);
		vkDestroyShaderModule(LDevice, computershadermodule_caustics_blur_y, Allocator);
	}
}
void Renderer::CreateFramebuffers()
{
	int w, h;
	glfwGetFramebufferSize(Window, &w, &h);

	VkFramebufferCreateInfo fluidsframebufferinfo{};
	std::array<VkImageView, 2> fluidsattachments = { ThickImageView,CustomDepthImageView};
	fluidsframebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fluidsframebufferinfo.attachmentCount = static_cast<uint32_t>(fluidsattachments.size());
	fluidsframebufferinfo.pAttachments = fluidsattachments.data();
	fluidsframebufferinfo.width = w;
	fluidsframebufferinfo.height = h;
	fluidsframebufferinfo.layers = 1;
	fluidsframebufferinfo.renderPass = FluidGraphicRenderPass;

	if (vkCreateFramebuffer(LDevice, &fluidsframebufferinfo, Allocator, &FluidsFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create fluids framebuffer!");
	}
	
	VkFramebufferCreateInfo fluidsframebufferinfo2{};
	std::array<VkImageView, 2> fluidsattachments2 = { ThickImageView2,CustomDepthImageView2 };
	fluidsframebufferinfo2.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fluidsframebufferinfo2.attachmentCount = static_cast<uint32_t>(fluidsattachments2.size());
	fluidsframebufferinfo2.pAttachments = fluidsattachments2.data();
	fluidsframebufferinfo2.width = w;
	fluidsframebufferinfo2.height = h;
	fluidsframebufferinfo2.layers = 1;
	fluidsframebufferinfo2.renderPass = FluidGraphicRenderPass2;

	if (vkCreateFramebuffer(LDevice, &fluidsframebufferinfo2, Allocator, &FluidsFramebuffer2) != VK_SUCCESS) {
		throw std::runtime_error("failed to create cubes framebuffer!");
	}

	VkFramebufferCreateInfo boxframebufferinfo{};
	std::array<VkImageView, 2> boxattachments = { BackgroundImageView,DepthImageView };
	boxframebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	boxframebufferinfo.attachmentCount = static_cast<uint32_t>(boxattachments.size());
	boxframebufferinfo.pAttachments = boxattachments.data();
	boxframebufferinfo.width = w;
	boxframebufferinfo.height = h;
	boxframebufferinfo.layers = 1;
	boxframebufferinfo.renderPass = BoxGraphicRenderPass;

	if (vkCreateFramebuffer(LDevice, &boxframebufferinfo, Allocator, &BoxFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create box framebuffer!");
	}
}
void Renderer::RecordSimulatingCommandBuffers()
{
	SimulatingCommandBuffers.resize(MAXInFlightRendering);
	for (uint32_t i = 0; i < MAXInFlightRendering; ++i) {
		Base::AllocateCommandBuffer(LDevice, CommandPool, SimulatingCommandBuffers[i]);

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

		vkCmdBindDescriptorSets(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipelineLayout, 0, 1, &SimulateDescriptorSet[i], 0, nullptr);
		vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, wcsphAlgorithm ? SimulatePipeline_WCSPH_Euler : SimulatePipeline_Euler);
		vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

		// --------------------------------------------- Merge Sort  about ----------------------------------------------------------------------------------------- //
		vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ResetGrid);
		vkCmdDispatch(SimulatingCommandBuffers[i], (cellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT, 1, 1);

		vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_Partition);
		vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

		uint32_t tmp_cellCount = cellCount / 2;
		int layer_num = 2;
		PushConstantData constantData = { layer_num };
		// vkCmdPushConstants(SimulatingCommandBuffers[i], SimulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &constantData);

		while (tmp_cellCount > 256)
		{
			vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
			vkCmdPushConstants(SimulatingCommandBuffers[i], SimulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &constantData);
			vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_MergeSort1);
			vkCmdDispatch(SimulatingCommandBuffers[i], (tmp_cellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT, 1, 1);
			tmp_cellCount /= 2;
			layer_num *= 2;
			constantData.layer_num = layer_num;
		}
		vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		vkCmdPushConstants(SimulatingCommandBuffers[i], SimulatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantData), &constantData);
		vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_MergeSort2);
		vkCmdDispatch(SimulatingCommandBuffers[i], 1, 1, 1);

		vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_MergeSortAll);
		vkCmdDispatch(SimulatingCommandBuffers[i], (cellCount + ONE_GROUP_INVOCATION_COUNT - 1) / ONE_GROUP_INVOCATION_COUNT, 1, 1);

		vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
		vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_IndexMapped);
		vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

		//-------------------------------------------------update position & velocity---------------------------------------------------------- //
		vkCmdBindDescriptorSets(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipelineLayout, 0, 1, &SimulateDescriptorSet[i], 0, nullptr);
		if (wcsphAlgorithm) {
			vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
			vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_WCSPH_Pressure);
			vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

			vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
			vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_WCSPH_PressureForce);
			vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

			vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);
			vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_WCSPH_Advect);
			vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);
		}
		else {
			for (int iter = 0; iter < 2; ++iter) {
				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_Lambda);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ResetRigid);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_DeltaPosition);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_RigidPositionUpd);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_PositionUpd);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				// Ellipsoid Update
				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_EllipsoidUpd);
				vkCmdDispatch(SimulatingCommandBuffers[i], 1, 1, 1);

				// Shape Matching
				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ComputeCM);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ComputeCM2);
				vkCmdDispatch(SimulatingCommandBuffers[i], 1, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ComputeApq);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ComputeRotate);
				vkCmdDispatch(SimulatingCommandBuffers[i], 1, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_RigidDeltaPosition);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);
			}

			vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
				, 0, nullptr, 0, nullptr);
			vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_VelocityUpd);
			vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

			vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
				, 0, nullptr, 0, nullptr);
			vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_VelocityCache);
			vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

			if (startAnisotrophic) {
				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_Anisotrophic);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);
			}

			if (startDiffuseParticles && !sphVulkanAlgorithm) {
				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_ResetDiffuse);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_SurfaceDetection);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_FoamGeneration);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_FoamUpd);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT_DIFFUSE, 1, 1);

				vkCmdPipelineBarrier(SimulatingCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memorybarrier
					, 0, nullptr, 0, nullptr);
				vkCmdBindPipeline(SimulatingCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, SimulatePipeline_Compact);
				vkCmdDispatch(SimulatingCommandBuffers[i], WORK_GROUP_COUNT_DIFFUSE, 1, 1);
			}

			vkCmdPipelineBarrier(SimulatingCommandBuffers[i],VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&memorybarrier
			,0,nullptr,0,nullptr);
			vkCmdBindPipeline(SimulatingCommandBuffers[i],VK_PIPELINE_BIND_POINT_COMPUTE,SimulatePipeline_ViscosityCorr);
			vkCmdDispatch(SimulatingCommandBuffers[i],WORK_GROUP_COUNT,1,1); 
		}

		VkBufferMemoryBarrier endBarriers[2] = {};
		endBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		endBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		endBarriers[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		endBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		endBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		endBarriers[0].buffer = ParticleBuffers[i];
		endBarriers[0].offset = 0;
		endBarriers[0].size = VK_WHOLE_SIZE;

		uint32_t barrierCount = 1;
		if (startAnisotrophic) {
			endBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			endBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			endBarriers[1].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			endBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			endBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			endBarriers[1].buffer = AnisotrophicBuffers[0];
			endBarriers[1].offset = 0;
			endBarriers[1].size = VK_WHOLE_SIZE;
			barrierCount = 2;
		}

		vkCmdPipelineBarrier(
			SimulatingCommandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			0,
			0, nullptr,
			barrierCount, endBarriers,
			0, nullptr);

		auto result = vkEndCommandBuffer(SimulatingCommandBuffers[i]);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to end simulating command buffer!");
		}
	}
}
void Renderer::RecordFluidsRenderingCommandBuffers()
{
	for (uint32_t i = 0; i < 2; ++i) {
		FluidsRenderingCommandBuffers[i].resize(SwapChainImages.size());
	}
	for (uint32_t pframe = 0; pframe < 2; ++pframe) {
		for (uint32_t img_idx = 0; img_idx < SwapChainImages.size(); ++img_idx) {
			auto& cb = FluidsRenderingCommandBuffers[pframe][img_idx];
			Base::AllocateCommandBuffer(LDevice, CommandPool, cb);

			VkCommandBufferBeginInfo begininfo{};
			begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			if (vkBeginCommandBuffer(cb, &begininfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin fluids rendering command buffer!");
			}

			VkRenderPassBeginInfo renderpass_begininfo{};
			renderpass_begininfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpass_begininfo.framebuffer = FluidsFramebuffer;
			std::array<VkClearValue, 2> clearvalues{};
			clearvalues[0].color = { {0,0,0,0} };
			clearvalues[1].color = { {255,0,0,0} };
			renderpass_begininfo.clearValueCount = static_cast<uint32_t>(clearvalues.size());
			renderpass_begininfo.pClearValues = clearvalues.data();
			renderpass_begininfo.renderPass = FluidGraphicRenderPass;
			renderpass_begininfo.renderArea.extent = SwapChainImageExtent;
			renderpass_begininfo.renderArea.offset = { 0,0 };
			vkCmdBeginRenderPass(cb, &renderpass_begininfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, FluidGraphicPipelineLayout, 0, 1, &FluidGraphicDescriptorSet, 0, nullptr);
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, FluidGraphicPipeline);

			VkViewport viewport;
			viewport.height = SwapChainImageExtent.height;
			viewport.width = SwapChainImageExtent.width;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f; 
			viewport.x = viewport.y = 0;
			VkRect2D scissor;
			scissor.offset = { 0,0 };
			scissor.extent = SwapChainImageExtent;
			vkCmdSetViewport(cb, 0, 1, &viewport);
			vkCmdSetScissor(cb, 0, 1, &scissor);

			if (startAnisotrophic) {
				// Billboard模式：绑定两个vertex buffer，使用instanced rendering
				VkDeviceSize offsets[2] = { 0, 0 };
				VkBuffer vertexBuffers[] = { ParticleBuffers[pframe], AnisotrophicBuffers[0] };
				vkCmdBindVertexBuffers(cb, 0, 2, vertexBuffers, offsets);
				// 每个粒子(instance)绘制4个顶点，组成billboard quad
				vkCmdDraw(cb, 4, simulatingobj.maxParticles, 0, 0);
			}
			else {
				// 普通模式：只绑定粒子buffer
				VkDeviceSize offsets[1] = { 0 };
				VkBuffer vertexBuffers[] = { ParticleBuffers[pframe] };
				vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);
				vkCmdDraw(cb, simulatingobj.maxParticles, 1, 0, 0);
			}

			// draw cubeParticles ... 
			VkDeviceSize offset = 0;
			if (cubeParticles.size() > 0) {
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, FluidGraphicPipeline3);
				vkCmdBindVertexBuffers(cb, 0, 1, &CubeParticleBuffers[pframe], &offset);
				vkCmdDraw(cb, cubeParticles.size(), 1, 0, 0);
			}

			// draw diffuseParticles (foam/spray/bubble)
			if (startDiffuseParticles && !sphVulkanAlgorithm) {
				vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, DiffuseGraphicPipeline);
				vkCmdBindVertexBuffers(cb, 0, 1, &DiffuseParticleBuffers[pframe], &offset);
				// 绘制所有可能的粒子，死亡粒子在vertex shader中剔除（通过LifeTime判断）
				vkCmdDraw(cb, simulatingobj.maxDiffuseParticles, 1, 0, 0);
			}

			vkCmdEndRenderPass(cb);

			VkImageMemoryBarrier imagebarrier{};
			imagebarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imagebarrier.subresourceRange.baseArrayLayer = 0;
			imagebarrier.subresourceRange.baseMipLevel = 0;
			imagebarrier.subresourceRange.layerCount = 1;
			imagebarrier.subresourceRange.levelCount = 1;
			VkMemoryBarrier memorybarrier{};
			memorybarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;

			memorybarrier.srcAccessMask = 0;
			memorybarrier.dstAccessMask = 0;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 1, &memorybarrier, 0, nullptr, 0, nullptr);

			// 两阶段可分离滤波
			// 第一阶段：X方向滤波 (CustomDepthImage -> FilteredDepthImage2)
			imagebarrier.image = FilteredDepthImage2;
			imagebarrier.srcAccessMask = 0;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, FilterPipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, FilterPipelineLayout, 0, 1, &FilterDescriptorSet, 0, nullptr);
			vkCmdDispatch(cb, SwapChainImageExtent.width / 4, SwapChainImageExtent.height / 4, 1);

			// X方向完成后的同步：FilteredDepthImage2 从写变为读
			imagebarrier.image = FilteredDepthImage2;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			// 第二阶段：Y方向滤波 (FilteredDepthImage2 -> FilteredDepthImage)
			imagebarrier.image = FilteredDepthImage;
			imagebarrier.srcAccessMask = 0;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, FilterPipeline2);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, FilterPipelineLayout, 0, 1, &FilterDescriptorSet2, 0, nullptr);
			vkCmdDispatch(cb, SwapChainImageExtent.width / 4, SwapChainImageExtent.height / 4, 1);

			// Y方向完成后：FilteredDepthImage 从写变为读
			imagebarrier.image = FilteredDepthImage;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			imagebarrier.image = CausticsImage;
			imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, CausticsPipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, CausticsPipelineLayout, 0, 1, &CausticsDescriptorSet, 0, nullptr);
			vkCmdDispatch(cb, SwapChainImageExtent.width / 4, SwapChainImageExtent.height / 4, 1);

			imagebarrier.image = CausticsImage;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			imagebarrier.image = CausticsImage2;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, CausticsBlurPipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, CausticsPipelineLayout, 0, 1, &CausticsBlurDescriptorSet, 0, nullptr);
			vkCmdDispatch(cb, SwapChainImageExtent.width / 4, SwapChainImageExtent.height / 4, 1);

			imagebarrier.image = CausticsImage2;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			imagebarrier.image = CausticsImage;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, CausticsBlurPipeline2);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, CausticsPipelineLayout, 0, 1, &CausticsBlurDescriptorSet2, 0, nullptr);
			vkCmdDispatch(cb, SwapChainImageExtent.width / 4, SwapChainImageExtent.height / 4, 1);

			imagebarrier.image = CausticsImage;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, PostprocessPipeline);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, PostprocessPipelineLayout, 0, 1, &PostprocessDescriptorSets[img_idx], 0, nullptr);
			imagebarrier.image = SwapChainImages[img_idx];
			imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imagebarrier.srcAccessMask = 0;
			imagebarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			vkCmdDispatch(cb, SwapChainImageExtent.width / 4, SwapChainImageExtent.height / 4, 1);

			// transform swapchainimage layout from 'VK_IMAGE_LAYOUT_GENERAL' to 'VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL'
			// for imgui's demands
			imagebarrier.image = SwapChainImages[img_idx];
			imagebarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imagebarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imagebarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imagebarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imagebarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &imagebarrier);

			auto result = vkEndCommandBuffer(cb);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to end fluids rendering command buffer!");
			}
		}
	}
}

void Renderer::RecordRigidRenderingCommandBuffers()
{
	for (uint32_t i = 0; i < 2; ++i) {
		RigidRenderingCommandBuffers[i].resize(SwapChainImages.size());
	}

	for (uint32_t pframe = 0; pframe < 2; ++pframe) {
		for (uint32_t img_idx = 0; img_idx < SwapChainImages.size(); ++img_idx) {

			auto& cb = RigidRenderingCommandBuffers[pframe][img_idx];
			Base::AllocateCommandBuffer(LDevice, CommandPool, cb);

			VkCommandBufferBeginInfo begininfo{};
			begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			if (vkBeginCommandBuffer(cb, &begininfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin rigid rendering command buffer!");
			}

			VkRenderPassBeginInfo renderpass_begininfo{};
			renderpass_begininfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpass_begininfo.framebuffer = FluidsFramebuffer2;
			std::array<VkClearValue, 2> clearvalues{};
			clearvalues[0].color = { {0,0,0,0} };
			clearvalues[1].color = { {1000,0,0,0} };
			renderpass_begininfo.clearValueCount = static_cast<uint32_t>(clearvalues.size());
			renderpass_begininfo.pClearValues = clearvalues.data();
			renderpass_begininfo.renderPass = FluidGraphicRenderPass2;
			renderpass_begininfo.renderArea.extent = SwapChainImageExtent;
			renderpass_begininfo.renderArea.offset = { 0,0 };
			vkCmdBeginRenderPass(cb, &renderpass_begininfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport;
			viewport.height = SwapChainImageExtent.height;
			viewport.width = SwapChainImageExtent.width;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			viewport.x = viewport.y = 0;
			VkRect2D scissor;
			scissor.offset = { 0,0 };
			scissor.extent = SwapChainImageExtent;

			VkDeviceSize offset = 0;

			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, FluidGraphicPipelineLayout, 0, 1, &FluidGraphicDescriptorSet, 0, nullptr);
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, FluidGraphicPipeline2);
			vkCmdSetViewport(cb, 0, 1, &viewport);
			vkCmdSetScissor(cb, 0, 1, &scissor);
			vkCmdBindVertexBuffers(cb, 0, 1, &RigidParticleBuffers[pframe], &offset);
			if (!rigidCubeGenerate)
				vkCmdDraw(cb, rigidParticles.size(), 1, 0, 0);

			vkCmdEndRenderPass(cb);
			auto result = vkEndCommandBuffer(cb);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to end rigid rendering command buffer!");
			}
		}
	}
}

void Renderer::RecordBoxRenderingCommandBuffers()
{
	Base::AllocateCommandBuffer(LDevice, CommandPool, BoxRenderingCommandBuffer);

	VkCommandBufferBeginInfo begininfo{};
	begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begininfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	if (vkBeginCommandBuffer(BoxRenderingCommandBuffer, &begininfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin box rendering command buffer!");
	}
	VkRenderPassBeginInfo renderpass_begininfo{};
	renderpass_begininfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	std::array<VkClearValue, 2> clearvalues{};
	clearvalues[0] = { {0.5,0.5,0.5,1} };
	clearvalues[1].depthStencil = { 1 };
	renderpass_begininfo.clearValueCount = static_cast<uint32_t>(clearvalues.size());
	renderpass_begininfo.pClearValues = clearvalues.data();
	renderpass_begininfo.framebuffer = BoxFramebuffer;
	renderpass_begininfo.renderArea.extent = SwapChainImageExtent;
	renderpass_begininfo.renderArea.offset = { 0,0 };
	renderpass_begininfo.renderPass = BoxGraphicRenderPass;

	vkCmdBeginRenderPass(BoxRenderingCommandBuffer, &renderpass_begininfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport;
	viewport.height = SwapChainImageExtent.height;
	viewport.width = SwapChainImageExtent.width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = viewport.y = 0;
	VkRect2D scissor;
	scissor.offset = { 0,0 };
	scissor.extent = SwapChainImageExtent;
	vkCmdSetViewport(BoxRenderingCommandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(BoxRenderingCommandBuffer, 0, 1, &scissor);
	VkDeviceSize offsets[] = { 0 };

	// sphere draw
	if (sphere_resource.graphicPipeline != VK_NULL_HANDLE && sphere_resource.pipelineLayout != VK_NULL_HANDLE && sphere_resource.descriptorSet != VK_NULL_HANDLE &&
		sphere_resource.vertexBuffer != VK_NULL_HANDLE && sphere_resource.indexBuffer != VK_NULL_HANDLE && !sphere_resource.indices.empty()) {
		vkCmdBindPipeline(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sphere_resource.graphicPipeline);
		vkCmdBindDescriptorSets(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sphere_resource.pipelineLayout, 0, 1, &sphere_resource.descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(BoxRenderingCommandBuffer, 0, 1, &sphere_resource.vertexBuffer, offsets);
		vkCmdBindIndexBuffer(BoxRenderingCommandBuffer, sphere_resource.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(BoxRenderingCommandBuffer, static_cast<uint32_t>(sphere_resource.indices.size()), 1, 0, 0, 0);
	}

	// don't consider obj_resource drawing 
	// 1. dragon draw
	// 2. other objs draw   1. cylinder  2. table   3. sand   4....
	for (size_t obj_i = 1; obj_i < objResManagements.size(); obj_i++) {
		ObjResource* objR = objResManagements[obj_i].obj_res;
		if (objR == nullptr || objR->graphicPipeline == VK_NULL_HANDLE || objR->pipelineLayout == VK_NULL_HANDLE || objR->descriptorSet == VK_NULL_HANDLE ||
			objR->vertexBuffer == VK_NULL_HANDLE || objR->indexBuffer == VK_NULL_HANDLE || objR->indices.empty()) {
			continue;
		}
		vkCmdBindPipeline(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objR->graphicPipeline);
		vkCmdBindDescriptorSets(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objR->pipelineLayout, 0, 1, &objR->descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(BoxRenderingCommandBuffer, 0, 1, &objR->vertexBuffer, offsets);
		vkCmdBindIndexBuffer(BoxRenderingCommandBuffer, objR->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(BoxRenderingCommandBuffer, static_cast<uint32_t>(objR->indices.size()), 1, 0, 0, 0);
	}

	// rigid cube draw
	if (rigidCubeGenerate) {
		rigidCube->RecordRigidCubeCommandBuffer(BoxRenderingCommandBuffer);
	}

	// skybox draw
	if (skybox_resource.graphicPipeline != VK_NULL_HANDLE && skybox_resource.pipelineLayout != VK_NULL_HANDLE && skybox_resource.descriptorSet != VK_NULL_HANDLE &&
		skybox_resource.vertexBuffer != VK_NULL_HANDLE && skybox_resource.indexBuffer != VK_NULL_HANDLE && !skyboxIndices.empty()) {
		vkCmdBindPipeline(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_resource.graphicPipeline);
		vkCmdBindDescriptorSets(BoxRenderingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_resource.pipelineLayout, 0, 1, &skybox_resource.descriptorSet, 0, nullptr);
		vkCmdBindVertexBuffers(BoxRenderingCommandBuffer, 0, 1, &skybox_resource.vertexBuffer, offsets);
		vkCmdBindIndexBuffer(BoxRenderingCommandBuffer, skybox_resource.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(BoxRenderingCommandBuffer, static_cast<uint32_t>(skyboxIndices.size()), 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(BoxRenderingCommandBuffer);

	auto result = vkEndCommandBuffer(BoxRenderingCommandBuffer);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to end box rendering command buffer!");
	}

}

// ----------------------------------------------------- draw ----------------------------------------------------------------- //
TickWindowResult Renderer::TickWindow(float DeltaTime)
{
	if (glfwWindowShouldClose(Window)) return TickWindowResult::EXIT;
	glfwPollEvents();
	int w, h;
	glfwGetFramebufferSize(Window, &w, &h);
	if (w * h == 0) {
		return TickWindowResult::HIDE;
	}
	return TickWindowResult::NONE;
}

void Renderer::Simulate()
{
	CurrentFlight = (CurrentFlight + 1) % MAXInFlightRendering;
	vkQueueWaitIdle(GraphicNComputeQueue); 
	if (!mpmAlgorithm) {
		InjectWaterColumn();
	}
	std::vector<VkCommandBuffer> submitCommandBuffers;
	submitCommandBuffers.push_back(SimulatingCommandBuffers[CurrentFlight]);
	if (cloth && clothGenerate) {
		VkCommandBuffer clothCommandBuffer = cloth->getCommandBufferForSimulation(CurrentFlight);
		submitCommandBuffers.push_back(clothCommandBuffer);
	}
	VkSubmitInfo submitinfo{};
	submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.commandBufferCount = submitCommandBuffers.size();
	submitinfo.pCommandBuffers = submitCommandBuffers.data();
	submitinfo.signalSemaphoreCount = 1;
	submitinfo.pSignalSemaphores = &SimulatingFinish[CurrentFlight];

	if (vkQueueSubmit(GraphicNComputeQueue, 1, &submitinfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit simulating command buffer!");
	}
}

void Renderer::BoxRender(uint32_t dstimage)
{
	std::array<VkCommandBuffer, 1> uploadingCommandBuffers =
	{ BoxRenderingCommandBuffer};
	std::vector<VkSemaphore> waitsems = { ImageAvaliable[CurrentFlight] };
	std::vector<VkPipelineStageFlags> waitstages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo rendering_submitinfo{};
	rendering_submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	rendering_submitinfo.commandBufferCount = static_cast<uint32_t> (uploadingCommandBuffers.size());
	rendering_submitinfo.pCommandBuffers = uploadingCommandBuffers.data();
	rendering_submitinfo.waitSemaphoreCount = static_cast<uint32_t> (waitsems.size());
	rendering_submitinfo.pWaitSemaphores = waitsems.data();
	rendering_submitinfo.pWaitDstStageMask = waitstages.data();
	rendering_submitinfo.pSignalSemaphores = &BoxRenderingFinish[CurrentFlight];
	rendering_submitinfo.signalSemaphoreCount = 1;
	if (vkQueueSubmit(GraphicNComputeQueue, 1, &rendering_submitinfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit box rendering command buffer!");
	}
}
void Renderer::FluidsRender(uint32_t dstimage)
{
	std::vector<VkCommandBuffer> uploadingCommandBuffers;
	uploadingCommandBuffers.push_back(FluidsRenderingCommandBuffers[CurrentFlight][dstimage]);

	if (specialParticleGenerate && specialParticle) {
		VkCommandBuffer sp_cb = specialParticle->getCommandBufferForImage(dstimage);
		if (sp_cb != VK_NULL_HANDLE) {
			uploadingCommandBuffers.push_back(sp_cb);
		}
	}
	if (clothGenerate && cloth) {
		VkCommandBuffer cloth_cb = cloth->getCommandBufferForImage(dstimage);
		uploadingCommandBuffers.push_back(cloth_cb);
	}
	if (sdf && isSDF && true) {
		//VkCommandBuffer sdf_cb = sdf->GetSimCommandBuffer();
		for (auto cb : sdfCommandBuffers) {
			uploadingCommandBuffers.push_back(*cb);
		}
	}

	uploadingCommandBuffers.push_back(ImGuiCommandBuffers[CurrentFlight]);

	VkSubmitInfo rendering_submitinfo{};
	rendering_submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	rendering_submitinfo.commandBufferCount = static_cast<uint32_t>(uploadingCommandBuffers.size());
	rendering_submitinfo.pCommandBuffers = uploadingCommandBuffers.data();
	std::array<VkSemaphore, 2> rendering_waitsems = { BoxRenderingFinish[CurrentFlight], CubesRenderingFinish[CurrentFlight] };
	std::array<VkPipelineStageFlags, 2> rendering_waitstages = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
	rendering_submitinfo.waitSemaphoreCount = static_cast<uint32_t>(rendering_waitsems.size());
	rendering_submitinfo.pWaitSemaphores = rendering_waitsems.data();
	rendering_submitinfo.pWaitDstStageMask = rendering_waitstages.data();
	rendering_submitinfo.pSignalSemaphores = &FluidsRenderingFinish[CurrentFlight];
	rendering_submitinfo.signalSemaphoreCount = 1;
	if (vkQueueSubmit(GraphicNComputeQueue, 1, &rendering_submitinfo, DrawingFence[CurrentFlight]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit fluids rendering command buffer!");
	}

	// use cube particle to present voxel
	if (!isVoxelize && !mpmAlgorithm) {
		isVoxelize = true;
		vkDeviceWaitIdle(LDevice);
		cubeParticles.resize(0);
		std::vector<uint32_t> voxels;
		std::vector<float> sdfValues;
		{
			// dragon voxel
			Scene::VoxelWithCubeParticle(PDevice, LDevice, CommandPool, GraphicNComputeQueue, obj_r2, cubeParticles, voxels, true, false);
			
			// other objs (collsion objs...)  voxelized xxx
			ObjResource* duckFloat = scene_set->GetObjResource_DuckFloat();
			Scene::VoxelWithCubeParticle(PDevice, LDevice, CommandPool, GraphicNComputeQueue, *duckFloat, cubeParticles, voxels, false, true);

			// sdf in cpu  
			if (sdf && isSDF) {
				StartSDF(duckFloat, voxels, sdfValues);
				isSDF = false;
			}
		}
		cubeCpyParticles = cubeParticles;
		simulatingobj.cubeParticleNum = cubeParticles.size();
		SetSimulatingObj(simulatingobj);
		// recreate cubeparticle buffer and descriptorset
		for (uint32_t i = 0; i < MAXInFlightRendering; i++) {
			CleanupBuffer(CubeParticleBuffers[i], CubeParticleBufferMemory[i], false);
		}
		CreateCubeParticleBuffer();
		CreateDescriptorSet();
		RecordFluidsRenderingCommandBuffers();
		RecordSimulatingCommandBuffers();
	}
}
void Renderer::CubesRender(uint32_t dstimage)
{
	std::vector<VkCommandBuffer> uploadingCommandBuffers = { RigidRenderingCommandBuffers[CurrentFlight][dstimage] };
	VkSubmitInfo rendering_submitinfo{};
	rendering_submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	rendering_submitinfo.commandBufferCount = 1;
	rendering_submitinfo.pCommandBuffers = uploadingCommandBuffers.data();
	std::array<VkSemaphore, 1> rendering_waitsems = { SimulatingFinish[CurrentFlight] };
	std::array<VkPipelineStageFlags, 1> rendering_waitstages = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
	rendering_submitinfo.waitSemaphoreCount = static_cast<uint32_t>(rendering_waitsems.size());
	rendering_submitinfo.pWaitSemaphores = rendering_waitsems.data();
	rendering_submitinfo.pWaitDstStageMask = rendering_waitstages.data();
	rendering_submitinfo.pSignalSemaphores = &CubesRenderingFinish[CurrentFlight];
	rendering_submitinfo.signalSemaphoreCount = 1;
	if (vkQueueSubmit(GraphicNComputeQueue, 1, &rendering_submitinfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit cubes rendering command buffer!");
	}
}
void Renderer::Draw()
{
	uint64_t notimeout = UINT64_MAX;
	VkResult result;

	uint32_t image_idx;  // 0, 1, 2

	vkWaitForFences(LDevice, 1, &DrawingFence[CurrentFlight], VK_TRUE, notimeout);
	vkResetFences(LDevice, 1, &DrawingFence[CurrentFlight]);

	result = vkAcquireNextImageKHR(LDevice, SwapChain, notimeout, ImageAvaliable[CurrentFlight], VK_NULL_HANDLE, &image_idx);

	BoxRender(image_idx);
	CubesRender(image_idx);

	ConfigImGui(image_idx);
	FluidsRender(image_idx);

	std::vector<VkSemaphore> waitsems = {
		FluidsRenderingFinish[CurrentFlight]
	};

	VkPresentInfoKHR presentinfo{};
	presentinfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.swapchainCount = 1;
	presentinfo.pSwapchains = &SwapChain;
	presentinfo.pWaitSemaphores = waitsems.data();
	presentinfo.waitSemaphoreCount = static_cast<uint32_t>(waitsems.size());
	presentinfo.pImageIndices = &image_idx;
	result = vkQueuePresentKHR(PresentQueue, &presentinfo);
	if (result != VK_SUCCESS) {
		std::cerr << "vkQueuePresentKHR failed with error: " << result << std::endl;
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			RecreateSwapChain();
		}
		else {
			throw std::runtime_error("Failed to present queue");
		}
		return;
	}
}

void Renderer::WaitIdle()
{
	vkDeviceWaitIdle(LDevice);
}

// ---------------------------------------- imgui 相关 --------------------------------------------------------------------------//
void Renderer::CreateImGuiResources()
{
	QueuefamliyIndices familyIndices = Base::GetPhysicalDeviceQueueFamilyIndices(PDevice, Surface);
	DearGui::CreateGuiDescriptorPool(LDevice, ImGuiDescriptorPool);

	DearGui::CreateGuiCommandBuffers(LDevice, familyIndices, MAXInFlightRendering, ImGuiCommandPool, ImGuiCommandBuffers);

	DearGui::CreateGuiRenderPass(LDevice, SwapChainImageFormat, ImGuiRenderPass);

	DearGui::CreateGuiFrameBuffers(LDevice, SwapChainImageViews, ImGuiRenderPass, SwapChainImageExtent, ImGuiFrameBuffers);

	DearGui::InitImGui(Window, Instance, PDevice, LDevice, familyIndices, GraphicNComputeQueue, ImGuiDescriptorPool,
		ImGuiRenderPass, SwapChainImages);
}

void Renderer::ConfigImGui(uint32_t image_index)
{
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	float particle_radius = renderingobj.particleRadius;
	int maxParticles = simulatingobj.maxParticles;
	int maxCubeParticles = simulatingobj.maxCubeParticles;
	int currentRigidParticles = simulatingobj.rigidParticles;
	// 控件
	{
		ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Once); 
		ImGui::SetNextWindowSize(ImVec2(380, 520), ImGuiCond_Once); 
		ImGui::Begin("Fluid Simulation Control", nullptr, ImGuiWindowFlags_NoCollapse); 

		if (ImGui::CollapsingHeader("Simulation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Water Column Injector", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Spacing();
				ImGui::Checkbox("Enable Injector", &injectorEnabled);
				const char* injectorSides[] = { "X Min -> +X", "X Max -> -X" };
				ImGui::PushItemWidth(-1);
				ImGui::Combo("##InjectorSide", &injectorSide, injectorSides, IM_ARRAYSIZE(injectorSides));
				ImGui::PopItemWidth();
				ImGui::PushItemWidth(-120);
				ImGui::Text("Radius");
				ImGui::SliderFloat("##InjectorRadius", &injectorRadius, 0.01f, 0.30f, "%.3f");
				ImGui::Text("Center Y");
				ImGui::SliderFloat("##InjectorCenterY", &injectorCenterY, boxinfobj.clampY.x, boxinfobj.clampY.y, "%.3f");
				ImGui::Text("Center Z");
				ImGui::SliderFloat("##InjectorCenterZ", &injectorCenterZ, boxinfobj.clampZ.x, boxinfobj.clampZ.y, "%.3f");
				ImGui::Text("Rate (particles/s)");
				ImGui::SliderFloat("##InjectorRate", &injectorRate, 0.0f, 20000.0f, "%.0f");
				ImGui::Text("Speed");
				ImGui::SliderFloat("##InjectorSpeed", &injectorSpeed, 0.0f, 10.0f, "%.2f");
				ImGui::PopItemWidth();
				if (ImGui::Button("Reset Injector Accumulator", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
					injectorAccumulator = 0.0f;
				}
				ImGui::Spacing();
			}
			ImGui::PushItemWidth(-120);
			
			ImGui::Text("Viscosity");
			ImGui::SliderFloat("##viscosity", &simulatingobj.visc, 0.01f, 1.00f, "%.2f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Controls fluid thickness\n0.01 = water, 1.0 = honey");
			
			ImGui::Text("SPH Radius");
			ImGui::SliderFloat("##sphRadius", &simulatingobj.sphRadius, 2.5f * particle_radius, 5.5f * particle_radius, "%.4f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Smoothing kernel radius for neighbor search");
			
			ImGui::Text("External Force");
			ImGui::SliderFloat("##force", &simulatingobj.k, 1.0, 100.0, "%.1f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Strength of mouse interaction force");
			
			ImGui::Text("Relaxation");
			ImGui::SliderFloat("##relaxation", &simulatingobj.relaxation, 0.5, 1.0, "%.2f");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Position correction relaxation factor");
			
			ImGui::PopItemWidth();
			
			ImGui::Spacing();
			if (ImGui::TreeNode("Advanced SCORR Settings")) {
				ImGui::PushItemWidth(-1);
				ImGui::InputFloat("K Coefficient", &simulatingobj.scorrK, 0.0001f, 0.9999f, "%.4f");
				ImGui::InputFloat("Q Parameter", &simulatingobj.scorrQ, 0.1f, 0.9f, "%.1f");
				ImGui::InputFloat("N Exponent", &simulatingobj.scorrN, 0.1f, 9.9f, "%.1f");
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// Artificial Viscosity Settings
			bool artViscEnabled = (simulatingobj.enableArtificialViscosity != 0);
			if (ImGui::Checkbox("Enable Artificial Viscosity", &artViscEnabled)) {
				simulatingobj.enableArtificialViscosity = artViscEnabled ? 1 : 0;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Particle-particle artificial viscosity\n(Monaghan 1992)");
			
			if (artViscEnabled) {
				ImGui::PushItemWidth(-120);
				ImGui::Text("Alpha Coefficient");
				ImGui::SliderFloat("##artVisAlpha", &simulatingobj.artificialViscosityAlpha, 0.0f, 0.2f, "%.3f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Controls viscous damping\nRecommended: 0.01-0.1");
				
				ImGui::Text("Beta Coefficient");
				ImGui::SliderFloat("##artVisBeta", &simulatingobj.artificialViscosityBeta, 0.0f, 0.1f, "%.3f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Usually set to 0\nNon-zero for high velocity impacts");
				ImGui::PopItemWidth();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// Surface Tension Settings
			bool surfTensionEnabled = (simulatingobj.enableSurfaceTension != 0);
			if (ImGui::Checkbox("Enable Surface Tension", &surfTensionEnabled)) {
				simulatingobj.enableSurfaceTension = surfTensionEnabled ? 1 : 0;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Color field based surface tension\n(Muller et al. 2003)");
			
			if (surfTensionEnabled) {
				ImGui::PushItemWidth(-120);
				ImGui::Text("Tension Coefficient");
				ImGui::SliderFloat("##surfTensionCoef", &simulatingobj.surfaceTensionCoefficient, 0.0f, 0.1f, "%.4f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Surface tension strength\nRecommended: 0.001-0.05");
				
				ImGui::Text("Surface Threshold");
				ImGui::SliderFloat("##surfTensionThresh", &simulatingobj.surfaceTensionThreshold, 0.01f, 2.0f, "%.2f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Gradient threshold to detect surface\nLower = more sensitive");
				ImGui::PopItemWidth();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// Anisotropic Kernel Settings
			if (ImGui::TreeNode("Anisotropic Kernel (Yu & Turk 2013)")) {
				ImGui::PushItemWidth(-120);
				
				ImGui::Text("Base Radius Scale");
				ImGui::SliderFloat("##aniBaseScale", &simulatingobj.anisotropicBaseRadiusScale, 1.0f, 3.0f, "%.2f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Base radius multiplier\nDefault: 1.5");
				
				ImGui::Text("Max Anisotropy Ratio");
				ImGui::SliderFloat("##aniMaxRatio", &simulatingobj.anisotropicMaxRatio, 2.0f, 10.0f, "%.1f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Max ratio between longest/shortest axis\nDefault: 5.0");
				
				ImGui::Text("Min Radius Scale");
				ImGui::SliderFloat("##aniMinScale", &simulatingobj.anisotropicMinScale, 0.1f, 0.8f, "%.2f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Minimum radius scale\nDefault: 0.2");
				
				ImGui::Text("Max Radius Scale");
				ImGui::SliderFloat("##aniMaxScale", &simulatingobj.anisotropicMaxScale, 0.5f, 2.0f, "%.2f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Maximum radius scale\nDefault: 1.0");
				
				ImGui::Text("Neighbor Threshold");
				ImGui::SliderFloat("##aniNeighborThresh", &simulatingobj.anisotropicNeighborThreshold, 2.0f, 20.0f, "%.1f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Neighbor count for anisotropy blend\nDefault: 6.0");
				
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Wave Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
				bool waveEnabled = (simulatingobj.waveEnabled != 0);
				if (ImGui::Checkbox("Enable Waves", &waveEnabled)) {
					simulatingobj.waveEnabled = waveEnabled ? 1 : 0;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Applies a surface perturbation (wave-like velocity)\nDriven by accumulated time");
				
				ImGui::PushItemWidth(-120);
				
				ImGui::Text("Wave Intensity");
				ImGui::SliderFloat("##waveIntensity", &simulatingobj.waveIntensity, 0.0f, 3.0f, "%.2f");
				
				ImGui::Text("Wave Steepness");
				ImGui::SliderFloat("##waveSteepness", &simulatingobj.waveSteepness, 0.0f, 1.0f, "%.2f");
				
				ImGui::Text("Base Height");
				ImGui::SliderFloat("##waveBaseHeight", &simulatingobj.waveBaseHeight, boxinfobj.clampY.x, boxinfobj.clampY.y, "%.3f");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Used to estimate surface depth: depth = baseHeight - position.y\nSet close to your free-surface height");
				
				ImGui::Text("Wave Period (s)");
				ImGui::SliderFloat("##wavePeriod", &simulatingobj.wavePeriod, 0.2f, 5.0f, "%.2f");
				
				float windDir[2] = { simulatingobj.windDirX, simulatingobj.windDirZ };
				ImGui::Text("Wind Direction XZ");
				if (ImGui::SliderFloat2("##windDir", windDir, -1.0f, 1.0f, "%.2f")) {
					glm::vec2 d(windDir[0], windDir[1]);
					float len = glm::length(d);
					if (len > 0.0001f) {
						d /= len;
						windDir[0] = d.x;
						windDir[1] = d.y;
					}
					else {
						windDir[0] = 1.0f;
						windDir[1] = 0.0f;
					}
					simulatingobj.windDirX = windDir[0];
					simulatingobj.windDirZ = windDir[1];
				}
				
				ImGui::Text("Wind Strength");
				ImGui::SliderFloat("##windStrength", &simulatingobj.windStrength, 0.0f, 2.0f, "%.2f");
				
				ImGui::PopItemWidth();
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("SDF Export", ImGuiTreeNodeFlags_DefaultOpen)) {
				static int sdfExportSelection = 0;
				static char sdfExportStatus[256] = { 0 };
				std::vector<size_t> sdfExportIndices;
				std::vector<std::string> sdfExportLabels;
				sdfExportIndices.reserve(objResManagements.size());
				sdfExportLabels.reserve(objResManagements.size());
				for (size_t i = 0; i < objResManagements.size(); ++i) {
					if (!objResManagements[i].isVox || objResManagements[i].obj_res == nullptr) continue;
					std::string label = objResManagements[i].displayName;
					if (label.empty()) {
						label = std::string("Obj ") + std::to_string(i);
					}
					sdfExportIndices.push_back(i);
					sdfExportLabels.push_back(label);
				}
				if (!sdfExportLabels.empty()) {
					if (sdfExportSelection < 0) sdfExportSelection = 0;
					if (sdfExportSelection >= static_cast<int>(sdfExportLabels.size())) sdfExportSelection = 0;
					std::vector<const char*> sdfExportLabelPtrs;
					sdfExportLabelPtrs.reserve(sdfExportLabels.size());
					for (auto& s : sdfExportLabels) {
						sdfExportLabelPtrs.push_back(s.c_str());
					}
					ImGui::PushItemWidth(-1);
					ImGui::Combo("##SDFExportModel", &sdfExportSelection, sdfExportLabelPtrs.data(), static_cast<int>(sdfExportLabelPtrs.size()));
					ImGui::PopItemWidth();
					if (ImGui::Button("Export SDF to ./sdf", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
						try {
							ExportSDFField(sdfExportIndices[static_cast<size_t>(sdfExportSelection)], "./sdf");
							strcpy_s(sdfExportStatus, sizeof(sdfExportStatus), "Exported to ./sdf");
						} catch (const std::exception& e) {
							strcpy_s(sdfExportStatus, sizeof(sdfExportStatus), e.what());
						}
					}
					if (sdfExportStatus[0] != 0) {
						ImGui::TextWrapped("%s", sdfExportStatus);
					}
				} else {
					ImGui::TextWrapped("No voxelized models available for export.");
				}
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Rendering Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Spacing();
			
			ImGui::Text("Fluid Color");
			ImGui::ColorEdit4("##color", colorWithAlpha, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);
			
			ImGui::Spacing();
			
			ImGui::Text("Render Mode");
			const char* renderModes[] = { "Final Image", "Thickness", "Depth", "Normals" };
			static int currentRenderMode = 0;
			ImGui::PushItemWidth(-1);
			if (ImGui::Combo("##RenderMode", &currentRenderMode, renderModes, IM_ARRAYSIZE(renderModes))) {
				renderingobj.renderType = currentRenderMode;
				SetRenderingObj(renderingobj);
			}
			
			ImGui::Spacing();
			
			ImGui::Text("Fluid Type Preset");
			const char* fluidModes[] = { "Water", "Milk", "Orange Juice", "Honey", "Glass", "Ocean", "PBF Style" };
			static int currentFluidMode = 6;
			if (ImGui::Combo("##FluidType", &currentFluidMode, fluidModes, IM_ARRAYSIZE(fluidModes))) {
				renderingobj.fluidType = currentFluidMode;
				SetRenderingObj(renderingobj);
				
				// Auto-adjust viscosity
				if (currentFluidMode == 1) simulatingobj.visc = 0.8;       // Milk
				else if (currentFluidMode == 2) simulatingobj.visc = 0.3;  // Orange Juice
				else if (currentFluidMode == 3) simulatingobj.visc = 0.8;  // Honey
			}
			ImGui::PopItemWidth();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			ImGui::Text("Depth Filtering");
			static int selectedOption = 0;
			ImGui::PushStyleColor(ImGuiCol_Button, selectedOption == 0 ? ImVec4(0.26f, 0.80f, 0.90f, 0.8f) : ImVec4(0.20f, 0.25f, 0.30f, 1.0f));
			if (ImGui::Button("Enabled", ImVec2(120, 0))) { selectedOption = 0; }
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, selectedOption == 1 ? ImVec4(0.26f, 0.80f, 0.90f, 0.8f) : ImVec4(0.20f, 0.25f, 0.30f, 1.0f));
			if (ImGui::Button("Disabled", ImVec2(120, 0))) { selectedOption = 1; }
			ImGui::PopStyleColor();
			
			if (selectedOption == 0) {
				renderingobj.isFilter = true;
				int fR = renderingobj.filterRadius;
				ImGui::Text("Filter Radius");
				ImGui::PushItemWidth(-1);
				ImGui::SliderInt("##filterRadius", &fR, 1, 50);
				ImGui::PopItemWidth();
				renderingobj.filterRadius = fR;
			}
			else {
				renderingobj.isFilter = false;
			}
			ImGui::Spacing();
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Lighting Settings")) {
			ImGui::Spacing();
			ImGui::Text("Ambient Color");
			float ambientCol[3] = { renderingobj.ambientColor.x, renderingobj.ambientColor.y, renderingobj.ambientColor.z };
			if (ImGui::ColorEdit3("##AmbientColor", ambientCol)) {
				renderingobj.ambientColor = glm::vec3(ambientCol[0], ambientCol[1], ambientCol[2]);
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// 光源数量显示
			int activeLights = 0;
			for (int i = 0; i < 3; i++) {
				if (renderingobj.lights[i].enabled) activeLights++;
			}
			ImGui::Text("Active Lights: %d / 3", activeLights);
			
			ImGui::Spacing();
			
			const char* lightTypeNames[] = { "Point", "Directional" };
			
			for (int i = 0; i < 3; i++) {
				ImGui::PushID(i);
				
				char headerName[32];
				snprintf(headerName, sizeof(headerName), "Light %d", i + 1);
				
				bool enabled = renderingobj.lights[i].enabled != 0;
				if (ImGui::Checkbox(headerName, &enabled)) {
					renderingobj.lights[i].enabled = enabled ? 1 : 0;
				}
				if (enabled) {
					ImGui::Indent();
					int lightType = renderingobj.lights[i].type;
					ImGui::Text("Type");
					ImGui::SameLine(100);
					ImGui::PushItemWidth(150);
					if (ImGui::Combo("##LightType", &lightType, lightTypeNames, IM_ARRAYSIZE(lightTypeNames))) {
						renderingobj.lights[i].type = lightType;
					}
					ImGui::PopItemWidth();
					
					if (lightType == 0) {
						ImGui::Text("Position");
					} else {
						ImGui::Text("Direction");
					}
					float pos[3] = { renderingobj.lights[i].position.x, renderingobj.lights[i].position.y, renderingobj.lights[i].position.z };
					ImGui::PushItemWidth(-1);
					if (ImGui::DragFloat3("##LightPos", pos, 0.1f, -10.0f, 10.0f, "%.2f")) {
						renderingobj.lights[i].position = glm::vec3(pos[0], pos[1], pos[2]);
					}
					ImGui::PopItemWidth();
					
					// 颜色
					ImGui::Text("Color");
					float col[3] = { renderingobj.lights[i].color.x, renderingobj.lights[i].color.y, renderingobj.lights[i].color.z };
					if (ImGui::ColorEdit3("##LightColor", col)) {
						renderingobj.lights[i].color = glm::vec3(col[0], col[1], col[2]);
					}
					
					// 强度
					ImGui::Text("Intensity");
					ImGui::PushItemWidth(-1);
					float intensity = renderingobj.lights[i].intensity;
					if (ImGui::SliderFloat("##LightIntensity", &intensity, 0.0f, 3.0f, "%.2f")) {
						renderingobj.lights[i].intensity = intensity;
					}
					ImGui::PopItemWidth();
					
					ImGui::Unindent();
				}
				
				ImGui::PopID();
				
				if (i < 2) {
					ImGui::Spacing();
				}
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			// 焦散参数
			ImGui::Text("Caustics Settings");
			ImGui::PushItemWidth(-1);
			static bool causticsInit = false;
			static bool causticsEnabled = true;
			static float causticsIntensityBackup = 0.15f;
			if (!causticsInit) {
				causticsInit = true;
				causticsEnabled = (renderingobj.causticsIntensity > 0.0f);
				causticsIntensityBackup = (renderingobj.causticsIntensity > 0.0f) ? renderingobj.causticsIntensity : causticsIntensityBackup;
			}

			bool causticsChanged = false;
			if (ImGui::Checkbox("Enable Caustics", &causticsEnabled)) {
				causticsChanged = true;
				if (!causticsEnabled) {
					if (renderingobj.causticsIntensity > 0.0f) {
						causticsIntensityBackup = renderingobj.causticsIntensity;
					}
					renderingobj.causticsIntensity = 0.0f;
				}
				else {
					renderingobj.causticsIntensity = (causticsIntensityBackup > 0.0f) ? causticsIntensityBackup : 0.15f;
				}
			}

			if (causticsEnabled) {
				if (ImGui::SliderFloat("##CausticsIntensity", &renderingobj.causticsIntensity, 0.0f, 1.0f, "Intensity: %.2f")) {
					causticsChanged = true;
					causticsIntensityBackup = renderingobj.causticsIntensity;
				}
			}
			else {
				if (ImGui::SliderFloat("##CausticsIntensity", &causticsIntensityBackup, 0.0f, 1.0f, "Intensity: %.2f")) {
					causticsChanged = true;
				}
			}
			if (ImGui::SliderFloat("##CausticsRadius", &renderingobj.causticsPhotonRadius, 1.0f, 50.0f, "Radius: %.1f")) {
				causticsChanged = true;
			}
			ImGui::PopItemWidth();
			if (causticsChanged) {
				SetRenderingObj(renderingobj);
			}
			
			ImGui::Spacing();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		ImVec2 buttonSize(ImGui::GetContentRegionAvail().x, 35);
		if (ImGui::Button("Recreate Rigid Body", buttonSize)) {
			std::cout << "rigid recreate...\n";
			if (!rigidCubeGenerate) {
				rigidCube = new RigidCube();
				rigidCube->PreCreateResources(
					PDevice, LDevice, CommandPool, GraphicNComputeQueue, BoxGraphicRenderPass, DescriptorPool,
					originRigidParticles, smi, renderingobj, ShapeMatchingBuffers[0]
				);
			}
			rigidCubeGenerate = true;
			vkDeviceWaitIdle(LDevice);
			RecordFluidsRenderingCommandBuffers();
			RecordRigidRenderingCommandBuffers();
			RecordBoxRenderingCommandBuffers();
		}
		
		ImGui::End();
	}
	{
		ImGui::SetNextWindowPos(ImVec2(15, Height - 215), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(380, 200), ImGuiCond_Once);
		ImGui::Begin("Simulation Statistics", nullptr, ImGuiWindowFlags_NoCollapse);

		// Performance
		if (fps >= 60.0f)
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "FPS: %.1f", fps);
		else if (fps >= 30.0f)
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "FPS: %.1f", fps);
		else
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "FPS: %.1f", fps);
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		// Particle counts
		ImGui::Text("Particle Counts:");
		ImGui::BulletText("Fluid: %d / %d", int(simulatingobj.numParticles), maxParticles);
		ImGui::BulletText("Object: %d / %d", int(cubeParticles.size()), maxCubeParticles);
		ImGui::BulletText("Rigid: %d", currentRigidParticles);
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		// Camera info
		ImGui::Text("Camera:");
		ImGui::BulletText("Pos: (%.1f, %.1f, %.1f)", cameraPos[0], cameraPos[1], cameraPos[2]);
		ImGui::BulletText("Target: (%.1f, %.1f, %.1f)", cameraTarget[0], cameraTarget[1], cameraTarget[2]);
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		// Interaction mode
		ImGui::Text("Interaction Mode:");
		if (isEmitFluids) {
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "  Add Fluids");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "  Apply Force");
			if (simulatingobj.attract)
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  [Active]");
		}
		
		ImGui::End();
	}
	ImGui::Render();

	ImDrawData* draw_data = ImGui::GetDrawData();

	// Record ImGui Command Buffers
	DearGui::RecordImGuiCommandBuffers2(image_index, draw_data, ImGuiCommandBuffers[CurrentFlight],
		ImGuiRenderPass, ImGuiFrameBuffers, SwapChainImageExtent);
}

void Renderer::CleanupImGui()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	vkDestroyDescriptorPool(LDevice, ImGuiDescriptorPool, nullptr);

	for (auto frameBuffer : ImGuiFrameBuffers) {
		vkDestroyFramebuffer(LDevice, frameBuffer, nullptr);
	}
	vkDestroyRenderPass(LDevice, ImGuiRenderPass, nullptr);
	vkDestroyCommandPool(LDevice, ImGuiCommandPool, nullptr);
}

