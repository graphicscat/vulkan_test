add_executable(vulkan_test 
vk_engine.h
vk_initializers.h
main.cpp
vk_engine.cpp
vk_initializers.cpp
vk_mesh.h
vk_mesh.cpp
vk_camera.h
vk_texture.h
vk_texture.cpp
vk_cubemap.h
vk_cubemap.cpp
vk_gltfloader.h
vk_gltfloader.cpp
)

target_include_directories(vulkan_test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(vulkan_test glm stb_image)
target_link_libraries(vulkan_test Vulkan::Vulkan SDL2 tinyobjloader tinygltf imgui) 

