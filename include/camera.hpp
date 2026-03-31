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


#define Allocator nullptr


static void print_camera_info(glm::vec3 cameraPos, glm::vec3 cameraTarget, glm::vec3 cameraUp)
{
	std::cout << "cameraPos: " << cameraPos[0] << "," << cameraPos[1] << "," << cameraPos[2] << "\n";
	std::cout << "cameraTarget: " << cameraTarget[0] << "," << cameraTarget[1] << "," << cameraTarget[2] << "\n";
	std::cout << "cameraUp: " << cameraUp[0] << "," << cameraUp[1] << "," << cameraUp[2] << "\n";
}

namespace Camera
{
	inline void PanCamera(float dx, float dy, float panSpeed,
		glm::vec3 &cameraPos, glm::vec3& cameraTarget, glm::vec3& cameraUp, 
		UniformRenderingObject & renderingobj)
	{
		glm::vec3 cameraFront = glm::normalize(cameraTarget - cameraPos);

		glm::vec3 right = glm::cross(cameraFront, cameraUp);

		glm::vec3 move = -right * dx * panSpeed + cameraUp * dy * panSpeed;
		cameraPos += move;
		cameraTarget += move;

		renderingobj.view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
		renderingobj.inv_view = glm::inverse(renderingobj.view);  // 更新inv_view用于Cubemap反射
	}


	inline void ZoomCamera(float delta, float zoomSpeed,
		glm::vec3& cameraPos, glm::vec3& cameraTarget, glm::vec3& cameraUp,
		UniformRenderingObject& renderingobj)
	{
		glm::vec3 direction = glm::normalize(cameraTarget - cameraPos);

		cameraPos += direction * delta * zoomSpeed;

		float distance = glm::length(cameraPos - cameraTarget);
		if (distance < 0.1f) {
			cameraPos = cameraTarget - direction * 0.1f;
		}
		renderingobj.view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
		renderingobj.inv_view = glm::inverse(renderingobj.view);  // 更新inv_view用于Cubemap反射
	}


	inline void RotateCamera(float dx, float dy, float rotateSpeed,
		glm::vec3& cameraPos, glm::vec3& cameraTarget, glm::vec3& cameraUp,
		UniformRenderingObject& renderingobj)
	{
		float yaw = dx * rotateSpeed;
		float pitch = dy * rotateSpeed;
		glm::vec3 direction = glm::normalize(cameraTarget - cameraPos);
		glm::mat4 yawMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
		direction = glm::vec3(yawMatrix * glm::vec4(direction, 0.0f));
		glm::vec3 right = glm::cross(direction, cameraUp);
		glm::mat4 pitchMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(pitch), right);
		direction = glm::vec3(pitchMatrix * glm::vec4(direction, 0.0f));

		cameraPos = cameraTarget - direction * glm::length(cameraTarget - cameraPos);

		renderingobj.view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
		renderingobj.inv_view = glm::inverse(renderingobj.view);  // 更新inv_view用于Cubemap反射
	}


}