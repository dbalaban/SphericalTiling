cmake_minimum_required(VERSION 3.15)

# Set output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

# CLI Demo executable
add_executable(demo
    demo.cpp
)
target_link_libraries(demo PRIVATE
    geometric
)

# Benchmark executable
# add_executable(spherical_tiling_benchmark
#     benchmark.cpp
# )
# target_link_libraries(spherical_tiling_benchmark PRIVATE
#     geometric
#     optimization
# )

# # GUI executable
# add_executable(spherical_tiling_gui
#     gui_main.cpp
# )
# target_link_libraries(spherical_tiling_gui PRIVATE
#     geometric
#     optimization
#     visualization
# )
