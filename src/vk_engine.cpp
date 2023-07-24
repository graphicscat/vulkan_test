#include <vk_engine.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

//#include <vk_types.h>
#include <vk_initializers.h>
#include <iostream>
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


void VulkanEngine::init(){
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags windows_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        _windowExtent.width,
        _windowExtent.height,
        windows_flags
    );

    init_vulkan();

    init_swapchain();

    init_default_renderpass();

    init_framebuffers();

    init_commands();

    init_sync_structures();

    _isInitialized = true;
}

void VulkanEngine::cleanup(){
    if(_isInitialized)
    {
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }
}

void VulkanEngine::draw(){}

void VulkanEngine::run(){
    SDL_Event e;
    bool bQuit = false;

    while(!bQuit)
    {
        while(SDL_PollEvent(&e) != 0)
        {
            if(e.type == SDL_QUIT) bQuit = true;
        }
        draw();
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
    swapchainInfo.imageColorSpace = details.format.colorSpace;
    swapchainInfo.imageExtent = details.imageExtent;
    swapchainInfo.imageFormat = details.format.format;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.minImageCount = details.imageCount;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.presentMode = details.present;
    swapchainInfo.preTransform = details.transform;
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
        auto createInfo = vkinit::imageview_begin_info(_swapchainImages[i],details.format.format,VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(_device,&createInfo,nullptr,&_swapchainImageViews[i]))
    }

    _swapchainImageFormat = details.format.format;
}

void VulkanEngine::init_default_renderpass(){}

void VulkanEngine::init_framebuffers(){}

void VulkanEngine::init_commands(){}

void VulkanEngine::init_sync_structures(){}

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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_chosenGPU,_surface,&details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_chosenGPU,_surface,&formatCount,nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_chosenGPU,_surface,&formatCount,surfaceFormats.data());

    details.format = surfaceFormats[0];

    for(const auto& format:surfaceFormats)
    {
        if(format.format == VK_FORMAT_R8G8B8_SRGB&&
        format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR){
            details.format = format;
            break;
        }
    }
    
    details.imageCount = 2;
    details.imageExtent.width = 1024;
    details.imageExtent.height = 720;
    details.transform = details.capabilities.currentTransform;

    details.present = VK_PRESENT_MODE_FIFO_KHR;
}