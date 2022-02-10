project(SharedLib)

add_library(${PROJECT_NAME} SHARED sharedLib.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
        -Wl,--whole-archive
        StaticLibWithChild
        $<TARGET_PROPERTY:StaticLibWithChild,INTERFACE_LINK_LIBRARIES>
        -Wl,--no-whole-archive
        )

target_link_libraries(${PROJECT_NAME} PUBLIC StaticLibWithChild)

