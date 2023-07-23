#include <vk_engine.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>
#include <iostream>
#include <array>
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

void VulkanEngine::init_swapchain(){}

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