cmake_minimum_required(VERSION 3.15)

# Set GLAD source file as C language explicitly
set(GLAD_SRC ${CMAKE_SOURCE_DIR}/third_party/glad/src/gl.c)
set_source_files_properties(${GLAD_SRC} PROPERTIES LANGUAGE C)

# Visualization library
add_library(visualization
    src/camera.cpp
    src/mesh_renderer.cpp
    ${GLAD_SRC}
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(visualization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/glad/include
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

find_package(glfw3 REQUIRED)

target_link_libraries(visualization PUBLIC
    geometric
    Eigen3::Eigen
    glfw
    ${CMAKE_DL_LIBS}
)
