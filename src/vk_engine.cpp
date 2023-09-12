#include <vk_engine.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vk_texture.h>
//#include <vk_types.h>
#include <vk_initializers.h>
#include <vk_cubemap.h>
#include <math.h>

// #include <imgui.h>
// #include <backends/imgui_impl_vulkan.h>
// #include <backends/imgui_impl_sdl2.h>
#include <array>
#include <algorithm>
constexpr bool bUseValidationLayers = true;

//we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                \
		}                                                           \
	} while (0);


void VulkanEngine::mouse_callback()
{
	int xpos = 0;
	int ypos = 0;
	SDL_GetRelativeMouseState(&xpos,&ypos);	
	_camera.ProcessMouseMovement(xpos,ypos);
	//std::cout<<xpos<<' '<<ypos<<std::endl;
}

void VulkanEngine::keyboard_callback()
{
	
}


void VulkanEngine::init(){
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)<0)
		std::cout<<"INIT JOYSTICK FAILED"<<std::endl;
	//SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_WindowFlags windows_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        _windowExtent.width,
        _windowExtent.height,
        windows_flags
    );
	SDL_Joystick* gGameController = NULL;
	if( SDL_NumJoysticks() < 1 )
	{ 
    	printf( "Warning: No joysticks connected!\n" );
	}
	else
	{
		gGameController = SDL_JoystickOpen( 0 );
		if( gGameController == NULL )
		{ 
			printf( "Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError() );
		}
	}

	init_camera();

    init_vulkan();

    init_swapchain();

    createDepthStencil();

    init_default_renderpass();

    init_framebuffers();

    init_commands();

    init_sync_structures();

	init_descriptors();

    init_pipelines();

	load_texture();

	load_meshes();

	init_scene();

	init_imgui();

	//testGLTF.engine = *this;

    buildCommandBuffer();

    _isInitialized = true;
}

void VulkanEngine::cleanup(){
    if(_isInitialized)
    {
        vkDeviceWaitIdle(_device);

        _mainDeletionQueue.flush();

        vkDestroySurfaceKHR(_instance,_surface,nullptr);
        vkDestroyDevice(_device,nullptr);
        vkDestroyInstance(_instance,nullptr);
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }
}

void VulkanEngine::draw(){
    	//check if window is minimized and skip drawing
	if (SDL_GetWindowFlags(_window) & SDL_WINDOW_MINIMIZED)
		return;

	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, true, 1000000000));
	VK_CHECK(vkResetFences(_device, 1, &_renderFence));

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

	//request image from the swapchain
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _presentSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = _mainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//make a clear-color from frame number. This will flash with a 120 frame period.
	VkClearValue clearValue;
	float flash = std::abs(std::sin(_frameNumber / 120.f));
	clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

	//start the main renderpass. 
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

	//connect clear values
	rpInfo.clearValueCount = 1;
	rpInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);


	//once we start adding rendering commands, they will go here
	if (_selectedShader == 0)
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
	}
	else
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _redTrianglePipeline);
	}
	vkCmdDraw(cmd, 3, 1, 0, 0);

	//finalize the render pass
	vkCmdEndRenderPass(cmd);
	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkSubmitInfo submit = vkinit::submit_info(&cmd);
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_presentSemaphore;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_renderSemaphore;

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkinit::present_info();

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &_renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	//increase the number of frames drawn
	_frameNumber++;
}

void VulkanEngine::run(){
    SDL_Event e;
    bool bQuit = false;
	int x,y;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;
	const int JOYSTICK_DEAD_ZONE = 8000;
    while(!bQuit)
    {
		float currentTime = static_cast<float>(SDL_GetTicks())/1000.f;
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;
        while(SDL_PollEvent(&e) != 0)
        {
			//ImGui_ImplSDL2_ProcessEvent(&e);
			//if(e.type == SDL_MOUSEMOTION)mouse_callback();
			if(e.type == SDL_KEYDOWN)
			{
				if(e.key.keysym.sym == SDLK_w)
				_camera.ProcessKeyboard(FORWARD,deltaTime);
				if(e.key.keysym.sym == SDLK_s)
				_camera.ProcessKeyboard(BACKWARD,deltaTime);
				if(e.key.keysym.sym == SDLK_a)
				_camera.ProcessKeyboard(LEFT,deltaTime);
				if(e.key.keysym.sym == SDLK_d)
				_camera.ProcessKeyboard(RIGHT,deltaTime);
				if(e.key.keysym.sym == SDLK_ESCAPE)
				bQuit = true;
			}
            if(e.type == SDL_QUIT) bQuit = true;

			if( e.type == SDL_JOYAXISMOTION )
			{
				if( e.jaxis.which == 0 )
				{
					 if( e.jaxis.value < -JOYSTICK_DEAD_ZONE )
					{ 
						_camera.ProcessKeyboard(LEFT,deltaTime);
					}
					 else if( e.jaxis.value > JOYSTICK_DEAD_ZONE )
					{ 
						_camera.ProcessKeyboard(RIGHT,deltaTime);
					}
				}
			}
        }
        //draw();
		 ImGui_ImplVulkan_NewFrame();
		 ImGui_ImplSDL2_NewFrame(_window);

		 ImGui::NewFrame();


        // //imgui commands
        ImGui::ShowDemoWindow();
		ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
		updateUniformBuffer();
        //updateFrame();
		reBuildCommandBuffer(draw_data);
    }
}

void VulkanEngine::init_vulkan(){
    uint32_t extCount;
    SDL_Vulkan_GetInstanceExtensions(_window,&extCount,nullptr);
    std::vector<const char *> extensions(extCount);
    SDL_Vulkan_GetInstanceExtensions(_window,&extCount,extensions.data());

    VkInstanceCreateInfo instanceInfo{};
    VkApplicationInfo appInfo{};
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName = "Vulkan Engine";
    appInfo.pApplicationName = "Vulkan App";

    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledLayerCount = validationLayers.size();
    instanceInfo.ppEnabledLayerNames = validationLayers.data();
    instanceInfo.enabledExtensionCount = extensions.size();
    instanceInfo.ppEnabledExtensionNames = extensions.data();

    VK_CHECK(vkCreateInstance(&instanceInfo,nullptr,&_instance))

    SDL_Vulkan_CreateSurface(_window,_instance,&_surface);
    //init physicalDevice

    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(_instance,&physicalDeviceCount,nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(_instance,&physicalDeviceCount,physicalDevices.data());

    _chosenGPU = physicalDevices[0];

    VkPhysicalDeviceProperties physicalDevicePops{};
    vkGetPhysicalDeviceProperties(_chosenGPU,&physicalDevicePops);

    std::cout<<physicalDevicePops.deviceName<<std::endl;

    findQueueIndex();

    VkDeviceQueueCreateInfo queueInfo{};
    float queuePriority = 1.0f;
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;
    queueInfo.queueFamilyIndex = _graphicsQueueFamily;

    std::vector<const char*> arr = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = arr.data();

    VK_CHECK(vkCreateDevice(_chosenGPU,&deviceInfo,nullptr,&_device))
    vkGetDeviceQueue(_device,_graphicsQueueFamily,0,&_graphicsQueue);
}

void VulkanEngine::init_swapchain(){
    querySwapchainSupport();
    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageColorSpace = _details.format.colorSpace;
    swapchainInfo.imageExtent = _details.imageExtent;
    swapchainInfo.imageFormat = _details.format.format;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.minImageCount = _details.imageCount;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.presentMode = _details.present;
    swapchainInfo.preTransform = _details.transform;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
    swapchainInfo.surface = _surface;
    VK_CHECK(vkCreateSwapchainKHR(_device,&swapchainInfo,nullptr,&_swapchain))

    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(_device,_swapchain,&imageCount,nullptr);
    _swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_device,_swapchain,&imageCount,_swapchainImages.data());

    _swapchainImageViews.resize(_swapchainImages.size());

    for(int i = 0;i<_swapchainImageViews.size();i++)
    {
        auto createInfo = vkinit::imageview_begin_info(_swapchainImages[i],_details.format.format,VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(_device,&createInfo,nullptr,&_swapchainImageViews[i]))
    }

    _swapchainImageFormat = _details.format.format;

    _mainDeletionQueue.push_function([=](){
        vkDestroySwapchainKHR(_device,_swapchain,nullptr);
    });
}

void VulkanEngine::init_default_renderpass(){
    VkAttachmentDescription color_attachment{};
    color_attachment.format = _swapchainImageFormat;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment{};
    //TODO
	depth_attachment.format = _depthStencil.format;
	depth_attachment.flags = 0;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref{};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency depth_dependency = {};
	depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depth_dependency.dstSubpass = 0;
	depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.srcAccessMask = 0;
	depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::vector<VkAttachmentDescription> attachments = {color_attachment,depth_attachment};
	std::vector<VkSubpassDependency> dependencies = { dependency,depth_dependency};
    VkRenderPassCreateInfo renderpassInfo{};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassInfo.attachmentCount = 2;
    renderpassInfo.pAttachments = attachments.data();
    renderpassInfo.dependencyCount = 2;
    renderpassInfo.pDependencies = dependencies.data();
    renderpassInfo.subpassCount = 1;
    renderpassInfo.pSubpasses = &subpass;

    VK_CHECK(vkCreateRenderPass(_device,&renderpassInfo,nullptr,&_renderPass))

    _mainDeletionQueue.push_function([=](){
        vkDestroyRenderPass(_device,_renderPass,nullptr);
    });
}

void VulkanEngine::init_framebuffers(){
    VkFramebufferCreateInfo fb_info = vkinit::framebuffer_create_info(_renderPass,_windowExtent);

    const uint32_t swapchain_imageCount = _swapchainImageViews.size();
    _framebuffers.resize(swapchain_imageCount);

    for( int i = 0;i<swapchain_imageCount;i++)
    {
		std::vector<VkImageView> attachments = {_swapchainImageViews[i],_depthStencil.view};
        fb_info.pAttachments = attachments.data();
		fb_info.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(_device,&fb_info,nullptr,&_framebuffers[i]))

        _mainDeletionQueue.push_function([=](){
            vkDestroyFramebuffer(_device,_framebuffers[i],nullptr);
            vkDestroyImageView(_device,_swapchainImageViews[i],nullptr);
        });
    }

    
}

void VulkanEngine::init_commands(){
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily,VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(_device,&commandPoolInfo,nullptr,&_commandPool))

    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));

    flightCmdBuffers.resize(_framebuffers.size());
    for(auto& cmd :flightCmdBuffers)
    {
        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &cmd));
    }
	_mainDeletionQueue.push_function([=]() {
        vkFreeCommandBuffers(_device,_commandPool,flightCmdBuffers.size(),flightCmdBuffers.data());
        vkFreeCommandBuffers(_device,_commandPool,1,&_mainCommandBuffer);
		vkDestroyCommandPool(_device, _commandPool, nullptr);
	});
}

void VulkanEngine::init_sync_structures(){
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence));
	_renderFences.resize(2);
	for(int i = 0; i < 2; i++)
	{
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFences[i]));
		_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _renderFences[i], nullptr);
		});
	}

	//enqueue the destruction of the fence
	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _renderFence, nullptr);
		});
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));
	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));
	
	//enqueue the destruction of semaphores
	_mainDeletionQueue.push_function([=]() {
		vkDestroySemaphore(_device, _presentSemaphore, nullptr);
		vkDestroySemaphore(_device, _renderSemaphore, nullptr);
		});

	_presentSemaphores.resize(2);
	_renderSemaphores.resize(2);

	for(int i = 0;i < 2;i++)
	{
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphores[i]));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphores[i]));

		_mainDeletionQueue.push_function([=]() {
		vkDestroySemaphore(_device, _presentSemaphores[i], nullptr);
		vkDestroySemaphore(_device, _renderSemaphores[i], nullptr);
		});
	}


}

void VulkanEngine::findQueueIndex(){
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(_chosenGPU,&count,nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(_chosenGPU,&count,queueFamilyProperties.data());

    for(int i = 0 ;i<queueFamilyProperties.size();i++)
    {
        const auto& prop = queueFamilyProperties[i];
        if(prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            _graphicsQueueFamily = i;
        }
    }

}

void VulkanEngine::querySwapchainSupport(){
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_chosenGPU,_surface,&_details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_chosenGPU,_surface,&formatCount,nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_chosenGPU,_surface,&formatCount,surfaceFormats.data());

    _details.format = surfaceFormats[0];

    for(const auto& format:surfaceFormats)
    {
        if(format.format == VK_FORMAT_R8G8B8_SRGB&&
        format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR){
            _details.format = format;
            break;
        }
    }
    
    _details.imageCount = 2;
    _details.imageExtent.width = 1024;
    _details.imageExtent.height = 720;
    _details.transform = _details.capabilities.currentTransform;

    _details.present = VK_PRESENT_MODE_FIFO_KHR;
}

void VulkanEngine::init_pipelines()
{
	VkShaderModule colorMeshShader;
	if (!load_shader_module("../../shaders/test.frag.spv", &colorMeshShader))
	{
		std::cout << "Error when building the colored mesh shader" << std::endl;
	}

	VkShaderModule meshVertShader;
	if (!load_shader_module("../../shaders/test.vert.spv", &meshVertShader))
	{
		std::cout << "Error when building the mesh vertex shader module" << std::endl;
	}

	VkShaderModule defaultMeshShader;
	if (!load_shader_module("../../shaders/default.frag.spv", &defaultMeshShader))
	{
		std::cout << "Error when building the colored mesh shader" << std::endl;
	}

	VkShaderModule defaultVertShader;
	if (!load_shader_module("../../shaders/default.vert.spv", &defaultVertShader))
	{
		std::cout << "Error when building the mesh vertex shader module" << std::endl;
	}

	
	//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, colorMeshShader));


	//we start from just the default empty pipeline layout info
	VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
	std::vector<VkDescriptorSetLayout> texLayouts = {_descriptorSetLayout,_textureSetLayout};
	//setup push constants
	VkPushConstantRange push_constant;
	//offset 0
	push_constant.offset = 0;
	//size of a MeshPushConstant struct
	push_constant.size = sizeof(MeshPushConstants);
	//for the vertex shader
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
	mesh_pipeline_layout_info.pushConstantRangeCount = 1;
	mesh_pipeline_layout_info.setLayoutCount = texLayouts.size();
	mesh_pipeline_layout_info.pSetLayouts = texLayouts.data();

	VkPipelineLayout meshPipLayout;
	VkPipelineLayout defaultPipLayout;

	VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &meshPipLayout));

	//hook the push constants layout
	pipelineBuilder._pipelineLayout = meshPipLayout;

	//vertex input controls how to read vertices from vertex buffers. We arent using it yet
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

	//input assembly is the configuration for drawing triangle lists, strips, or individual points.
	//we are just going to draw triangle list
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	//build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;

	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = _windowExtent;

	//configure the rasterizer to draw filled triangles
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	//pipelineBuilder._rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	//we dont use multisampling, so just run the default one
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

	//a single blend attachment with no blending and writing to RGBA
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();


	//default depthtesting
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);

	//build the mesh pipeline

	VertexInputDescription vertexDescription = Vertex::get_vertex_description();

	//connect the pipeline builder vertex input info to the one we get from Vertex
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

	
	//build the mesh triangle pipeline
	VkPipeline meshPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);


	//default mesh pipeline
	pipelineBuilder._shaderStages.clear();
	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,defaultVertShader)
	);

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, defaultMeshShader)
	);
	
	mesh_pipeline_layout_info.setLayoutCount = 1;
	mesh_pipeline_layout_info.pSetLayouts = &_descriptorSetLayout;

	VK_CHECK(vkCreatePipelineLayout(_device,&mesh_pipeline_layout_info,nullptr,&defaultPipLayout))
	pipelineBuilder._pipelineLayout = defaultPipLayout;
	pipelineBuilder._depthStencil.depthTestEnable = VK_TRUE;
	pipelineBuilder._depthStencil.depthWriteEnable = VK_TRUE;
	pipelineBuilder._depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	//pipelineBuilder._rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	VkPipeline defaultPipeline =  pipelineBuilder.build_pipeline(_device,_renderPass);

	create_material(meshPipeline, meshPipLayout, "skyboxmesh");
	create_material(defaultPipeline,defaultPipLayout,"defaultmesh");

	vkDestroyShaderModule(_device, meshVertShader, nullptr);
	vkDestroyShaderModule(_device, colorMeshShader, nullptr);

	vkDestroyShaderModule(_device, defaultVertShader, nullptr);
	vkDestroyShaderModule(_device, defaultMeshShader, nullptr);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipeline(_device, meshPipeline, nullptr);

		vkDestroyPipelineLayout(_device, meshPipLayout, nullptr);

		vkDestroyPipeline(_device, defaultPipeline, nullptr);

		vkDestroyPipelineLayout(_device, defaultPipLayout, nullptr);
		
	});
}

bool VulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
{
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
        std::cout<<"cant find file";
		return false;
	}

	//find what the size of the file is by looking up the location of the cursor
	//because the cursor is at the end, it gives the size directly in bytes
	size_t fileSize = (size_t)file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve a int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	//put file cursor at beggining
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char*)buffer.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();

	//create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	//codeSize has to be in bytes, so multply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	//check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}
	*outShaderModule = shaderModule;
	return true;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{
	//make viewport state from our stored viewport and scissor.
		//at the moment we wont support multiple viewports or scissors
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &_viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &_scissor;

	//setup dummy color blending. We arent using transparent objects yet
	//the blending is just "no blend", but we do write to the color attachment
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;

	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

	//build the actual pipeline
	//we now use all of the info structs we have been writing into into this one to create the pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	pipelineInfo.stageCount = _shaderStages.size();
	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pVertexInputState = &_vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &_inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &_rasterizer;
	pipelineInfo.pMultisampleState = &_multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.pDepthStencilState = &_depthStencil;
	//its easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case
	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(
		device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
		std::cout << "failed to create pipline\n";
		return VK_NULL_HANDLE; // failed to create graphics pipeline
	}
	else
	{
		return newPipeline;
	}
}

void VulkanEngine::buildCommandBuffer()
{
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info();

    VkClearValue clearValues[2];
    //float flash = std::abs(std::sin(_frameNumber / 120.f));
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f , 0};
    for(int i = 0; i < flightCmdBuffers.size(); i++)
    {
        VK_CHECK(vkBeginCommandBuffer(flightCmdBuffers[i], &cmdBeginInfo));
        //start the main renderpass. 
        //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
        VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[i]);

        //connect clear values
        rpInfo.clearValueCount = 2;
        rpInfo.pClearValues = clearValues;

        vkCmdBeginRenderPass(flightCmdBuffers[i], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);


        //once we start adding rendering commands, they will go here
        // if (_selectedShader == 0)
        // {
        //     vkCmdBindPipeline(flightCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
        // }
        // else
        // {
        //     vkCmdBindPipeline(flightCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _redTrianglePipeline);
        // }
        //vkCmdDraw(flightCmdBuffers[i], 3, 1, 0, 0);
		draw_objects(flightCmdBuffers[i], _renderables.data(), _renderables.size());

        //finalize the render pass
		//ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), flightCmdBuffers[i]);
        vkCmdEndRenderPass(flightCmdBuffers[i]);
        //finalize the command buffer (we can no longer add commands, but it can now be executed)
        VK_CHECK(vkEndCommandBuffer(flightCmdBuffers[i]));
    }
}

void VulkanEngine::updateFrame()
{
	
	uint32_t currentFrame = _frameNumber % 2;
    uint32_t nextImage = 0;
	VK_CHECK(vkWaitForFences(_device,1,&_renderFences[currentFrame],VK_TRUE,UINT64_MAX));
	
	VK_CHECK(vkResetFences(_device,1,&_renderFences[currentFrame]));
    VK_CHECK(vkAcquireNextImageKHR(_device,_swapchain,UINT64_MAX,_presentSemaphores[currentFrame],VK_NULL_HANDLE,&nextImage))
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &flightCmdBuffers[currentFrame];
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &_presentSemaphores[currentFrame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &_renderSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(_graphicsQueue,1,&submit,_renderFences[currentFrame]))

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pImageIndices = &nextImage;
    present.swapchainCount = 1;
    present.pSwapchains = &_swapchain;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &_renderSemaphores[currentFrame];
    VK_CHECK(vkQueuePresentKHR(_graphicsQueue,&present))

	_frameNumber ++;
   // vkQueueWaitIdle(_graphicsQueue);

}

void VulkanEngine::reBuildCommandBuffer(ImDrawData* draw_data)
{
	uint32_t currentFrame = _frameNumber % 2;
    uint32_t nextImage = 0;
	VK_CHECK(vkWaitForFences(_device,1,&_renderFences[currentFrame],VK_TRUE,UINT64_MAX));
	VK_CHECK(vkResetFences(_device,1,&_renderFences[currentFrame]));
    VK_CHECK(vkAcquireNextImageKHR(_device,_swapchain,UINT64_MAX,_presentSemaphores[currentFrame],VK_NULL_HANDLE,&nextImage))

	vkResetCommandBuffer(flightCmdBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info();

    VkClearValue clearValues[2];
    //float flash = std::abs(std::sin(_frameNumber / 120.f));
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
	clearValues[1].depthStencil = { 1.0f , 0};
    
	VK_CHECK(vkBeginCommandBuffer(flightCmdBuffers[currentFrame], &cmdBeginInfo));
	//start the main renderpass. 
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[currentFrame]);

	//connect clear values
	rpInfo.clearValueCount = 2;
	rpInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(flightCmdBuffers[currentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);


	//once we start adding rendering commands, they will go here
	// if (_selectedShader == 0)
	// {
	//     vkCmdBindPipeline(flightCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
	// }
	// else
	// {
	//     vkCmdBindPipeline(flightCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _redTrianglePipeline);
	// }
	//vkCmdDraw(flightCmdBuffers[i], 3, 1, 0, 0);
	draw_objects(flightCmdBuffers[currentFrame], _renderables.data(), _renderables.size());
	ImGui_ImplVulkan_RenderDrawData(draw_data,flightCmdBuffers[currentFrame]);
	//finalize the render pass
	//ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), flightCmdBuffers[i]);
	vkCmdEndRenderPass(flightCmdBuffers[currentFrame]);
	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(flightCmdBuffers[currentFrame]));
    

	VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &flightCmdBuffers[currentFrame];
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &_presentSemaphores[currentFrame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &_renderSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(_graphicsQueue,1,&submit,_renderFences[currentFrame]))

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pImageIndices = &nextImage;
    present.swapchainCount = 1;
    present.pSwapchains = &_swapchain;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &_renderSemaphores[currentFrame];
    VK_CHECK(vkQueuePresentKHR(_graphicsQueue,&present))
	_frameNumber ++;

}

Material *VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string &name)
{
    Material mat;
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;
	_materials[name] = mat;
	return &_materials[name];
}

Material* VulkanEngine::get_material(const std::string& name)
{
    auto it = _materials.find(name);
	if (it == _materials.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}

Mesh* VulkanEngine::get_mesh(const std::string& name)
{
    auto it = _meshes.find(name);
	if (it == _meshes.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first, int count)
{
	glm::vec3 camPos = { 0.f,-6.f,-10.f };

	glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
	//camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
	projection[1][1] *= -1;

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < count; i++)
	{
		RenderObject& object = first[i];

		//only bind the pipeline if it doesnt match with the already bound one
		if (object.material != lastMaterial) {

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;
		}


		glm::mat4 model = object.transformMatrix;
		//final render matrix, that we are calculating on the cpu
		glm::mat4 mesh_matrix = model;

		MeshPushConstants constants;
		constants.render_matrix = mesh_matrix;
		constants.objectColor = glm::vec4(object.mesh->objectColor,1.0f);

		//upload the mesh to the gpu via pushconstants
		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);
		vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,object.material->pipelineLayout,0,1,&_uboSet,0,nullptr);
		if(object.material->textureSet!=VK_NULL_HANDLE)
		vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,object.material->pipelineLayout,1,1,&object.material->textureSet,0,nullptr);
		//only bind the mesh if its a different one from last bind
		if (object.mesh != lastMesh) {
			//bind the mesh vertex buffer with offset 0
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer.buffer, &offset);
			lastMesh = object.mesh;
		}
		//we can now draw
		vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, 0);
	}
}

void VulkanEngine::init_scene()
{
	RenderObject monkey;
	monkey.mesh = get_mesh("monkey");
	monkey.material = get_material("defaultmesh");
	monkey.transformMatrix = glm::translate(glm::vec3(0.0f,3.0f,0.0f))*glm::scale(glm::vec3(2.2f,2.2f,2.2f));
	monkey.mesh->objectColor = glm::vec3(1.0f,0.98f,0.95f);

	RenderObject skybox;
	skybox.mesh = get_mesh("cube");
	skybox.material = get_material("skyboxmesh");
	skybox.transformMatrix = glm::mat4{ 2.0f };

	RenderObject floor;
	floor.mesh = get_mesh("cube");
	floor.material = get_material("defaultmesh");
	floor.transformMatrix = glm::scale(glm::vec3(50.f,.2f,50.f));
	floor.mesh->objectColor = glm::vec3(0.6f,0.8f,0.16f);

	_renderables.push_back(skybox);
	_renderables.push_back(monkey);
	_renderables.push_back(floor);

	// for (int x = -20; x <= 20; x++) {
	// 	for (int y = -20; y <= 20; y++) {

	// 		RenderObject tri;
	// 		tri.mesh = get_mesh("triangle");
	// 		tri.material = get_material("defaultmesh");
	// 		glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
	// 		glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
	// 		tri.transformMatrix = translation * scale;

	// 		_renderables.push_back(tri);
	// 	}
	// }
	RenderObject map;
	map.mesh = get_mesh("empire");
	map.material = get_material("defaultmesh");
	map.transformMatrix = glm::translate(glm::vec3{ 5,-10,0 }); //glm::mat4{ 1.0f };

	//_renderables.push_back(map);
	Material* texturedMat=	get_material("skyboxmesh");

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = nullptr;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &_textureSetLayout;

	vkAllocateDescriptorSets(_device, &allocInfo, &texturedMat->textureSet);

	VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &_loadedTextures["skybox"].descriptor, 0);
	
	vkUpdateDescriptorSets(_device,1,&texture1,0,nullptr);
}

void VulkanEngine::load_meshes()
{
    Mesh triMesh{};
	//make the array 3 vertices long
	triMesh._vertices.resize(3);

	//vertex positions
	triMesh._vertices[0].position = { 1.f,1.f, 0.0f };
	triMesh._vertices[1].position = { -1.f,1.f, 0.0f };
	triMesh._vertices[2].position = { 0.f,-1.f, 0.0f };

	//vertex colors, all green
	triMesh._vertices[0].color = { 0.f,1.f, 0.0f }; //pure green
	triMesh._vertices[1].color = { 0.f,1.f, 0.0f }; //pure green
	triMesh._vertices[2].color = { 0.f,1.f, 0.0f }; //pure green
	//we dont care about the vertex normals

	//load the monkey
	Mesh cubeMesh{};
	cubeMesh.load_from_obj("../../assets/cube.obj");
	Mesh lostEmpire{};
	lostEmpire.load_from_obj("../../assets/lost_empire.obj");
	Mesh monkeyMesh{};
	
	monkeyMesh.load_from_obj("../../assets/monkey_smooth.obj");

	//alloc buffer
	upload_mesh(triMesh);
	upload_mesh(cubeMesh);
	upload_mesh(lostEmpire);
	upload_mesh(monkeyMesh);

	_meshes["monkey"] = monkeyMesh;
	_meshes["cube"] = cubeMesh;
	_meshes["triangle"] = triMesh;
	_meshes["empire"] = lostEmpire;
    //_meshes.emplace("monkey",monkeyMesh);

	//testGLTF.loadgltfFile(*this,"../../assets/glTF-Sample-Models-master/glTF-Sample-Models-master/2.0/Sponza/glTF/sponza.gltf");
}

void VulkanEngine::upload_mesh(Mesh& mesh)
{
    // //allocate vertex buffer
	// VkBufferCreateInfo bufferInfo = {};
	// bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	// bufferInfo.pNext = nullptr;
	// //this is the total size, in bytes, of the buffer we are allocating
	// bufferInfo.size = mesh._vertices.size() * sizeof(Vertex);
	// //this buffer is going to be used as a Vertex Buffer
	// bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    size_t vertexBufferSize = mesh._vertices.size()*sizeof(Vertex);

    struct StagingBuffer
    {
        /* data */
        VkBuffer buffer;
        VkDeviceMemory memory;
    }stagingBuffer{};

	if(mesh._vertices.empty())
	{
		std::cout<<"empty";
	}
    createBuffer(vertexBufferSize,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    stagingBuffer.buffer,
    stagingBuffer.memory,
    mesh._vertices.data());

    createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mesh._vertexBuffer.buffer,
        mesh._vertexBuffer.memory
    );

    copyBuffer(stagingBuffer.buffer,mesh._vertexBuffer.buffer,vertexBufferSize);

    vkFreeMemory(_device,stagingBuffer.memory,nullptr);
    vkDestroyBuffer(_device,stagingBuffer.buffer,nullptr);

    _mainDeletionQueue.push_function([=](){
        vkFreeMemory(_device,mesh._vertexBuffer.memory,nullptr);
        vkDestroyBuffer(_device,mesh._vertexBuffer.buffer,nullptr);
    }
    );

}

VkCommandBuffer VulkanEngine::beginSingleCommand()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(_device,&allocInfo,&cmdBuffer))

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer,&beginInfo))

    return cmdBuffer;

};

void VulkanEngine::endSingleCommand(VkCommandBuffer cmdBuffer)
{
    vkEndCommandBuffer(cmdBuffer);
    VkSubmitInfo submit{};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmdBuffer;
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    vkQueueSubmit(_graphicsQueue,1,&submit,VK_NULL_HANDLE);
    vkDeviceWaitIdle(_device);
    vkFreeCommandBuffers(_device,_commandPool,1,&cmdBuffer);
};

void VulkanEngine::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory,
    void* data/* = nullptr*/)
    {
        VkBufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = size;
        createInfo.usage = usage;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(_device,&createInfo,nullptr,&buffer))

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements memRequirements{};
        vkGetBufferMemoryRequirements(_device,buffer,&memRequirements);
        auto typeFilter = memRequirements.memoryTypeBits;
        allocInfo.memoryTypeIndex = findMemoryType(typeFilter,properties);
        allocInfo.allocationSize = memRequirements.size;

        VK_CHECK(vkAllocateMemory(_device,&allocInfo,nullptr,&memory))
        VK_CHECK(vkBindBufferMemory(_device,buffer,memory,0))
        void* mapped = nullptr;

        if(data != nullptr)
        {
            vkMapMemory(_device,memory,0,size,0,&mapped);
            memcpy(mapped,data,size);
            vkUnmapMemory(_device,memory);
        }
};

int VulkanEngine::findMemoryType(int typeFilter,VkMemoryPropertyFlags properties)
{
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties(_chosenGPU,&memProperties);

        for(uint32_t i = 0; i < memProperties.memoryTypeCount;i++)
        {
            if(typeFilter & ( 1 << i ) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i ;
        }

        return 0;
}

 void VulkanEngine::copyBuffer(VkBuffer srcBuffer,VkBuffer dstBuffer,VkDeviceSize size)
{
        VkCommandBuffer cmd = beginSingleCommand();
		VkBufferCopy bufferCopy{};
		bufferCopy.size = size;
		bufferCopy.dstOffset = 0;
		bufferCopy.srcOffset = 0;

		vkCmdCopyBuffer(cmd,srcBuffer,dstBuffer,1,&bufferCopy);
		endSingleCommand(cmd);
}

void VulkanEngine::createImage(
		uint32_t width,
		uint32_t height,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory
	)
{
    VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	VK_CHECK(vkAllocateMemory(_device,&allocInfo,nullptr,&imageMemory))
	VK_CHECK(vkBindImageMemory(_device,image,imageMemory,0))
}

void VulkanEngine::createImage(VkImageCreateInfo imageInfo,
		VkMemoryPropertyFlags properties,
		VkImage& image,
		VkDeviceMemory& imageMemory)
{
	if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	VK_CHECK(vkAllocateMemory(_device,&allocInfo,nullptr,&imageMemory))
	VK_CHECK(vkBindImageMemory(_device,image,imageMemory,0))
}

VkFormat VulkanEngine::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{

	for (auto format : candidates)
	{
		VkFormatProperties props{};
		vkGetPhysicalDeviceFormatProperties(_chosenGPU, format, &props);
		//props = physicalDevice.getFormatProperties(format);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}
    return VK_FORMAT_D32_SFLOAT;
}

VkFormat VulkanEngine::findDepthFormat()
{
	return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

void VulkanEngine::createDepthStencil()
{
	VkFormat format = findDepthFormat();
	_depthStencil.format = format;
	createImage(
		_details.imageExtent.width,
		_details.imageExtent.height,
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_depthStencil.image,
		_depthStencil.mem
	);
	_depthStencil.view = createImageView(_depthStencil.image, format, VK_IMAGE_ASPECT_DEPTH_BIT);

    _mainDeletionQueue.push_function([=](){
        vkFreeMemory(_device,_depthStencil.mem,nullptr);
        vkDestroyImageView(_device,_depthStencil.view,nullptr);
        vkDestroyImage(_device,_depthStencil.image,nullptr);
    });
}

VkImageView VulkanEngine::createImageView(VkImage image, VkFormat format, VkImageAspectFlagBits aspect)
{
    VkImageViewCreateInfo createInfo{};
	VkImageSubresourceRange range{};
	VkImageView  imageView{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.format = format;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	range.aspectMask = aspect;
	range.baseArrayLayer = 0;
	range.baseMipLevel = 0;
	range.layerCount = 1;
	range.levelCount = 1;
	createInfo.subresourceRange = range;
	VK_CHECK(vkCreateImageView(_device,&createInfo,nullptr,&imageView))
	return imageView;
}

VkImageView VulkanEngine::createImageView(VkImage image, VkImageViewCreateInfo imageViewInfo)
{
	VkImageView  imageView{};
	VK_CHECK(vkCreateImageView(_device,&imageViewInfo,nullptr,&imageView))
	return imageView;
}	



void VulkanEngine::init_descriptors()
{
	createUniformBuffer();

	std::vector<VkDescriptorPoolSize> sizes = 
	{
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,10}
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 10;
	poolInfo.poolSizeCount = (uint32_t)sizes.size();
	poolInfo.pPoolSizes = sizes.data();

	VK_CHECK(vkCreateDescriptorPool(_device,&poolInfo,nullptr,&_descriptorPool))

	VkDescriptorSetLayoutBinding cameraBind = 
	vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0);

	VkDescriptorSetLayoutBinding textureBind = 
	vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,0);

	std::vector<VkDescriptorSetLayoutBinding> bindings = {cameraBind};
	std::vector<VkDescriptorSetLayoutBinding> texbindings = {textureBind};

	VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo {};
	descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayoutInfo.flags = 0;
	descriptorLayoutInfo.pBindings = bindings.data();
	descriptorLayoutInfo.bindingCount = bindings.size();

	VK_CHECK(vkCreateDescriptorSetLayout(_device,&descriptorLayoutInfo,nullptr,&_descriptorSetLayout))

	descriptorLayoutInfo.pBindings = texbindings.data();
	descriptorLayoutInfo.bindingCount = texbindings.size();
	VK_CHECK(vkCreateDescriptorSetLayout(_device,&descriptorLayoutInfo,nullptr,&_textureSetLayout))

	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.descriptorPool = _descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &_descriptorSetLayout;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

		VK_CHECK(vkAllocateDescriptorSets(_device,&allocInfo,&_uboSet))

		VkWriteDescriptorSet writer{};
		writer.descriptorCount = 1;
		writer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writer.dstBinding = 0;
		writer.dstSet = _uboSet;
		writer.pBufferInfo = &_shaderData._cameraBuffer.descriptor;
		writer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vkUpdateDescriptorSets(_device,1,&writer,0,nullptr);
	}


	_mainDeletionQueue.push_function([=](){
		vkDestroyDescriptorSetLayout(_device,_descriptorSetLayout,nullptr);
		vkDestroyDescriptorSetLayout(_device,_textureSetLayout,nullptr);
		vkDestroyDescriptorPool(_device,_descriptorPool,nullptr);
	});

}

void VulkanEngine::createUniformBuffer()
{
	uint32_t size = sizeof(_shaderData._cameraData);
	createBuffer(size,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	_shaderData._cameraBuffer.buffer,
	_shaderData._cameraBuffer.mem);


	_shaderData._cameraBuffer.descriptor.buffer = _shaderData._cameraBuffer.buffer;
	_shaderData._cameraBuffer.descriptor.offset = 0;
	_shaderData._cameraBuffer.descriptor.range = size;

	VK_CHECK(vkMapMemory(_device,_shaderData._cameraBuffer.mem,0,size,0,&_shaderData._cameraBuffer.mapped))

	_mainDeletionQueue.push_function([=](){
		vkFreeMemory(_device,_shaderData._cameraBuffer.mem,nullptr);
		vkDestroyBuffer(_device,_shaderData._cameraBuffer.buffer,nullptr);
	});
}

void VulkanEngine::updateUniformBuffer()
{
	// glm::vec3 camPos = { 0.f,-6.f,-10.f };

	// glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
	// //camera projection
	// glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
	// projection[1][1] *= -1;

	// _shaderData._cameraData.proj = projection;

	 _shaderData._cameraData.view = _camera.GetViewMatrix();
	 _shaderData._cameraData.viewPos = glm::vec4(_camera.Position,1.0f);
	 _shaderData._cameraData.viewproj = _shaderData._cameraData.proj *_shaderData._cameraData.view;

	memcpy(_shaderData._cameraBuffer.mapped,&_shaderData._cameraData,sizeof(_shaderData._cameraData));
}

void VulkanEngine::init_camera()
{
	glm::vec3 camPos = { 0.f,2.f,10.f };
	glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
	projection[1][1] *= -1;
	_camera = Camera{camPos};
	_shaderData._cameraData.viewPos = glm::vec4(_camera.Position,1.0f);
	_shaderData._cameraData.view = _camera.GetViewMatrix();
	_shaderData._cameraData.proj = projection;
	_shaderData._cameraData.viewproj = _shaderData._cameraData.proj*_shaderData._cameraData.view;
}

VkSampler VulkanEngine::createSampler()
{
	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.anisotropyEnable = VK_FALSE;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0.0f;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = 0.0f;
	VkSampler sampler{};
	VK_CHECK(vkCreateSampler(_device, &createInfo, nullptr, &sampler));
	return sampler;
}

void VulkanEngine::load_texture()
{
	AllocatedImage lostEmpire;
	vkutil::load_image_from_file(*this, "../../assets/lost_empire-RGBA.png", lostEmpire);

	std::vector<const char*> files ={
		"../../assets/skybox_right.jpg",
		"../../assets/skybox_left.jpg",
		"../../assets/skybox_top.jpg",
		"../../assets/skybox_bottom.jpg",
		"../../assets/skybox_front.jpg",
		"../../assets/skybox_back.jpg"
	};
	AllocatedImage skybox;
	vkcubemap::load_image_from_file(*this,files,skybox);

	_loadedTextures["lostEmpire"] = lostEmpire;
	_loadedTextures["skybox"] = skybox;
}


void VulkanEngine::init_imgui()
{
	//1: create descriptor pool for IMGUI
	// the size of the pool is very oversize, but it's copied from imgui demo itself.
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));


	// 2: initialize imgui library

	//this initializes the core structures of imgui
	ImGui::CreateContext();

	//this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_window);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, _renderPass);

	//execute a gpu command to upload imgui font textures
	VkCommandBuffer cmd = beginSingleCommand();
	ImGui_ImplVulkan_CreateFontsTexture(cmd);
	endSingleCommand(cmd);

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	//add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {

		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		});
}
