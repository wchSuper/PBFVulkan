#include"renderer.h"
#include"renderer_types.h"
#include"glm/gtc/matrix_transform.hpp"

#include<iostream>
#include<exception>
#include<chrono>
#include<string>
#include<algorithm>
#include <filesystem>
#include <vulkan/vulkan.h>

#undef APIENTRY
#define NOMINMAX
#define FLUID_DEMO 1


#if FLUID_DEMO == 1
#define PI 3.1415926f
#define MAX_PARTICLES 200000;
#define MAX_CUBE_PARTICLES 50000;
#define MAX_DIFFUSE_PARTICLES 30000;

const bool _MPM_AL_ = false;
const bool _MPM2_AL_ = false;
const bool _MPM2_INCOMPRESSIBLE_ = false;
const bool _WCSPH_AL_ = false;
const bool _SPH_VULKAN_AL_ = false;

uint32_t AddCubeRigid(std::vector<ExtraParticle>& rigidParticles, Renderer* renderer, float diam);
uint32_t AddRingRigid(std::vector<ExtraParticle>& rigidParticles, Renderer* renderer, float diam, float innerRadius, float outerRadius);


int main(int argc, char** argv) {
	try {
		try {
			if (argc > 0 && argv != nullptr && argv[0] != nullptr) {
				std::filesystem::path exePath = std::filesystem::absolute(std::filesystem::path(argv[0]));
				if (exePath.has_parent_path()) {
					std::filesystem::current_path(exePath.parent_path());
				}
			}
		}
		catch (...) {
		}
		float radius = 0.008;
		float renderRadius = radius;
		if (_MPM_AL_ && _MPM2_AL_) {
			renderRadius = radius * 0.75f;
		}
		float restDesity = 1000.0f;
		float diam = 2 * radius;

		float rigid_radius = 0.01;
		float rigid_diam = 2 * rigid_radius;

		glm::vec3 camera_pos = glm::vec3(0.8f, 0.7f, 1.7f);
		glm::vec3 camera_view = glm::vec3(0.8f, 0.3f, 0.3f);
		glm::vec3 camera_up = glm::vec3(0, 1, 0);

		Renderer renderer = Renderer(1200, 900, true);

		UniformRenderingObject renderingobj{};
		renderingobj.camera_pos = camera_pos;
		renderingobj.camera_view = camera_view;
		renderingobj.camera_up = camera_up;
		renderingobj.model = glm::mat4(1.0f);
		renderingobj.view = glm::lookAt(camera_pos, camera_view, camera_up);
		renderingobj.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		renderingobj.projection[1][1] *= -1;
		renderingobj.inv_projection = glm::inverse(renderingobj.projection);
		renderingobj.inv_view = glm::inverse(renderingobj.view);  // 计算视图矩阵的逆，用于Cubemap反射
		renderingobj.zNear = 0.1f;
		renderingobj.zFar = 10.0f;
		renderingobj.aspect = 1;
		renderingobj.fovy = glm::radians(90.0f);
		renderingobj.particleRadius = renderRadius;
		renderingobj.rigidRadius = rigid_radius;
		renderingobj.renderType = 0;
		renderingobj.fluidType = 0;

		renderingobj.lights[0].position = glm::vec3(2.0f, 3.0f, 2.0f);
		renderingobj.lights[0].color = glm::vec3(1.0f, 1.0f, 1.0f);
		renderingobj.lights[0].intensity = 1.0f;
		renderingobj.lights[0].type = 0;      
		renderingobj.lights[0].enabled = 1;   
		
		renderingobj.lights[1].position = glm::vec3(-2.0f, 2.0f, 1.0f);
		renderingobj.lights[1].color = glm::vec3(0.8f, 0.9f, 1.0f);
		renderingobj.lights[1].intensity = 0.5f;
		renderingobj.lights[1].type = 0;
		renderingobj.lights[1].enabled = 0;
		
		renderingobj.lights[2].position = glm::vec3(0.0f, 1.0f, -2.0f);
		renderingobj.lights[2].color = glm::vec3(1.0f, 0.9f, 0.8f);
		renderingobj.lights[2].intensity = 0.3f;
		renderingobj.lights[2].type = 0;      
		renderingobj.lights[2].enabled = 0;   
		renderingobj.numActiveLights = 1;     
		
		renderingobj.ambientColor = glm::vec3(0.15f, 0.15f, 0.15f);
		renderingobj.causticsIntensity = 0.15f;
		renderingobj.causticsPhotonRadius = 15.0f;
		renderingobj.refractionIOR = 1.33f;
		renderingobj.redIOR = 1.331f;
		renderingobj.greenIOR = 1.333f;
		renderingobj.blueIOR = 1.336f;
		renderingobj.isFilter = true;
		renderingobj.filterRadius = (_MPM_AL_ && _MPM2_AL_) ? 16 : 8;

		renderingobj.fluidType = 6;
		renderer.SetRenderingObj(renderingobj);

		UniformSimulatingObject simulatingobj{};
		simulatingobj.dt = 1 / 240.0f;
		const bool useSphFamily = (_WCSPH_AL_ || _SPH_VULKAN_AL_);
		simulatingobj.restDensity = useSphFamily ? restDesity : (1.0f / (diam * diam * diam));
		simulatingobj.sphRadius = useSphFamily ? (_SPH_VULKAN_AL_ ? (0.01f * 2.0f) : (4.0f * radius)) : (4.5f * radius);

		simulatingobj.coffPoly6 = 315.0f / (64 * PI * pow(simulatingobj.sphRadius, 3));
		simulatingobj.coffGradSpiky = -45 / (PI * pow(simulatingobj.sphRadius, 4));
		simulatingobj.coffSpiky = 15 / (PI * pow(simulatingobj.sphRadius, 3));

		simulatingobj.scorrK = 0.0001;
		simulatingobj.scorrQ = 0.1;
		simulatingobj.scorrN = 4;

		float spaceSize = 2.2;
		simulatingobj.spaceSize = spaceSize;
		simulatingobj.fluidParticleRadius = radius;
		simulatingobj.cubeParticleRadius = radius;
		simulatingobj.rigidParticleRadius = rigid_radius;
		float grid_size = 0.01 * 2;
		int gridCount_1 = int(spaceSize / grid_size) + 8;
		simulatingobj.gridCount = gridCount_1 * gridCount_1 * gridCount_1;

		simulatingobj.testData1 = MAX_PARTICLES;
		simulatingobj.maxParticles = MAX_PARTICLES;
		simulatingobj.maxCubeParticles = MAX_CUBE_PARTICLES;
		simulatingobj.maxNgbrs = 256;
		simulatingobj.visc = useSphFamily ? 0.0f : 0.1f;
		simulatingobj.psi = 1.2;
		simulatingobj.relaxation = 1.0;
		
		// Artificial Viscosity Parameters
		simulatingobj.enableArtificialViscosity = 1;      // 默认启用 (0=off, 1=on)
		simulatingobj.artificialViscosityAlpha = 0.01f;   // 推荐值 0.01
		simulatingobj.artificialViscosityBeta = 0.0f;     // 通常设为 0
		
		// Surface Tension Parameters
		simulatingobj.enableSurfaceTension = 0;           // 默认禁用 (0=off, 1=on)
		simulatingobj.surfaceTensionCoefficient = 0.01f;  // 表面张力系数
		simulatingobj.surfaceTensionThreshold = 0.5f;     // 表面检测阈值
		
		// Anisotropic Kernel Parameters (Yu & Turk 2013)
		simulatingobj.anisotropicBaseRadiusScale = 1.5f;  // 基础半径缩放
		simulatingobj.anisotropicMaxRatio = 5.0f;         // 最大各向异性比
		simulatingobj.anisotropicMinScale = 0.2f;         // 最小半径缩放
		simulatingobj.anisotropicMaxScale = 1.0f;         // 最大半径缩放
		simulatingobj.anisotropicNeighborThreshold = 6.0f; // 邻居数阈值
		
		simulatingobj.attract = 0;
		simulatingobj.mouse_pos = glm::vec3(0.0, 0.0, 0.0);
		simulatingobj.k = 10.0;
		simulatingobj.boundary_min = glm::vec3(-0.1, -0.1, -0.1);
		simulatingobj.isSDF = false;

		// Diffuse about
		simulatingobj.maxDiffuseParticles = MAX_DIFFUSE_PARTICLES;
		simulatingobj.lifeTime = 2.0;
		simulatingobj.surfaceThreshold = 0.12;
		simulatingobj.foamThreshold = 1.0;

		// Wave Simulation - 已禁用
		simulatingobj.waveEnabled = 1;
		simulatingobj.waveIntensity = 1.0f;
		simulatingobj.waveSteepness = 0.7f;
		simulatingobj.waveBaseHeight = 1.35f;
		simulatingobj.windDirX = 1.0f;
		simulatingobj.windDirZ = 0.0f;
		simulatingobj.windStrength = 0.3f;
		simulatingobj.wavePeriod = 2.0f;

		renderer.SetCells(simulatingobj.gridCount);

		UniformBoxInfoObject boxinfoobj{};
		// fluid range
		boxinfoobj.clampX = glm::vec2{ 0.0,  2.0 };
		boxinfoobj.clampY = glm::vec2{ 0.0,  1.8 };
		//boxinfoobj.clampZ = glm::vec2{ -1.0,  0.6 };
		boxinfoobj.clampZ = glm::vec2{ 0.0,  1.2 };
		// box range
		boxinfoobj.clampX_still = glm::vec2{ 0.0,   1.2 };
		boxinfoobj.clampY_still = glm::vec2{ 0.0,   1.2 };
		boxinfoobj.clampZ_still = glm::vec2{ 0.0,  1.2 };
		renderer.SetBoxinfoObj(boxinfoobj);

		std::vector<Particle> particles;
		float particleMass = useSphFamily ? (restDesity * diam * diam * diam) : 1.0f;
		if (_SPH_VULKAN_AL_) {
			float h = simulatingobj.sphRadius;
			float w0 = 315.0f / (64.0f * PI * h * h * h);
			particleMass = restDesity / w0;
		}
		// PBF
		if (!_MPM_AL_) {
			for (float x = 0.35; x <= 0.95; x += diam) {
				for (float z = 0.35; z <= 0.65; z += diam) {
					for (float y = 0.65; y <= 1.05; y += diam) {
						Particle particle{};
						particle.Location = glm::vec3(x, y, z);

						particle.Mass = particleMass;
						particle.NumNgbrs = 0;
						particles.push_back(particle);

					}
				}
			}
		}
		else {
			// MPM
			for (float x = 0.35; x <= 0.95; x += diam) {
				for (float z = 0.35; z <= 0.65; z += diam) {
					for (float y = 0.75; y <= 1.35; y += diam) {
						Particle particle{};
						particle.Location = glm::vec3(x, y, z);

						particle.Mass = 1;
						particle.NumNgbrs = 0;
						particles.push_back(particle);

					}
				}
			}
		}
		renderer.SetValues(_MPM_AL_);
		renderer.SetMPM2(_MPM2_AL_);
		renderer.SetMPM2Incompressible(_MPM2_INCOMPRESSIBLE_);
		renderer.SetWCSPH(_WCSPH_AL_);
		renderer.SetSPHVulkan(_SPH_VULKAN_AL_);

		std::vector<ExtraParticle> rigid_particles;
		uint32_t rigidCount = 0;
		rigidCount += AddCubeRigid(rigid_particles, &renderer, rigid_diam);
		float innerRadius = 0.1f, outerRadius = 0.2f;  

		simulatingobj.rigidParticles = rigidCount;

		renderer.SetParticles(particles);
		renderer.SetRigidParticles(rigid_particles);
		renderer.SetSimulatingObj(simulatingobj);

		float accumulated_time = 0.0f;
		renderer.Init();
		auto now = std::chrono::high_resolution_clock::now();
		for (;;) {
			auto last = now;
			now = std::chrono::high_resolution_clock::now();
			float deltatime = std::chrono::duration<float, std::chrono::seconds::period>(now - last).count();

			// float dt = std::clamp(deltatime, 1 / 480.0f, 1 / 120.0f);
			float dt = _SPH_VULKAN_AL_ ? 0.001f : (useSphFamily ? 0.003f : (1 / 240.f));
			accumulated_time += dt;

			simulatingobj.dt = dt;
			simulatingobj.accumulated_t = accumulated_time;  // 必须更新时间给波浪计算用
			renderer.SetSimulatingObj(simulatingobj);

			// boxinfoobj.clampX.y = 1.5 + 0.25 * (1 - glm::cos(5 * accumulated_time));
			renderer.SetBoxinfoObj(boxinfoobj);

			renderer.Simulate();

			auto result = renderer.TickWindow(deltatime);

			if (result == TickWindowResult::EXIT) {
				break;
			}
			if (result != TickWindowResult::HIDE) {
				renderer.Draw();
			}

			renderer.SetFPS(1 / deltatime);

			renderer.UpdateUniformSimulatingBuffer();
		}

		renderer.Cleanup();
	}
	catch (std::runtime_error err) {
		std::cerr << err.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

uint32_t AddCubeRigid(std::vector<ExtraParticle>& rigidParticles, Renderer* renderer, float diam)
{
	ShapeMatchingInfo smi{};
	for (float x = 1.25; x <= 1.40; x += diam) {
		for (float z = 0.25; z <= 0.40; z += diam) {
			for (float y = 0.05; y <= 0.20; y += diam) {
				ExtraParticle rigid_particle{};
				rigid_particle.Location = glm::vec3(x, y, z);
				smi.CM0.x += x;
				smi.CM0.y += y;
				smi.CM0.z += z;
				rigid_particle.Mass = 1;
				rigid_particle.NumNgbrs = 0;
				rigidParticles.push_back(rigid_particle);
			}
		}
	}
	// simulatingobj.rigidParticles = rigidParticles.size();
	uint32_t rigidParticleCount = rigidParticles.size();
	smi.CM0.x /= rigidParticleCount;
	smi.CM0.y /= rigidParticleCount;
	smi.CM0.z /= rigidParticleCount;
	smi.CM = smi.CM0;
	renderer->SetShapeMatching(smi);
	return rigidParticleCount;
}

uint32_t AddRingRigid(std::vector<ExtraParticle>& rigidParticles, Renderer* renderer, float diam, float innerRadius, float outerRadius)
{
	ShapeMatchingInfo smi{};
	uint32_t count = 0;

	float height = diam * 3;
	glm::vec3 center(1.25f, 0.5f, 0.5f);
	
	for (float theta = 0; theta < 2 * PI; theta += diam / outerRadius) {
		for (float r = innerRadius; r <= outerRadius; r += diam) {
			for (float y = 0; y < height; y += diam) {
				float x = center.x + r * cos(theta);
				float z = center.z + r * sin(theta);
				float yPos = center.y + y;
				
				if (r >= innerRadius && r <= outerRadius) {
					ExtraParticle rigid_particle{};
					rigid_particle.Location = glm::vec3(x, yPos, z);
					smi.CM0.x += x;
					smi.CM0.y += yPos;
					smi.CM0.z += z;
					rigid_particle.Mass = 1;
					rigid_particle.NumNgbrs = 0;
					rigidParticles.push_back(rigid_particle);
					count++;
				}
			}
		}
	}
	
	if (count > 0) {
		smi.CM0.x /= count;
		smi.CM0.y /= count;
		smi.CM0.z /= count;
		smi.CM = smi.CM0;
		renderer->SetShapeMatching(smi);
	}
	return count;
}




#else
int main(int argc, char** argv) {

	std::cout << "other demos\n";
}

#endif


