cmake_minimum_required(VERSION 3.20)

project(vulkan_test LANGUAGES CXX)

find_package(Vulkan REQUIRED)


add_subdirectory(third_party)
add_subdirectory(src)

if(NOT TARGET SDL2)
    #message(FATAL_ERROR "error")

    set(SDL2_ROOT "" CACHE PATH "SDL2 root directory")
    set(SDL2_INCLUDE_DIR "${Vulkan_INCLUDE_DIRS}")
    set(SDL2_LIB_DIR "$ENV{VULKAN_SDK}/Lib")
    set(SDL2_BIN_DIR "$ENV{VULKAN_SDK}/Bin")
    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
    set_target_properties(
        SDL2::SDL2
        PROPERTIES
            IMPORTED_LOCATION "${SDL2_BIN_DIR}/SDL2.dll"
            IMPORTED_IMPLIB "${SDL2_LIB_DIR}/SDL2.lib"
            INTERFACE_INCLUDE_DIRECTORIES ${SDL2_INCLUDE_DIR}
    )
    add_library(SDL2::SDL2main SHARED IMPORTED GLOBAL)
    set_target_properties(
        SDL2::SDL2main
        PROPERTIES
            IMPORTED_LOCATION "${SDL2_BIN_DIR}/SDL2.dll"
            IMPORTED_IMPLIB "${SDL2_LIB_DIR}/SDL2main.lib"
            INTERFACE_INCLUDE_DIRECTORIES ${SDL2_INCLUDE_DIR}
    )
    add_library(SDL2 INTERFACE IMPORTED GLOBAL)
    target_link_libraries(SDL2 INTERFACE SDL2::SDL2 SDL2::SDL2main)
endif()
