cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(RayTracedShadows)

set(RUSH_RENDER_API "VK" CACHE STRING "Force Vulkan renderer")

add_subdirectory("External")
add_subdirectory("Source")
