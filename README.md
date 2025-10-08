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

Run the demo with default parameters (radius=1.0, frequency=3):

```bash
./build/spherical_tiling_demo
```

Specify a custom subdivision frequency:

```bash
./build/spherical_tiling_demo 4
```

Specify a custom sphere radius:

```bash
./build/spherical_tiling_demo --R 2.5
```

Combine both radius and frequency:

```bash
./build/spherical_tiling_demo --R 2.5 4
```

Show help message:

```bash
./build/spherical_tiling_demo --help
```

### Parameters

- `--R <radius>`: Set the sphere radius (default: 1.0)
- `--help`: Display usage information
- `FREQUENCY`: Subdivision frequency (default: 3)

The frequency parameter controls the fineness of the subdivision:
- `frequency=1`: Original icosahedron (12 vertices, 20 faces)
- `frequency=2`: 42 vertices, 80 faces
- `frequency=3`: 92 vertices, 180 faces
- `frequency=4`: 162 vertices, 320 faces

## Example Output

```
Spherical Tiling Demo - Goldberg Subdivision with Optimization
==============================================================

Parameters:
  Sphere radius: 1
  Subdivision frequency: 4

[1/5] Generating icosahedron...
  Generated 12 vertices and 20 faces

[2/5] Performing Goldberg Class-I subdivision...
  Generated 162 vertices and 320 faces

[3/5] Building tile graph...
  Built graph with 162 nodes and 480 edges

[4/5] Constructing dual cells (Voronoi regions)...
  Computed dual cell areas and angle variances

[5/5] Running spatially-weighted optimization...
  Optimization complete

=== Tile Graph Statistics ===
Number of nodes: 162
Number of edges: 480

--- Area Statistics ---
Total area: 12.566371
Sphere area: 12.566371
Coverage: 100.000000%
Average area per node: 0.077570
Min area: 0.051369
Max area: 0.084717
Area variance: 42.990737%

--- Angle Statistics ---
Average angle defect: 0.077570 radians
Average angle variance: 0.012264
Min angle variance: 0.000000
Max angle variance: 0.028779

--- Energy Statistics ---
Average energy per node: 0.009156
Total energy: 1.483195

--- Sample Nodes ---
      ID    Latitude          Area      AngleVar        Energy  Neighbors
--------------------------------------------------------------------------
      68   90.000000      0.083145      0.006211      0.006211           6
      14    0.000000      0.051369      0.000000      0.026201           5
      83  -90.000000      0.083145      0.006211      0.006211           6
```

## Algorithm Details

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
├── include/
│   ├── goldberg_subdivision.h
│   ├── tile_graph.h
│   ├── dual_construction.h
│   └── optimization.h
└── src/
    ├── goldberg_subdivision.cpp
    ├── tile_graph.cpp
    ├── dual_construction.cpp
    ├── optimization.cpp
    └── main.cpp
```

## License

See LICENSE file for details.
