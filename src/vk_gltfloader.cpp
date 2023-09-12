#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <vk_gltfloader.h>
#include <vk_texture.h>
#include <vk_engine.h>

void GLTFLoader::loadNode(VulkanEngine& engine,const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, uint32_t nodeIndex, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer)
{
	Node* node = new Node{};
	node->matrix = glm::mat4(1.0f);
	node->index = nodeIndex;
	node->parent = parent;

	// Get the local node matrix
	// It's either made up from translation, rotation, scale or a 4x4 matrix
	if (inputNode.translation.size() == 3) {
		node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
	}
	if (inputNode.rotation.size() == 4) {
		glm::quat q = glm::make_quat(inputNode.rotation.data());
		node->matrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3) {
		node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
	}
	if (inputNode.matrix.size() == 16) {
		node->matrix = glm::make_mat4x4(inputNode.matrix.data());
	};

	// Load node's children
	if (inputNode.children.size() > 0) {
		for (size_t i = 0; i < inputNode.children.size(); i++) {
			loadNode(engine,input.nodes[inputNode.children[i]], input, node, inputNode.children[i], indexBuffer, vertexBuffer);
		}
	}

	// If the node contains mesh data, we load vertices and indices from the buffers
	// In glTF this is done via accessors and buffer views
	if (inputNode.mesh > -1) {
		const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
		// Iterate through all primitives of this node's mesh
		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				const float* tangentsBuffer = nullptr;
				size_t vertexCount = 0;

				// Get buffer data for vertex positions
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					tangentsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}


				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					vert.color = glm::vec3(1.0f);
					vert.tangent = tangentsBuffer ? glm::vec4(glm::make_vec4(&tangentsBuffer[v * 4])) : glm::vec4(0.0f);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}
			Primitive primitive{};
			primitive.firstIndex = firstIndex;
			primitive.indexCount = indexCount;
			primitive.materialIndex = glTFPrimitive.material;
			node->mesh.primitives.push_back(primitive);
		}
	}

	if (parent) {
		parent->children.push_back(node);
	}
	else {
		nodes.push_back(node);
	}
}

void GLTFLoader::loadImages(VulkanEngine& engine,tinygltf::Model& input)
{
	images.resize(input.images.size()); //do 
	for (size_t i = 0; i < input.images.size(); i++) {
		tinygltf::Image& glTFImage = input.images[i];
		// Get the image data from the glTF loader
		unsigned char* buffer = nullptr;
		VkDeviceSize bufferSize = 0;
		bool deleteBuffer = false;
		// We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
		if (glTFImage.component == 3) {
			bufferSize = glTFImage.width * glTFImage.height * 4;
			buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &glTFImage.image[0];
			for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
				memcpy(rgba, rgb, sizeof(unsigned char) * 3);
				rgba += 4;
				rgb += 3;
			}
			deleteBuffer = true;
		}
		else {
			std::cout << " rgba";
			buffer = &glTFImage.image[0];
			bufferSize = glTFImage.image.size();
		}
		// Load texture from image buffer
		vkutil::load_image_from_buffer(engine,buffer,bufferSize,glTFImage.width,glTFImage.height,images[i].texture);

		if (deleteBuffer) {
			delete[] buffer;
		}
	}
}

void GLTFLoader::loadTextures(VulkanEngine& engine,tinygltf::Model& input)
{
	textures.resize(input.textures.size() );
	for (size_t i = 0; i < input.textures.size(); i++) {
		textures[i].imageIndex = input.textures[i].source;
	}
}

void GLTFLoader::loadMaterials(VulkanEngine& engine,tinygltf::Model& input)
{
	materials.resize(input.materials.size());
	for (size_t i = 0; i < input.materials.size(); i++) {
		// We only read the most basic properties required for our sample
		tinygltf::Material glTFMaterial = input.materials[i];
		// Get the base color factor
		if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
			materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
		}
		// Get base color texture index
		if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
			materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
		}

		if (glTFMaterial.additionalValues.find("normalTexture") != glTFMaterial.additionalValues.end()) {
			materials[i].normalTextureIndex = glTFMaterial.additionalValues["normalTexture"].TextureIndex();
			//assert(1==2);
		}
		if (glTFMaterial.additionalValues.find("emissiveTexture") != glTFMaterial.additionalValues.end()) {
			materials[i].emissiveTextureIndex = glTFMaterial.additionalValues["emissiveTexture"].TextureIndex();
		}
		else {
			//materials[i].emissiveTextureIndex = textures[textures.size() - 1].imageIndex;
		}
		if (glTFMaterial.values.find("metallicRoughnessTexture") != glTFMaterial.values.end()) {
			materials[i].metallicRoughnessTextureIndex = glTFMaterial.values["metallicRoughnessTexture"].TextureIndex();
		}
	}
}

void GLTFLoader::drawNode(VulkanEngine& engine,VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, GLTFLoader::Node* node)
{
	if (node->mesh.primitives.size() > 0) {
		//Pass the node's matrix via push constants
		//Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		glm::mat4 nodeMatrix = node->matrix;
		GLTFLoader::Node* currentParent = node->parent;
		while (currentParent) {
			nodeMatrix = currentParent->matrix * nodeMatrix;
			currentParent = currentParent->parent;
		}
		//Pass the final matrix to the vertex shader using push constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &hmwks[node->index].descriptorSet, 0, nullptr);
		for (GLTFLoader::Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				// Get the texture index for this primitive
				//VulkanglTFModel::Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
				//VulkanglTFModel::Texture texture = textures[materials[primitive.materialIndex].normalTextureIndex];
				//VulkanglTFModel::Texture colorTexture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
				// Bind the descriptor for the current primitive's texture
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &materials[primitive.materialIndex].matDescriptorSet, 0, nullptr);
				//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &hmwks[node->index].descriptorSet, 0, nullptr);
				//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &images[colorTexture.imageIndex].descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNode(engine,commandBuffer, pipelineLayout, child);
	}
}

void GLTFLoader::draw(VulkanEngine& engine,VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
{
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.verticesBuffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	// Render all nodes at top-level
	for (auto& node : nodes) {
		drawNode(engine,commandBuffer, pipelineLayout, node);
	}
}

void GLTFLoader::loadgltfFile(VulkanEngine& engine,std::string filename)
{
    tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;

	std::string error, warning;

	bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

	std::vector<uint32_t> indexBuffer;
	std::vector<Vertex> vertexBuffer;
	if (fileLoaded)
	{
		loadImages(engine,glTFInput);

		loadTextures(engine,glTFInput);
		loadMaterials(engine,glTFInput);

		const tinygltf::Scene& scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			loadNode(engine,node, glTFInput, nullptr, scene.nodes[i], indexBuffer, vertexBuffer);
		}
	}
	else {
		std::cout << "Could not open the glTF file.\n\nThe file is part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.";
		return;
	}

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
	indices.count = static_cast<uint32_t>(indexBuffer.size());

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging, indexStaging;


	engine.createBuffer(
		vertexBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexStaging.buffer,
		vertexStaging.memory,
		vertexBuffer.data()
	);
	engine.createBuffer(
		indexBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexStaging.buffer,
		indexStaging.memory,
		indexBuffer.data()
	);

	engine.createBuffer(
		vertexBufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertices.verticesBuffer,
		vertices.verticesMemory
	);		
	engine.createBuffer(
		indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indices.buffer,
		indices.memory
	);

	engine.copyBuffer(vertexStaging.buffer, vertices.verticesBuffer, vertexBufferSize);
	engine.copyBuffer(indexStaging.buffer, indices.buffer, indexBufferSize);

	vkDestroyBuffer(engine._device, vertexStaging.buffer, nullptr);
	vkFreeMemory(engine._device, vertexStaging.memory, nullptr);
	vkDestroyBuffer(engine._device, indexStaging.buffer, nullptr);
	vkFreeMemory(engine._device, indexStaging.memory, nullptr);
}
