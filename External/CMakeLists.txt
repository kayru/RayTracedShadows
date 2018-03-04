add_subdirectory("librush")

if (MSVC)
	add_compile_options(-W0)
else()
	add_compile_options(-w)
endif()

add_subdirectory("stb")
add_subdirectory("tiny_obj_loader")
