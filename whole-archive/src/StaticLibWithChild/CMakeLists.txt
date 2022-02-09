project(StaticLibWithChild)

add_library(${PROJECT_NAME} STATIC staticLibWithChild1.cpp staticLibWithChild2.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PUBLIC StaticLibChild)