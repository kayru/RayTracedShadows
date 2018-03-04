cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(RayTracedShadows)

set(RUSH_RENDER_API "VK" CACHE STRING "Force Vulkan renderer")

find_program(GLSLC NAMES glslc PATHS
    $ENV{VK_SDK_PATH}/Bin
    $ENV{PATH}
    "~/bin"
)

add_subdirectory("External")
add_subdirectory("Source")
