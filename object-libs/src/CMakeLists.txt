cmake_minimum_required(VERSION 3.20)
project(object_libs)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(StaticLib)
add_subdirectory(ObjectLib)
add_subdirectory(Main)
add_subdirectory(SharedLib)
