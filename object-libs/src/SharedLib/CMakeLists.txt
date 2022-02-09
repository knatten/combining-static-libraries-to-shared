project(SharedLib)

add_library(${PROJECT_NAME} SHARED sharedLib.cpp)
target_link_libraries(${PROJECT_NAME} StaticLib)
target_link_libraries(${PROJECT_NAME} ObjectLib)
