add_library(tiny_obj_loader STATIC tiny_obj_loader.cc)
target_include_directories(tiny_obj_loader INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
