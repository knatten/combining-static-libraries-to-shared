project(StandaloneStaticLib)

add_library(${PROJECT_NAME} STATIC standaloneStaticLib1.cpp standaloneStaticLib2.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
