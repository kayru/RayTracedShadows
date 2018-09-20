set(shaderDependencies
	# Add explicit dependencies here
)

set(shaders
	Shaders/Blit.vert
	Shaders/Combine.frag
	Shaders/Model.vert
	Shaders/Model.frag
	Shaders/RayTracedShadows.comp
)

set(src
	${shaders}
	BaseApplication.cpp
	BaseApplication.h
	BVHBuilder.cpp
	BVHBuilder.h
	MovingAverage.h
	RayTracedShadows.cpp
	RayTracedShadows.h
)

set(app RayTracedShadows)

add_executable(${app}
	${src}
)

source_group("Shaders" FILES ${shaders} ${shaderDependencies})

find_program(GLSLC NAMES glslc PATHS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VK_SDK_PATH}/Bin
	$ENV{PATH}
	"~/bin"
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	find_program(spirv-cross NAMES spirv-cross PATHS
		$ENV{VK_SDK_PATH}/Bin
		$ENV{PATH}
		"~/bin"
	)
	function(shader_compile_rule shaderName dependencies)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}/Shaders
			COMMAND ${GLSLC} -o ${CMAKE_CFG_INTDIR}/${shaderName}.spv ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${spirv-cross} --metal ${CMAKE_CFG_INTDIR}/${shaderName}.spv > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			DEPENDS ${dependencies}
		)
	endfunction(shader_compile_rule)
else()
	function(shader_compile_rule shaderName dependencies)
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.spv
			COMMAND ${GLSLC} -o ${CMAKE_CFG_INTDIR}/${shaderName}.spv ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			DEPENDS ${dependencies}
		)
	endfunction(shader_compile_rule)
endif()

foreach(shader ${shaders})
	shader_compile_rule(${shader} "${shaderDependencies}")
endforeach()

target_compile_definitions(${app} PRIVATE
	RUSH_USING_NAMESPACE # Automatically use Rush namespace
)

target_link_libraries(${app}
	Rush
	stb
	tiny_obj_loader
)
