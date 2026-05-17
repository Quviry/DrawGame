cmake_minimum_required(VERSION 3.16)
project(DrawGame VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# На macOS статическая линковка проще
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    # Универсальный бинарник (x86_64 + arm64)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures")
endif()

# ============================================================
# Зависимости
# ============================================================

find_package(OpenGL REQUIRED)

include(FetchContent)

# GLFW - статическая сборка
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
FetchContent_MakeAvailable(glfw)

# GLAD
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

# ============================================================
# Исполняемый файл
# ============================================================

set(SOURCES
    main.cpp
    common/canvas.cpp
    server/server.cpp
    client/client.cpp
    client/renderer.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})

# ============================================================
# Линковка по платформам
# ============================================================

if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        OpenGL::GL
        glfw
        glad
        ws2_32
    )
    # Статическая линковка MSVC runtime
    if(MSVC)
        set_property(TARGET ${PROJECT_NAME} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        # Убираем консольное окно в Release (опционально — оставляем для логов)
        # set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()

    # MinGW статическая линковка
    if(MINGW)
        target_link_options(${PROJECT_NAME} PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

elseif(APPLE)
    target_link_libraries(${PROJECT_NAME}
        OpenGL::GL
        glfw
        glad
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreFoundation"
        pthread
    )

else() # Linux
    target_link_libraries(${PROJECT_NAME}
        OpenGL::GL
        glfw
        glad
        pthread
        dl
        X11
    )
    # Статическая линковка libstdc++ и libgcc для переносимости
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()

# ============================================================
# Установка
# ============================================================

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
