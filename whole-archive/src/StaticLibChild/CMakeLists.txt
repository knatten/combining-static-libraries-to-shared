project(StaticLibChild)

add_library(${PROJECT_NAME} STATIC staticLibChild1.cpp staticLibChild2.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)