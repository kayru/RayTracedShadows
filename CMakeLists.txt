cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(RayTracedShadows)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(RUSH_RENDER_API "MTL" CACHE STRING "Force Metal renderer")
	set(USE_VK_RAYTRACING 0)
else()
	set(RUSH_RENDER_API "VK" CACHE STRING "Force Vulkan renderer")
	set(USE_VK_RAYTRACING 1)
endif()

add_subdirectory("External")
add_subdirectory("Source")
