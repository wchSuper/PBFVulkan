#pragma once
#include <vector>
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
#include "tiny_gltf.h"
#include <fstream>
#include <sstream>
#include <string>
#define Allocator nullptr

#define M_PI 3.1415926


namespace Scene
{
	inline glm::mat3 getRotationMatrix(float angle_x, float angle_y, float angle_z) {
		glm::mat4 I(1.0f);
		glm::mat4 rot_x = glm::rotate(I, angle_x, glm::vec3(1, 0, 0));
		glm::mat4 rot_y = glm::rotate(I, angle_y, glm::vec3(0, 1, 0));
		glm::mat4 rot_z = glm::rotate(I, angle_z, glm::vec3(0, 0, 1));
		glm::mat4 rot = rot_z * rot_y * rot_x; 
		return glm::mat3(rot);
	}

	inline void CreateObjModelResources(std::string& obj_path, ObjResource& objRes, 
		glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation)
	{
		std::string obj_file_path = obj_path;
		bool ret = tinyobj::LoadObj(&objRes.attrib, &objRes.shapes, &objRes.materials, &objRes.warn, &objRes.err, obj_file_path.c_str());
		if (!ret) {
			std::cerr << "Failed to load OBJ: " << objRes.err << std::endl;
			return;
		}

		// handle .obj model vertex and index data 
		for (const auto& shape : objRes.shapes) {
			for (size_t i = 0; i < shape.mesh.indices.size(); i++) {
				tinyobj::index_t idx = shape.mesh.indices[i];

				ObjVertex vertex{};
				// first to apply translation and scale
				glm::vec3 pos = glm::vec3(
					(objRes.attrib.vertices[3 * idx.vertex_index + 0]) * scale_rate[0] + translation[0],
					(objRes.attrib.vertices[3 * idx.vertex_index + 1]) * scale_rate[1] + translation[1],
					(objRes.attrib.vertices[3 * idx.vertex_index + 2]) * scale_rate[2] + translation[2]
				);
				// second to apply rotation
				pos = rotation * pos;
				vertex.pos[0] = pos.x;
				vertex.pos[1] = pos.y;
				vertex.pos[2] = pos.z;

				if (idx.normal_index >= 0) {
					vertex.normal[0] = objRes.attrib.normals[3 * idx.normal_index + 0];
					vertex.normal[1] = objRes.attrib.normals[3 * idx.normal_index + 1];
					vertex.normal[2] = objRes.attrib.normals[3 * idx.normal_index + 2];
				}

				if (idx.texcoord_index >= 0) {
					vertex.texCoord[0] = objRes.attrib.texcoords[2 * idx.texcoord_index + 0];
					vertex.texCoord[1] = objRes.attrib.texcoords[2 * idx.texcoord_index + 1];
					vertex.texCoord[1] = 1.0f - vertex.texCoord[1];
				}
				else {
					vertex.texCoord[0] = 0.0f;
					vertex.texCoord[1] = 0.0f;
				}
				objRes.vertices.push_back(vertex);
				objRes.indices.push_back(static_cast<uint32_t>(i));
			}
		}
	}


	inline void CreateObjModelResources_PolygonSupport(
		std::string& obj_path, ObjResource& objRes,
		glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation)
	{
		std::string obj_file_path = obj_path;
		bool ret = tinyobj::LoadObj(&objRes.attrib, &objRes.shapes, &objRes.materials, &objRes.warn, &objRes.err, obj_file_path.c_str());
		if (!ret) {
			std::cerr << "Failed to load OBJ: " << objRes.err << std::endl;
			return;
		}

		for (const auto& shape : objRes.shapes) {
			size_t index_offset = 0;
			for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
				int fv = shape.mesh.num_face_vertices[f];     // vertex num in the face
				// polygon to triangle : v0,v1,v2,v3,v4... => (v0,v1,v2), (v0,v2,v3), (v0,v3,v4)...
				for (int i = 1; i < fv - 1; i++) {
					std::array<int, 3> idxs = { 0, i, i + 1 };
					for (int k = 0; k < 3; k++) {
						tinyobj::index_t idx = shape.mesh.indices[index_offset + idxs[k]];
						ObjVertex vertex{};
						glm::vec3 pos = glm::vec3(
							(objRes.attrib.vertices[3 * idx.vertex_index + 0]),
							(objRes.attrib.vertices[3 * idx.vertex_index + 1]),
							(objRes.attrib.vertices[3 * idx.vertex_index + 2])
						);
						pos = rotation * pos;
						pos[0] = pos[0] * scale_rate[0] + translation[0], pos[1] = pos[1] * scale_rate[1] + translation[1], pos[2] = pos[2] * scale_rate[2] + translation[2];
						vertex.pos[0] = pos.x;
						vertex.pos[1] = pos.y;
						vertex.pos[2] = pos.z;
						if (idx.normal_index >= 0) {
							vertex.normal[0] = objRes.attrib.normals[3 * idx.normal_index + 0];
							vertex.normal[1] = objRes.attrib.normals[3 * idx.normal_index + 1];
							vertex.normal[2] = objRes.attrib.normals[3 * idx.normal_index + 2];
						}
						if (idx.texcoord_index >= 0) {
							vertex.texCoord[0] = objRes.attrib.texcoords[2 * idx.texcoord_index + 0];
							vertex.texCoord[1] = 1.0f - objRes.attrib.texcoords[2 * idx.texcoord_index + 1];
						}
						else {
							vertex.texCoord[0] = 0.0f;
							vertex.texCoord[1] = 0.0f;
						}
						objRes.vertices.push_back(vertex);
						objRes.indices.push_back(static_cast<uint32_t>(objRes.vertices.size() - 1));
					}
				}
				index_offset += fv;
			}
		}
	}

	// Forward declaration for the recursive node loading function
	static void loadNode(
		ObjResource& objRes,
		const tinygltf::Model& model,
		const tinygltf::Node& node,
		const glm::mat4& parentTransform
	);

	// Forward declaration of texture loading functionality
	static void loadTexture(
		ObjResource& objRes, 
		const tinygltf::Model& model, 
		int textureIndex,
		VkDevice device,
		VkPhysicalDevice physicalDevice,
		VkCommandPool commandPool,
		VkQueue graphicsQueue
	);

	inline void LoadGLBModel(
		std::string& glb_path, ObjResource& objRes,
		glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation,
		VkDevice device = VK_NULL_HANDLE,
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE,
		VkCommandPool commandPool = VK_NULL_HANDLE,
		VkQueue graphicsQueue = VK_NULL_HANDLE)
	{
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		bool ret;
		if (glb_path.ends_with(".glb")) {
			ret = loader.LoadBinaryFromFile(&model, &err, &warn, glb_path);
		} else if (glb_path.ends_with(".gltf")) {
			ret = loader.LoadASCIIFromFile(&model, &err, &warn, glb_path);
		} else {
			throw std::runtime_error("Unsupported model format: " + glb_path);
		}
		
		if (!warn.empty()) {
			std::cout << "GLB Import WARN: " << warn << std::endl;
		}
		if (!err.empty()) {
			std::cerr << "GLB Import ERR: " << err << std::endl;
		}

		objRes.vertices.clear();
		objRes.indices.clear();

		// The user-provided transform is the root transform for the entire model.
		// Order: T * R * S
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale_rate);
		glm::mat4 rotMat = glm::mat4(rotation);
		glm::mat4 transMat = glm::translate(glm::mat4(1.0f), translation);
		glm::mat4 rootTransform = transMat * rotMat * scaleMat;

		// Check if we have textures in the model and a valid device
		if (device != VK_NULL_HANDLE && physicalDevice != VK_NULL_HANDLE && 
			commandPool != VK_NULL_HANDLE && graphicsQueue != VK_NULL_HANDLE &&
			!model.textures.empty() && !model.images.empty()) {
			
			// Find the base color texture if it exists
			for (const auto& material : model.materials) {
				if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
					int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
					std::cout << "Loading texture from GLB file..." << std::endl;
					
					// Load the texture
					loadTexture(objRes, model, textureIndex, device, physicalDevice, commandPool, graphicsQueue);
					break; // For now, just load the first texture found
				}
			}
		}

		const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node& node = model.nodes[scene.nodes[i]];
			loadNode(objRes, model, node, rootTransform);
		}
	}
	
	static void loadNode(
		ObjResource& objRes,
		const tinygltf::Model& model,
		const tinygltf::Node& node,
		const glm::mat4& parentTransform
	) {
		glm::mat4 nodeTransform = glm::mat4(1.0f);
		
		// Get the node's local transformation matrix
		if (node.matrix.size() == 16) {
			nodeTransform = glm::make_mat4(node.matrix.data());
		} else {
			if (node.translation.size() == 3) {
				nodeTransform = glm::translate(nodeTransform, glm::vec3(glm::make_vec3(node.translation.data())));
			}
			if (node.rotation.size() == 4) {
				glm::quat q = glm::make_quat(node.rotation.data());
				nodeTransform *= glm::mat4_cast(q);
			}
			if (node.scale.size() == 3) {
				nodeTransform = glm::scale(nodeTransform, glm::vec3(glm::make_vec3(node.scale.data())));
			}
		}

		glm::mat4 globalTransform = parentTransform * nodeTransform;

		// Load mesh data if the node has a mesh
		if (node.mesh > -1) {
			const tinygltf::Mesh& mesh = model.meshes[node.mesh];
			glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(globalTransform)));

			for (const auto& primitive : mesh.primitives) {
				uint32_t firstVertex = static_cast<uint32_t>(objRes.vertices.size());

				// Vertex data
				const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
				const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
				const unsigned char* posData = model.buffers[posView.buffer].data.data() + posView.byteOffset + posAccessor.byteOffset;
				int posByteStride = posAccessor.ByteStride(posView) ? posAccessor.ByteStride(posView) : sizeof(glm::vec3);
				size_t vertexCount = posAccessor.count;

				const unsigned char* normalData = nullptr;
				int normalByteStride = 0;
				if (primitive.attributes.count("NORMAL")) {
					const auto& normalAccessor = model.accessors.at(primitive.attributes.at("NORMAL"));
					const auto& normalView = model.bufferViews.at(normalAccessor.bufferView);
					normalData = model.buffers[normalView.buffer].data.data() + normalView.byteOffset + normalAccessor.byteOffset;
					normalByteStride = normalAccessor.ByteStride(normalView) ? normalAccessor.ByteStride(normalView) : sizeof(glm::vec3);
				}

				const unsigned char* texcoordData = nullptr;
				int texcoordByteStride = 0;
				if (primitive.attributes.count("TEXCOORD_0")) {
					const auto& texcoordAccessor = model.accessors.at(primitive.attributes.at("TEXCOORD_0"));
					const auto& texcoordView = model.bufferViews.at(texcoordAccessor.bufferView);
					texcoordData = model.buffers[texcoordView.buffer].data.data() + texcoordView.byteOffset + texcoordAccessor.byteOffset;
					texcoordByteStride = texcoordAccessor.ByteStride(texcoordView) ? texcoordAccessor.ByteStride(texcoordView) : sizeof(glm::vec2);
				}
				
				// Vertex Color Attribute
				const unsigned char* colorData = nullptr;
				int colorByteStride = 0, colorComponentType = 0, colorType = 0;
				if (primitive.attributes.count("COLOR_0")) {
					const auto& colorAccessor = model.accessors.at(primitive.attributes.at("COLOR_0"));
					const auto& colorView = model.bufferViews.at(colorAccessor.bufferView);
					colorData = model.buffers[colorView.buffer].data.data() + colorView.byteOffset + colorAccessor.byteOffset;
					colorByteStride = colorAccessor.ByteStride(colorView) ? colorAccessor.ByteStride(colorView) : (colorAccessor.type == TINYGLTF_TYPE_VEC3 ? sizeof(float)*3: sizeof(float)*4);
					colorComponentType = colorAccessor.componentType;
					colorType = colorAccessor.type;
				}

				// Material Color
				glm::vec4 materialColor(1.0f); // Default to white
				if (primitive.material > -1) {
					const auto& mat = model.materials[primitive.material];
					if (mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
						materialColor = glm::make_vec4(mat.pbrMetallicRoughness.baseColorFactor.data());
					}
				}

				// Create vertices
				for (size_t i = 0; i < vertexCount; ++i) {
					ObjVertex vert{};
					
					glm::vec3 pos = glm::make_vec3(reinterpret_cast<const float*>(posData + i * posByteStride));
					vert.pos[0] = (globalTransform * glm::vec4(pos, 1.0f)).x;
					vert.pos[1] = (globalTransform * glm::vec4(pos, 1.0f)).y;
					vert.pos[2] = (globalTransform * glm::vec4(pos, 1.0f)).z;

					if (normalData) {
						glm::vec3 norm = glm::make_vec3(reinterpret_cast<const float*>(normalData + i * normalByteStride));
						norm = normalMatrix * norm;
						vert.normal[0] = norm.x; vert.normal[1] = norm.y; vert.normal[2] = norm.z;
					}

					if (texcoordData) {
						glm::vec2 tex = glm::make_vec2(reinterpret_cast<const float*>(texcoordData + i * texcoordByteStride));
						vert.texCoord[0] = tex.x; vert.texCoord[1] = tex.y;  // vert.texCoord[1] = 1.0 - tex.y;
					}
					
					// Assign color: vertex attribute > material color > default white
					if (colorData) { // Priority 1: Vertex color attribute
						if (colorType == TINYGLTF_TYPE_VEC4) {
							if (colorComponentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
								glm::vec4 c = glm::make_vec4(reinterpret_cast<const float*>(colorData + i * colorByteStride));
								vert.color[0] = c.r; vert.color[1] = c.g; vert.color[2] = c.b; vert.color[3] = c.a;
							} else if (colorComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
								const uint8_t* c = reinterpret_cast<const uint8_t*>(colorData + i * colorByteStride);
								vert.color[0] = c[0] / 255.0f; vert.color[1] = c[1] / 255.0f; vert.color[2] = c[2] / 255.0f; vert.color[3] = c[3] / 255.0f;
							}
						}
					} else { // Priority 2: Material color
						vert.color[0] = materialColor.r;
						vert.color[1] = materialColor.g;
						vert.color[2] = materialColor.b;
						vert.color[3] = materialColor.a;
					}
					objRes.vertices.push_back(vert);
				}

				// Indices
				const auto& indexAccessor = model.accessors[primitive.indices];
				const auto& indexView = model.bufferViews[indexAccessor.bufferView];
				const unsigned char* indexData = model.buffers[indexView.buffer].data.data() + indexView.byteOffset + indexAccessor.byteOffset;
				
				for (size_t i = 0; i < indexAccessor.count; ++i) {
					uint32_t index = 0;
					switch (indexAccessor.componentType) {
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
							index = reinterpret_cast<const uint32_t*>(indexData)[i]; break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							index = reinterpret_cast<const uint16_t*>(indexData)[i]; break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
							index = reinterpret_cast<const uint8_t*>(indexData)[i]; break;
						default: std::cerr << "Unsupported index type" << std::endl; break;
					}
					objRes.indices.push_back(index + firstVertex);
				}
			}
		}

		for (size_t i = 0; i < node.children.size(); i++) {
			loadNode(objRes, model, model.nodes[node.children[i]], globalTransform);
		}
	}

	// PLY file
	inline bool LoadPLYModel(
		std::string& ply_path, ObjResource& objRes,
		glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation)
	{
		std::ifstream file(ply_path, std::ios::binary);
		if (!file.is_open()) {
			std::cerr << "Failed to open PLY file: " << ply_path << std::endl;
			return false;
		}
		objRes.vertices.clear();
		objRes.indices.clear();
		// header
		std::string line;
		int vertex_count = 0;
		int face_count = 0;
		bool is_binary = false;
		bool header_end = false;

		// read header
		while (std::getline(file, line)) {
			std::istringstream iss(line);
			std::string keyword;
			iss >> keyword;

			if (keyword == "format") {
				std::string format;
				iss >> format;
				is_binary = (format == "binary_little_endian" || format == "binary_big_endian");
			}
			else if (keyword == "element") {
				std::string element_type;
				iss >> element_type;
				if (element_type == "vertex") {
					iss >> vertex_count;
				}
				else if (element_type == "face") {
					iss >> face_count;
				}
			}
			else if (keyword == "end_header") {
				header_end = true;
				break;
			}
		}

		if (!header_end) {
			std::cerr << "Invalid PLY file format: header not found" << std::endl;
			return false;
		}

		if (is_binary) {
			if (file.peek() == '\n' || file.peek() == '\r') {
				char c;
				file.read(&c, 1);
			}
			// 读取顶点数据 - 每个顶点: 3 float (x,y,z) + 4 uchar (r,g,b,a)
			for (int i = 0; i < vertex_count; i++) {
				float x, y, z;
				uint8_t r, g, b, a;
				file.read(reinterpret_cast<char*>(&x), sizeof(float));
				file.read(reinterpret_cast<char*>(&y), sizeof(float));
				file.read(reinterpret_cast<char*>(&z), sizeof(float));
				file.read(reinterpret_cast<char*>(&r), sizeof(uint8_t));
				file.read(reinterpret_cast<char*>(&g), sizeof(uint8_t));
				file.read(reinterpret_cast<char*>(&b), sizeof(uint8_t));
				file.read(reinterpret_cast<char*>(&a), sizeof(uint8_t));

				ObjVertex vertex{};

				glm::vec3 pos = glm::vec3(x, y, z);
				pos = rotation * (pos * scale_rate) + translation;

				vertex.pos[0] = pos.x;
				vertex.pos[1] = pos.y;
				vertex.pos[2] = pos.z;

				vertex.color[0] = r / 255.0f;
				vertex.color[1] = g / 255.0f;
				vertex.color[2] = b / 255.0f;
				vertex.color[3] = a / 255.0f;

				vertex.normal[0] = 0.0f;
				vertex.normal[1] = 1.0f;
				vertex.normal[2] = 0.0f;
				vertex.texCoord[0] = 0.0f;
				vertex.texCoord[1] = 0.0f;

				objRes.vertices.push_back(vertex);
			}

			// 读取面数据 - 每个面: 1 uchar (顶点数) + 3 int (索引)
			for (int i = 0; i < face_count; i++) {
				uint8_t vertex_count_in_face;
				int32_t idx1, idx2, idx3;

				file.read(reinterpret_cast<char*>(&vertex_count_in_face), sizeof(uint8_t));
				file.read(reinterpret_cast<char*>(&idx1), sizeof(int32_t));
				file.read(reinterpret_cast<char*>(&idx2), sizeof(int32_t));
				file.read(reinterpret_cast<char*>(&idx3), sizeof(int32_t));

				// 检查是否为三角形面
				if (vertex_count_in_face != 3) {
					std::cerr << "Warning: Found non-triangular face with " << static_cast<int>(vertex_count_in_face) << " vertices" << std::endl;
					// 跳过非三角形面的额外数据
					for (int j = 3; j < vertex_count_in_face; j++) {
						int32_t extra_idx;
						file.read(reinterpret_cast<char*>(&extra_idx), sizeof(int32_t));
					}
					continue;
				}
				objRes.indices.push_back(static_cast<uint32_t>(idx1));
				objRes.indices.push_back(static_cast<uint32_t>(idx2));
				objRes.indices.push_back(static_cast<uint32_t>(idx3));
			}
		}
		else {
			// ASCII PLY格式
			for (int i = 0; i < vertex_count; i++) {
				std::getline(file, line);
				std::istringstream iss(line);

				float x, y, z;
				int r, g, b, a;

				iss >> x >> y >> z >> r >> g >> b >> a;

				ObjVertex vertex{};

				glm::vec3 pos = glm::vec3(x, y, z);
				pos = rotation * (pos * scale_rate) + translation;

				vertex.pos[0] = pos.x;
				vertex.pos[1] = pos.y;
				vertex.pos[2] = pos.z;

				vertex.color[0] = r / 255.0f;
				vertex.color[1] = g / 255.0f;
				vertex.color[2] = b / 255.0f;
				vertex.color[3] = a / 255.0f;

				vertex.normal[0] = 0.0f;
				vertex.normal[1] = 1.0f;
				vertex.normal[2] = 0.0f;
				vertex.texCoord[0] = 0.0f;
				vertex.texCoord[1] = 0.0f;

				objRes.vertices.push_back(vertex);
			}

			for (int i = 0; i < face_count; i++) {
				std::getline(file, line);
				std::istringstream iss(line);

				int vertex_count_in_face;
				iss >> vertex_count_in_face;

				if (vertex_count_in_face == 3) {
					int idx1, idx2, idx3;
					iss >> idx1 >> idx2 >> idx3;

					objRes.indices.push_back(static_cast<uint32_t>(idx1));
					objRes.indices.push_back(static_cast<uint32_t>(idx2));
					objRes.indices.push_back(static_cast<uint32_t>(idx3));
				}
				else {
					std::cerr << "Warning: Found non-triangular face with " << vertex_count_in_face << " vertices" << std::endl;
					continue;
				}
			}
		}

		if (face_count == 0) {
			for (uint32_t i = 0; i < objRes.vertices.size(); i++) {
				objRes.indices.push_back(i);
			}
		}
		file.close();
		return true;
	}

	// new function to load a texture from GLB model
	static void loadTexture(
		ObjResource& objRes, 
		const tinygltf::Model& model, 
		int textureIndex,
		VkDevice device,
		VkPhysicalDevice physicalDevice,
		VkCommandPool commandPool,
		VkQueue graphicsQueue) 
	{
		// Get texture and image indices
		const tinygltf::Texture& texture = model.textures[textureIndex];
		if (texture.source < 0) {
			std::cerr << "Invalid texture source index" << std::endl;
			return;
		}

		// Get the image data
		const tinygltf::Image& image = model.images[texture.source];
		const unsigned char* pixels = image.image.data();
		VkDeviceSize imageSize = image.image.size();
		int width = static_cast<int>(image.width);
		int height = static_cast<int>(image.height);
		
		if (!pixels || imageSize == 0) {
			std::cerr << "No image data found in GLB file" << std::endl;
			return;
		}
		
		std::cout << "Loading GLB texture: " << width << "x" << height << ", size: " << imageSize << " bytes" << std::endl;
		
		// 正确传递参数：路径、目标资源、像素数据、尺寸、设备参数
		Image::CreateTextureResources("", 
			objRes.textureImage, objRes.textureImageView, objRes.textureImageMemory, objRes.textureSampler,
			pixels, imageSize, width, height,
			physicalDevice, device, commandPool, graphicsQueue);
	}

	// sphere create
	inline void CreateSphere(
		SphereResource &sphere, glm::vec3 translation, float radius = 1.2f, int sectors = 36, int stacks = 18
	)
	{
		//const int sectors = 36;
		//const int stacks = 18;
		//const float radius = 1.2f;
		float sectorStep = 2 * M_PI / sectors;
		float stackStep = M_PI / stacks;

		for (int i = 0; i <= stacks; ++i) {
			float stackAngle = M_PI / 2 - i * stackStep;
			float xy = radius * cosf(stackAngle);
			float z = radius * sinf(stackAngle);
			for (int j = 0; j <= sectors; ++j) {
				float sectorAngle = j * sectorStep;
				float x = xy * cosf(sectorAngle);
				float y = xy * sinf(sectorAngle);
				glm::vec3 pos(x + translation[0], y + translation[1], z + translation[2]);
				glm::vec3 norm = glm::normalize(pos);
				glm::vec2 tex(0.0, 0.0);
				sphere.vertices.push_back({ pos, norm, tex });
			}
		}

		for (int i = 0; i < stacks; ++i) {
			int k1 = i * (sectors + 1);
			int k2 = k1 + sectors + 1;
			for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
				if (i != 0) {
					sphere.indices.push_back(k1);
					sphere.indices.push_back(k2);
					sphere.indices.push_back(k1 + 1);
				}
				if (i != (stacks - 1)) {
					sphere.indices.push_back(k1 + 1);
					sphere.indices.push_back(k2);
					sphere.indices.push_back(k2 + 1);
				}
			}
		}
	}

	// Ellipsoid create - 椭球创建函数
	// radiusX, radiusY, radiusZ: 椭球在X、Y、Z轴的半径
	// center: 椭球的中心位置
	// sectors: 经度方向的分段数（越大越平滑）
	// stacks: 纬度方向的分段数（越大越平滑）
	inline void CreateEllipsoid(
		ObjResource& objRes,
		glm::vec3 radii,           // 三个半径 (radiusX, radiusY, radiusZ)
		glm::vec3 center,          // 中心位置
		int sectors = 36,          // 经度分段数
		int stacks = 18            // 纬度分段数
	)
	{
		objRes.vertices.clear();
		objRes.indices.clear();

		float sectorStep = 2.0f * M_PI / sectors;  // 经度步长
		float stackStep = M_PI / stacks;            // 纬度步长

		// 生成顶点
		for (int i = 0; i <= stacks; ++i) {
			float stackAngle = M_PI / 2.0f - i * stackStep;  // 从π/2到-π/2（从顶部到底部）
			float cosStack = cosf(stackAngle);
			float sinStack = sinf(stackAngle);

			for (int j = 0; j <= sectors; ++j) {
				float sectorAngle = j * sectorStep;  // 从0到2π
				float cosSector = cosf(sectorAngle);
				float sinSector = sinf(sectorAngle);

				// 椭球参数化公式
				// x = a * cos(stack) * cos(sector)
				// y = b * cos(stack) * sin(sector)
				// z = c * sin(stack)
				float x = radii.x * cosStack * cosSector;
				float y = radii.y * cosStack * sinSector;
				float z = radii.z * sinStack;

				ObjVertex vertex{};
				
				// 位置 = 椭球表面 + 中心偏移
				vertex.pos[0] = x + center.x;
				vertex.pos[1] = y + center.y;
				vertex.pos[2] = z + center.z;

				// 椭球的法线计算（非归一化）：(x/a², y/b², z/c²)
				// 这是椭球几何的标准法线公式
				glm::vec3 normal = glm::vec3(
					x / (radii.x * radii.x),
					y / (radii.y * radii.y),
					z / (radii.z * radii.z)
				);
				normal = glm::normalize(normal);
				vertex.normal[0] = normal.x;
				vertex.normal[1] = normal.y;
				vertex.normal[2] = normal.z;

				// 纹理坐标（球面映射）
				vertex.texCoord[0] = static_cast<float>(j) / sectors;
				vertex.texCoord[1] = static_cast<float>(i) / stacks;

				// 默认颜色（白色）
				vertex.color[0] = 1.0f;
				vertex.color[1] = 1.0f;
				vertex.color[2] = 1.0f;
				vertex.color[3] = 1.0f;

				objRes.vertices.push_back(vertex);
			}
		}

		// 生成索引（三角形）
		for (int i = 0; i < stacks; ++i) {
			int k1 = i * (sectors + 1);      // 当前层的起始索引
			int k2 = k1 + sectors + 1;       // 下一层的起始索引

			for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
				// 每个网格单元由两个三角形组成
				// 跳过顶部的退化三角形
				if (i != 0) {
					objRes.indices.push_back(k1);
					objRes.indices.push_back(k2);
					objRes.indices.push_back(k1 + 1);
				}
				// 跳过底部的退化三角形
				if (i != (stacks - 1)) {
					objRes.indices.push_back(k1 + 1);
					objRes.indices.push_back(k2);
					objRes.indices.push_back(k2 + 1);
				}
			}
		}
	}

	inline void CreatePlane(
		ObjResource& objRes, glm::vec3 translation, glm::vec3 scale_rate, glm::mat3 rotation
	)
	{
		objRes.vertices.clear();
		objRes.indices.clear();

		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale_rate);
		glm::mat4 rotMat = glm::mat4(rotation);
		glm::mat4 transMat = glm::translate(glm::mat4(1.0f), translation);
		glm::mat4 transform = transMat * rotMat * scaleMat;
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

		std::vector<glm::vec3> local_positions = {
			glm::vec3(-0.5f, 0.0f, -0.5f),
			glm::vec3( 0.5f, 0.0f, -0.5f),
			glm::vec3( 0.5f, 0.0f,  0.5f),
			glm::vec3(-0.5f, 0.0f,  0.5f)
		};

		glm::vec3 local_normal = glm::vec3(0.0f, 1.0f, 0.0f);

		std::vector<glm::vec2> local_texcoords = {
			glm::vec2(0.0f, 0.0f),
			glm::vec2(1.0f, 0.0f),
			glm::vec2(1.0f, 1.0f),
			glm::vec2(0.0f, 1.0f)
		};

		for (size_t i = 0; i < local_positions.size(); ++i) {
			ObjVertex vertex{};

			glm::vec3 local_pos = local_positions[i];
			glm::vec4 final_pos = transform * glm::vec4(local_pos, 1.0f);
			vertex.pos[0] = final_pos.x;
			vertex.pos[1] = final_pos.y;
			vertex.pos[2] = final_pos.z;

			glm::vec3 final_normal = normalMatrix * local_normal;
			vertex.normal[0] = final_normal.x;
			vertex.normal[1] = final_normal.y;
			vertex.normal[2] = final_normal.z;

			vertex.texCoord[0] = local_texcoords[i].x;
			vertex.texCoord[1] = local_texcoords[i].y;

			vertex.color[0] = 1.0f;
			vertex.color[1] = 1.0f;
			vertex.color[2] = 1.0f;
			vertex.color[3] = 1.0f;

			objRes.vertices.push_back(vertex);
		}

		objRes.indices = {0, 1, 2, 0, 2, 3};
	}

	inline void VoxelizeObj(VkPhysicalDevice PDevice, VkDevice LDevice, ObjResource& objRes, bool isSDF)
	{
		// because the useVoxBox is true, so voxel buffer is must be created for voxel computing;
		// Voxel GPU Buffer
		{
			VkDeviceSize bufferSize = sizeof(uint32_t) * VOXEL_RES * VOXEL_RES * VOXEL_RES;
			Buffer::CreateBuffer(PDevice, LDevice, objRes.voxelBuffer, objRes.voxelBufferMemory, bufferSize,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}
		// Voxel Uniform Buffer
		{
			VkDeviceSize bufferSize = sizeof(UniformVoxelObject);
			Buffer::CreateBuffer(PDevice, LDevice, objRes.UniformBuffer, objRes.UniformBufferMemory, bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkMapMemory(LDevice, objRes.UniformBufferMemory, 0, bufferSize, 0, &objRes.MappedBuffer);
			memcpy(objRes.MappedBuffer, &objRes.voxinfobj, bufferSize);
		}
		// sdf start
		if (isSDF) {
			// ...
		}
	}

	inline void VoxelWithCubeParticle
	(
		VkPhysicalDevice PDevice, VkDevice LDevice, VkCommandPool CommandPool, VkQueue GraphicNComputeQueue,
		ObjResource& voxelObj, std::vector<CubeParticle>& cubeParticles, std::vector<uint32_t>& counts, 
		bool useParticles, bool outParam 
	)
	{
		if (!outParam) {
			std::vector<uint32_t> newCounts = 
				Buffer::RetrieveVoxelDataFromGPU(PDevice, LDevice, CommandPool, GraphicNComputeQueue, voxelObj);
			for (uint32_t i = 0; i < newCounts.size(); i++) {
				if (newCounts[i] > 0) {
					uint32_t res_x = voxelObj.voxinfobj.resolution.x;  // = VOXEL_RES
					uint32_t res_z = voxelObj.voxinfobj.resolution.z;  // = VOXEL_RES

					uint32_t iy = i / (res_z * res_x);
					uint32_t remainder = i % (res_z * res_x);
					uint32_t iz = remainder / res_x;
					uint32_t ix = remainder % res_x;

					CubeParticle cp{};
					cp.Location.x = voxelObj.voxinfobj.boxMin.x + (static_cast<float>(ix) + 0.5f) * voxelObj.voxinfobj.voxelSize;
					cp.Location.y = voxelObj.voxinfobj.boxMin.y + (static_cast<float>(iy) + 0.5f) * voxelObj.voxinfobj.voxelSize;
					cp.Location.z = voxelObj.voxinfobj.boxMin.z + (static_cast<float>(iz) + 0.5f) * voxelObj.voxinfobj.voxelSize;
					cubeParticles.push_back(cp);
				}
			}
			return;
		}
		counts = Buffer::RetrieveVoxelDataFromGPU(PDevice, LDevice, CommandPool, GraphicNComputeQueue, voxelObj);
		if (!useParticles) return;
		//for (uint32_t i = 0; i < counts.size(); i++) {
		//	if (counts[i] > 0) {
		//		uint32_t res_x = voxelObj.voxinfobj.resolution.x;  // = VOXEL_RES
		//		uint32_t res_z = voxelObj.voxinfobj.resolution.z;  // = VOXEL_RES

		//		uint32_t iy = i / (res_z * res_x);
		//		uint32_t remainder = i % (res_z * res_x);
		//		uint32_t iz = remainder / res_x;
		//		uint32_t ix = remainder % res_x;

		//		CubeParticle cp{};
		//		cp.Location.x = voxelObj.voxinfobj.boxMin.x + (static_cast<float>(ix) + 0.5f) * voxelObj.voxinfobj.voxelSize;
		//		cp.Location.y = voxelObj.voxinfobj.boxMin.y + (static_cast<float>(iy) + 0.5f) * voxelObj.voxinfobj.voxelSize;
		//		cp.Location.z = voxelObj.voxinfobj.boxMin.z + (static_cast<float>(iz) + 0.5f) * voxelObj.voxinfobj.voxelSize;
		//		cubeParticles.push_back(cp);
		//	}
		//}
	}

	inline void CleanupObj(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		ObjResource& objRes, bool useVox, bool useTex, bool use2Tex, bool usePBR, bool useTransform)
	{
		if (objRes.graphicPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(LDevice, objRes.graphicPipeline, Allocator);
			objRes.graphicPipeline = VK_NULL_HANDLE;
		}
		if (objRes.pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(LDevice, objRes.pipelineLayout, Allocator);
			objRes.pipelineLayout = VK_NULL_HANDLE;
		}

		if (objRes.descriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(LDevice, objRes.descriptorSetLayout, Allocator);
			objRes.descriptorSetLayout = VK_NULL_HANDLE;
		}

		Buffer::CleanupBuffer(LDevice, objRes.vertexBuffer, objRes.vertexBufferMemory, false);
		Buffer::CleanupBuffer(LDevice, objRes.indexBuffer, objRes.indexBufferMemory, false);
		
		if (useVox) {
			Buffer::CleanupBuffer(LDevice, objRes.voxelBuffer, objRes.voxelBufferMemory, false);
			Buffer::CleanupBuffer(LDevice, objRes.UniformBuffer, objRes.UniformBufferMemory, true);
		}

		if (useTransform) {
			Buffer::CleanupBuffer(LDevice, objRes.UniformBuffer_, objRes.UniformBufferMemory_, true);
		}

		if (usePBR) {
			int tsize = objRes.textureImagePBR.size();
			for (int i = 0; i < tsize; i++) {
				if (objRes.textureSamplerPBR[i] != VK_NULL_HANDLE) vkDestroySampler(LDevice, objRes.textureSamplerPBR[i], Allocator);
				if (objRes.textureImageViewPBR[i] != VK_NULL_HANDLE) vkDestroyImageView(LDevice, objRes.textureImageViewPBR[i], Allocator);
				if (objRes.textureImagePBR[i] != VK_NULL_HANDLE) vkDestroyImage(LDevice, objRes.textureImagePBR[i], Allocator);
				if (objRes.textureImageMemoryPBR[i] != VK_NULL_HANDLE) vkFreeMemory(LDevice, objRes.textureImageMemoryPBR[i], Allocator);
			}
		}

		if (useTex) {
			if (objRes.textureSampler != VK_NULL_HANDLE) vkDestroySampler  (LDevice, objRes.textureSampler, Allocator);
			if (objRes.textureImageView != VK_NULL_HANDLE) vkDestroyImageView(LDevice, objRes.textureImageView, Allocator);
			if (objRes.textureImage != VK_NULL_HANDLE) vkDestroyImage    (LDevice, objRes.textureImage, Allocator);
			if (objRes.textureImageMemory != VK_NULL_HANDLE) vkFreeMemory      (LDevice, objRes.textureImageMemory, Allocator);
			if (use2Tex) {
				if (objRes.textureSampler2 != VK_NULL_HANDLE) vkDestroySampler(LDevice, objRes.textureSampler2, Allocator);
				if (objRes.textureImageView2 != VK_NULL_HANDLE) vkDestroyImageView(LDevice, objRes.textureImageView2, Allocator);
				if (objRes.textureImage2 != VK_NULL_HANDLE) vkDestroyImage(LDevice, objRes.textureImage2, Allocator);
				if (objRes.textureImageMemory2 != VK_NULL_HANDLE) vkFreeMemory(LDevice, objRes.textureImageMemory2, Allocator);
			}
		}
	}

	inline void CleanupSkybox(
		VkPhysicalDevice PDevice, VkDevice LDevice,
		SkyboxResource& skyboxRes)
	{
		if (skyboxRes.graphicPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(LDevice, skyboxRes.graphicPipeline, Allocator);
			skyboxRes.graphicPipeline = VK_NULL_HANDLE;
		}
		if (skyboxRes.pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(LDevice, skyboxRes.pipelineLayout, Allocator);
			skyboxRes.pipelineLayout = VK_NULL_HANDLE;
		}

		if (skyboxRes.descriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(LDevice, skyboxRes.descriptorSetLayout, Allocator);
			skyboxRes.descriptorSetLayout = VK_NULL_HANDLE;
		}

		if (skyboxRes.skyboxSampler != VK_NULL_HANDLE) vkDestroySampler(LDevice, skyboxRes.skyboxSampler, Allocator);
		if (skyboxRes.skyboxImageView != VK_NULL_HANDLE) vkDestroyImageView(LDevice, skyboxRes.skyboxImageView, Allocator);
		if (skyboxRes.skyboxImage != VK_NULL_HANDLE) vkDestroyImage(LDevice, skyboxRes.skyboxImage, Allocator);
		if (skyboxRes.skyboxImageMemory != VK_NULL_HANDLE) vkFreeMemory(LDevice, skyboxRes.skyboxImageMemory, Allocator);

		Buffer::CleanupBuffer(LDevice, skyboxRes.vertexBuffer, skyboxRes.vertexBufferMemory, false);
		Buffer::CleanupBuffer(LDevice, skyboxRes.indexBuffer, skyboxRes.indexBufferMemory, false);
	}
	
}
