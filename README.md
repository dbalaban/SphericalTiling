# SphericalTiling

A C++17 library for geometric tilings of spheres using Goldberg subdivision and optimization.

## Features

- **Goldberg Class-I Subdivision**: Subdivides an icosahedron into a triangular mesh on a sphere
- **TileGraph Construction**: Builds a graph representation with nodes, edges, and neighbor relationships
- **Dual Cell Computation**: Constructs Voronoi regions using spherical circumcenters
- **Geometric Properties**: Computes per-node angle defects, areas, and angle variances
- **Spatially-Weighted Optimization**: Uses Ceres optimizer to balance equal-area and equal-angle constraints based on latitude
- **CLI Demo**: Interactive demonstration with detailed statistics
- **GUI Viewer**: 3D visualization with interactive camera controls and parameter configuration

## Dependencies

### Core Dependencies
- CMake 3.15+
- C++17 compiler (GCC, Clang, or MSVC)
- Eigen3 3.3+
- Ceres Solver 2.0+

### GUI Dependencies (for spherical_tiling_gui)
- GLFW 3.x
- OpenGL 3.3+
- GLAD (included in third_party/)
- ImGui (included in third_party/)

## Building

### Setup Third-Party Dependencies

Before building, you need to set up the third-party dependencies (GLAD and ImGui). Run the provided setup script:

```bash
./setup_deps.sh
```

This script will:
- Clone ImGui v1.90.1
- Generate GLAD OpenGL loader files

### On Ubuntu/Debian

```bash
# Install dependencies (CLI + GUI)
sudo apt-get install cmake libeigen3-dev libceres-dev libglfw3-dev

# Build the project (Release mode by default)
mkdir build && cd build
cmake ..
cmake --build .
```

To build in Debug mode with debugging symbols:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

To debug with gdb:

```bash
gdb ./bin/spherical_tiling_gui
# In gdb:
# run             - Start the program
# bt              - Show backtrace after crash
# break <func>    - Set breakpoint
# continue        - Continue execution
# print <var>     - Print variable value
```

### On macOS

```bash
# Install dependencies (CLI + GUI)
brew install cmake eigen ceres-solver glfw

# Build the project (Release mode by default)
mkdir build && cd build
cmake ..
cmake --build .
```

To build in Debug mode:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

To debug with lldb:

```bash
lldb ./bin/spherical_tiling_gui
# In lldb:
# run             - Start the program
# bt              - Show backtrace after crash
# breakpoint set --name <func> - Set breakpoint
# continue        - Continue execution
# frame variable  - Print variables in current frame
```

## Usage

### CLI Demo

Run the demo with default parameters (frequency=3):

```bash
./bin/spherical_tiling_demo
```

Specify a custom subdivision frequency:

```bash
./bin/spherical_tiling_demo --q 4
```

The frequency parameter controls the fineness of the subdivision:
- `frequency=1`: Icosahedron (12 vertices, 20 faces)
- `frequency=2`: 42 vertices, 80 faces
- `frequency=3`: 92 vertices, 180 faces
- `frequency=4`: 162 vertices, 320 faces

### GUI Viewer

Run the 3D graphical viewer:

```bash
./bin/spherical_tiling_gui
```

**Note**: The GUI requires a display server (X11, Wayland, or macOS window system). It will not run in headless environments.

Features:
- **Interactive 3D View**: Rotate camera by left-click and drag, zoom with mouse wheel
- **Create New Sphere**: Menu > Sphere > Create New Sphere to configure parameters
  - Q-frequency (1-8): Controls mesh subdivision density
  - Optimization toggle: Enable/disable vertex optimization
  - Weight functions: Choose optimization strategy (f1-f6)
- **Display Options**: Toggle visibility of primal mesh (red) and dual mesh (black)
- **Statistics**: View real-time mesh and sphere statistics

## Algorithm Details

### Icosahedron Rotation

The algorithm constructs a base icosahedron, with 12 vertices in a 1-5-5-1 latitude arangement; meaning 1 at the north pole, five others in the northern hemesphere, and symmetrical across the equator.  

### Goldberg Subdivision

The algorithm subdivides each triangular face of the icosahedron into `frequency²` smaller triangles using barycentric coordinates. Each new vertex is projected onto the sphere of radius R.

### Dual Cell Construction

For each vertex in the primal triangulation:
1. Find all neighboring vertices in cyclic order
2. For each pair of consecutive neighbors, compute the spherical circumcenter of the triangle formed by the vertex and its two neighbors
3. These circumcenters form the vertices of the dual Voronoi cell

### Optimization

The Ceres optimizer minimizes a weighted energy function:
- Near the equator: Emphasizes equal-area constraints
- Near the poles: Emphasizes equal-angle constraints
- Smooth interpolation based on latitude

This helps create more uniform tilings that balance area and angle regularity based on position.

## Project Structure

```
SphericalTiling/
├── CMakeLists.txt
├── README.md
├── bin/                            # Compiled executables
│   ├── spherical_tiling_demo
│   ├── spherical_tiling_benchmark
│   └── spherical_tiling_gui
├── third_party/                    # Third-party libraries
│   ├── glad/                       # OpenGL loader (generated)
│   └── imgui/                      # Dear ImGui (cloned)
└── src/
    ├── app/                        # Application executables
    │   ├── CMakeLists.txt
    │   ├── main.cpp                # CLI demo
    │   ├── benchmark.cpp           # Benchmark tool
    │   └── gui_main.cpp            # GUI application
    └── lib/                        # Project libraries
        ├── geometric/              # Geometric library
        │   ├── CMakeLists.txt
        │   ├── include/
        │   │   ├── goldberg_subdivision.h
        │   │   ├── tile_graph.h
        │   │   ├── dual_construction.h
        │   │   └── templated_geometry.h
        │   └── src/
        │       ├── goldberg_subdivision.cpp
        │       ├── tile_graph.cpp
        │       └── dual_construction.cpp
        ├── optimization/           # Optimization library
        │   ├── CMakeLists.txt
        │   ├── include/
        │   │   └── optimization.h
        │   └── src/
        │       └── optimization.cpp
        └── visualization/          # Visualization library
            ├── CMakeLists.txt
            ├── include/
            │   ├── camera.h
            │   └── mesh_renderer.h
            └── src/
                ├── camera.cpp
                └── mesh_renderer.cpp
```

## License

See LICENSE file for details.
