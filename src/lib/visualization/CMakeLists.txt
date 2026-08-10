cmake_minimum_required(VERSION 3.15)

# GLAD is C
set(GLAD_SRC ${CMAKE_SOURCE_DIR}/third_party/glad/src/gl.c)
set_source_files_properties(${GLAD_SRC} PROPERTIES LANGUAGE C)

add_library(visualization
    src/camera.cpp
    src/earth_texture.cpp
    src/mesh_renderer.cpp
    ${GLAD_SRC}
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
)

# C++ standard & warnings
set_target_properties(visualization PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
if (MSVC)
  target_compile_options(visualization PRIVATE /W4 /permissive-)
else()
  target_compile_options(visualization PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Public includes for consumers
target_include_directories(visualization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/glad/include
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

# Dependencies
find_package(Eigen3 REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)   # needed on Windows/Linux for gl symbols

# ImGui backend expects GLAD by default
target_compile_definitions(visualization PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)

# Link order: glad before OpenGL can help on some toolchains
target_link_libraries(visualization PUBLIC
    geometric               # your model target (ensure it exists elsewhere)
    Eigen3::Eigen
    glfw
    glm::glm
    OpenGL::GL
    ${CMAKE_DL_LIBS}        # dlopen on Linux; harmless elsewhere
)
