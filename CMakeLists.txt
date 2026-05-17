cmake_minimum_required(VERSION 3.16)
project(DrawGame VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
endif()

find_package(OpenGL REQUIRED)
include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)
FetchContent_Declare(glad GIT_REPOSITORY https://github.com/Dav1dde/glad.git GIT_TAG v0.1.36)
FetchContent_MakeAvailable(glfw glad)

add_executable(${PROJECT_NAME}
    main.cpp common/canvas.cpp server/server.cpp
    client/client.cpp client/renderer.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} OpenGL::GL glfw glad
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Linux>:pthread;dl;X11>
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME}
        "-framework Cocoa" "-framework IOKit"
        "-framework CoreVideo" "-framework CoreFoundation" pthread)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

# ============================================================
# Установка
# ============================================================

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
