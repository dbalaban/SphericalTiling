cmake_minimum_required(VERSION 3.15)

# Set output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)

# CLI Demo executable
add_executable(spherical_tiling_demo
    demo.cpp
)
target_link_libraries(spherical_tiling_demo PRIVATE
    geometric
)

if(SPHERICAL_TILING_BUILD_GUI)
    add_executable(spherical_tiling_gui
        gui_main.cpp
    )
    target_link_libraries(spherical_tiling_gui PRIVATE
        geometric
        visualization
    )
endif()
