# SphericalTiling

A C++17 library for geometric tilings of spheres using Goldberg subdivision and optimization.

## Features

- **Goldberg Class-I Subdivision**: Subdivides an icosahedron into a triangular mesh on a sphere
- **TileGraph Construction**: Builds a graph representation with nodes, edges, and neighbor relationships
- **Dual Cell Computation**: Constructs Voronoi regions using spherical circumcenters
- **Geometric Properties**: Computes per-node angle defects, areas, and angle variances
- **Spatially-Weighted Optimization**: Uses Ceres optimizer to balance equal-area and equal-angle constraints based on latitude
- **CLI Demo**: Interactive demonstration with detailed statistics

## Dependencies

- CMake 3.15+
- C++17 compiler (GCC, Clang, or MSVC)
- Eigen3 3.3+
- Ceres Solver 2.0+

## Building

### On Ubuntu/Debian

```bash
# Install dependencies
sudo apt-get install cmake libeigen3-dev libceres-dev

# Build the project
mkdir build && cd build
cmake ..
cmake --build .
```

### On macOS

```bash
# Install dependencies
brew install cmake eigen ceres-solver

# Build the project
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

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
│   └── spherical_tiling_benchmark
└── src/
    ├── app/                        # Application executables
    │   ├── CMakeLists.txt
    │   ├── main.cpp
    │   └── benchmark.cpp
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
        └── optimization/           # Optimization library
            ├── CMakeLists.txt
            ├── include/
            │   └── optimization.h
            └── src/
                └── optimization.cpp
```

## License

See LICENSE file for details.
