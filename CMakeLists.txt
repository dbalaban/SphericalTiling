cmake_minimum_required(VERSION 3.15)
project(spherical_tiling VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Ceres REQUIRED)

# Library
add_library(spherical_tiling_lib
    src/goldberg_subdivision.cpp
    src/tile_graph.cpp
    src/dual_construction.cpp
    src/optimization.cpp
)

target_include_directories(spherical_tiling_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(spherical_tiling_lib PUBLIC
    Eigen3::Eigen
    Ceres::ceres
)

# CLI Demo executable
add_executable(spherical_tiling_demo
    src/main.cpp
)

target_link_libraries(spherical_tiling_demo PRIVATE
    spherical_tiling_lib
)

# Benchmark executable
add_executable(spherical_tiling_benchmark
    src/benchmark.cpp
)

target_link_libraries(spherical_tiling_benchmark PRIVATE
    spherical_tiling_lib
)
