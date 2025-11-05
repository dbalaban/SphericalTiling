cmake_minimum_required(VERSION 3.15)
project(spherical_tiling VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler flags for Debug and Release modes
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Debug flags: -g for debug symbols, -O0 for no optimization
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    # Release flags: -O3 for optimization, -DNDEBUG to disable asserts
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    # Add common warning flags
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    # MSVC Debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /DDEBUG")
    # MSVC Release flags
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Find required packages
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Ceres REQUIRED)

# Add library subdirectories
add_subdirectory(src/lib/geometric)
# add_subdirectory(src/lib/optimization)
# add_subdirectory(src/lib/visualization)

# Add application subdirectory
add_subdirectory(src/app)
