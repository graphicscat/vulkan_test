find_package(Vulkan REQUIRED)

add_library(glm INTERFACE)

add_library(stb_image INTERFACE)

add_library(sdl2 INTERFACE)
set(sdl2_DIR "SDL_PATH" CACHE FILEPATH "Path to SDL2")

# if (WIN32)
# target_include_directories(sdl2 INTERFACE ${sdl2_DIR}/include ${sdl2_image_DIR}/include)
# target_link_directories(sdl2 INTERFACE ${sdl2_DIR}/lib/x64 ${sdl2_image_DIR}/lib/x64)
# endif()

# target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

target_include_directories(glm INTERFACE glm)
target_include_directories(stb_image INTERFACE stb_image.h)