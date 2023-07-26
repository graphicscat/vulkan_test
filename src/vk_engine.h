#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>
#include <vk_mesh.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

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
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;

	Material* material;

	glm::mat4 transformMatrix;
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
	VkFence _renderFence;

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
	}details;
	

	std::vector<VkFramebuffer> _framebuffers;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;
	VkPipeline _redTrianglePipeline;
	DeletionQueue _mainDeletionQueue;

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

	VkCommandBuffer beginSingleCommand();

	void endSingleCommand(VkCommandBuffer cmdBuffer);

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
};