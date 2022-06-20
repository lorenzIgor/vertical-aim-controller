cmake_minimum_required(VERSION 3.5)
project(SampleCPP)

set(CMAKE_CXX_STANDARD 14)

include_directories("C:\\Windows\\System32")
include_directories("C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)\\Lib\\x86")


add_executable(SampleCPP main.cpp)
target_link_libraries(SampleCPP dwmapi.dll d3d9.lib d3dx9.lib)
#add_library(SampleCPP main.cpp)
