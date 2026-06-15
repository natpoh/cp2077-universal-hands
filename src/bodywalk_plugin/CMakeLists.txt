cmake_minimum_required(VERSION 3.20)
project(cp2077_bridge)

set(CMAKE_CXX_STANDARD 17)

add_library(cp2077_bridge SHARED plugin_main.cpp)

set_target_properties(cp2077_bridge PROPERTIES PREFIX "")
