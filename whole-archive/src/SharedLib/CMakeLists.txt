project(SharedLib)

add_library(${PROJECT_NAME} SHARED sharedLib.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC StandaloneStaticLib StaticLibWithChild)
