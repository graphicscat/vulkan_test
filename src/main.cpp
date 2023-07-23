#include <iostream>
#include <glm/glm.hpp>
#include <test.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
int main(int argc, char** argv)
{
    VkInstance instance_;
    glm::vec3 vec(1.0f,1.0f,1.0f);
    std::cout<<vec.x;
    sayHello();
    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_Window* window = SDL_CreateWindow("sandbox",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          1024, 720,
                                          SDL_WINDOW_SHOWN|SDL_WINDOW_VULKAN);
    if (!window) {
        SDL_Log("create window failed");
        exit(2);
    }
    bool shouldClose = false;
    SDL_Event event;

    //toy2d::Init();

    while (!shouldClose) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                shouldClose = true;
            }
        }
    }

    //toy2d::Quit();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}