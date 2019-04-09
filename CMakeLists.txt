cmake_minimum_required(VERSION 3.6)
project(RGB_Processing)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp)
add_executable(RGB_Processing ${SOURCE_FILES})
target_link_libraries(RGB_Processing tbb freeimageplus)