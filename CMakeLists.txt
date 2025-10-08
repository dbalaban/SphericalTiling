cmake_minimum_required(VERSION 3.15)
project(spherical_tiling VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Ceres REQUIRED)

# Add library subdirectories
add_subdirectory(src/lib/geometric)
add_subdirectory(src/lib/optimization)
add_subdirectory(src/lib/visualization)

# Add application subdirectory
add_subdirectory(src/app)
