add_library(zeux_objparser STATIC objparser.h objparser.cpp)
target_include_directories(zeux_objparser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
