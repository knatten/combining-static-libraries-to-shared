cmake_minimum_required(VERSION 3.20)
project(whole_archive)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(StaticLibChild)
add_subdirectory(StaticLibWithChild)
add_subdirectory(SharedLib)
