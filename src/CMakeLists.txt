add_executable(vulkan_test main.cpp)

target_include_directories(vulkan_test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(vulkan_test glm)
target_link_libraries(vulkan_test Vulkan::Vulkan SDL2)

