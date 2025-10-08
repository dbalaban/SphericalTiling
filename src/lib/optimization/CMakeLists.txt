cmake_minimum_required(VERSION 3.15)

# Optimization library
add_library(optimization
    src/optimization.cpp
)

target_include_directories(optimization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(optimization PUBLIC
    geometric
    Eigen3::Eigen
    Ceres::ceres
)
