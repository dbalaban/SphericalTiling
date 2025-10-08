cmake_minimum_required(VERSION 3.15)

# Geometric library
add_library(geometric
    src/goldberg_subdivision.cpp
    src/tile_graph.cpp
    src/dual_construction.cpp
)

target_include_directories(geometric PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(geometric PUBLIC
    Eigen3::Eigen
)
