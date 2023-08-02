#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>
#include <vk_mesh.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_camera.h>
const std::vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"  
};

class PipelineBuilder {
public:

	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;
	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& func){
		deletors.push_back(func);
	}

	void flush(){
		for(auto it = deletors.rbegin();it!=deletors.rend();it++){
			(*it)();
		}

		deletors.clear();
	}
};

struct Material {
	VkDescriptorSet textureSet{VK_NULL_HANDLE};
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;

	Material* material;

	glm::mat4 transformMatrix;
};

struct MeshPushConstants {
	//glm::vec4 data;
	glm::mat4 render_matrix;
};

struct ShaderData
{
	struct GPUCameraData{
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 viewproj;
	}_cameraData;

	struct UBOBuffer
	{
		VkBuffer buffer;
		VkDeviceMemory mem;
		VkDescriptorBufferInfo descriptor{};
		void* mapped = nullptr;
	}_cameraBuffer;

};

class VulkanEngine {
public:

    bool _isInitialized{false};
    int _frameNumber {0};
	int _selectedShader{ 1 };

	VkExtent2D _windowExtent{ 1024 , 720 };

	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;

	VkSemaphore _presentSemaphore, _renderSemaphore;
	std::vector<VkSemaphore> _presentSemaphores;
	std::vector<VkSemaphore>_renderSemaphores;
	VkFence _renderFence;
	std::vector<VkFence> _renderFences;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	std::vector<VkCommandBuffer> flightCmdBuffers;
	
	VkRenderPass _renderPass;

	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	struct SwapchainDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		VkSurfaceFormatKHR format;
		VkPresentModeKHR present;
		VkSurfaceTransformFlagBitsKHR transform;
		VkExtent2D imageExtent;
		uint32_t imageCount;
	}_details;
	

	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;
	VkPipeline _redTrianglePipeline;
	DeletionQueue _mainDeletionQueue;

	ShaderData _shaderData;
	VkDescriptorPool _descriptorPool;
	VkDescriptorSetLayout _descriptorSetLayout;
	VkDescriptorSetLayout _textureSetLayout;
	VkDescriptorSet _uboSet;

	AllocatedImage _texture;
	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	//default array of renderable objects
	std::vector<RenderObject> _renderables;

	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;
	std::unordered_map<std::string, AllocatedImage> _loadedTextures;

	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkFormat format;
	} _depthStencil;

	Camera _camera;

		void createBuffer(
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkBuffer& buffer,
		VkDeviceMemory& memory,
		void* data = nullptr
	);

	int findMemoryType(int typeFilter,VkMemoryPropertyFlags properties);

	void copyBuffer(VkBuffer srcBuffer,VkBuffer dstBuffer,VkDeviceSize size);

	void createImage(
		uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory
	);

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlagBits aspect);

	VkCommandBuffer beginSingleCommand();

	void endSingleCommand(VkCommandBuffer cmdBuffer);

	VkSampler createSampler();

private:

	void init_vulkan();

	void init_swapchain();

	void init_default_renderpass();

	void init_framebuffers();

	void init_commands();

	void init_sync_structures();

    void findQueueIndex();

	void querySwapchainSupport();

	void init_pipelines();

	//loads a shader module from a spir-v file. Returns false if it errors
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	void buildCommandBuffer();

	void updateFrame();

	void init_camera();

	//functions

	//create material and add it to the map
	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	//returns nullptr if it cant be found
	Material* get_material(const std::string& name);

	//returns nullptr if it cant be found
	Mesh* get_mesh(const std::string& name);

	//our draw function
	void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);

	void init_scene();

	void load_meshes();

	void upload_mesh(Mesh& mesh);


	void load_texture();


	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	VkFormat findDepthFormat();

	void createDepthStencil();

	void init_descriptors();

	void createUniformBuffer();

	void updateUniformBuffer();

	void mouse_callback();
	void keyboard_callback();
};