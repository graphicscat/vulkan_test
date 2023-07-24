add_executable(vulkan_test 
vk_engine.h
vk_initializers.h
main.cpp
vk_engine.cpp
vk_initializers.cpp
)

target_include_directories(vulkan_test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(vulkan_test glm)
target_link_libraries(vulkan_test Vulkan::Vulkan SDL2)

