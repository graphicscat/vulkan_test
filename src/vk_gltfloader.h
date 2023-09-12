#pragma once
#include <tiny_gltf.h>
#include <vk_types.h>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
class VulkanEngine;
class GLTFLoader
{
    public:
    struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
		glm::vec4 tangent;
	};

	struct Vertices
	{
		VkBuffer verticesBuffer;
		VkDeviceMemory verticesMemory;
	}vertices;

	struct {
		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	} indices;

	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	struct Mesh {
		std::vector<Primitive> primitives;
	};
	struct Node {
		Node* parent;
		std::vector<Node*> children;
		Mesh mesh;
		glm::mat4 matrix;
		bool hasAnimation = false;
		uint32_t            index;
		glm::vec3           translation{};
		glm::vec3           scale{ 1.0f };
		glm::quat           rotation{};
		glm::mat4 getLocalMatrix()
		{
			return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
			//return matrix;
		}
		glm::mat4 getAnimationMatrix()
		{
			return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
			//return matrix;
		}

		~Node() {
			for (auto& child : children) {
				delete child;
			}
		}
	};

	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
		uint32_t normalTextureIndex;
		uint32_t emissiveTextureIndex;
		uint32_t metallicRoughnessTextureIndex;
		VkDescriptorSet matDescriptorSet;
	};

	struct Image {
		AllocatedImage texture;
		// We also store (and create) a descriptor set that's used to access this texture from the fragment shader
		VkDescriptorSet descriptorSet;
	};

	struct Texture {
		int32_t imageIndex;
	};

	std::vector<Image> images;
	std::vector<Texture> textures;
	std::vector<Node*> nodes;
	std::vector<Material> materials;
   
	void loadNode(
        VulkanEngine& engine,
		const tinygltf::Node& inputNode,
		const tinygltf::Model& input,
		GLTFLoader::Node* parent, uint32_t nodeIndex,
		std::vector<uint32_t>& indexBuffer,
		std::vector<Vertex>& vertexBuffer
	);
	void loadImages(VulkanEngine& engine,tinygltf::Model& input);
	void loadTextures(VulkanEngine& engine,tinygltf::Model& input);
	void loadMaterials(VulkanEngine& engine,tinygltf::Model& input);
	void drawNode(VulkanEngine& engine,VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, GLTFLoader::Node* node);
	void draw(VulkanEngine& engine,VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
    void loadgltfFile(VulkanEngine& engine,std::string filename);
};