# SphericalTiling

C++17 spherical tiling experiments centered on Goldberg class-I subdivision and dual-cell construction.

## Current Status

The default build is the geometric core plus a CLI demo. A lightweight GUI viewer is also available behind a CMake flag for visual inspection of the generated tilings, Earth-colored dual cells, and the first sphere-to-local-flat zoom morph.

## What Works

- Goldberg class-I subdivision of an icosahedron on a sphere
- Construction of primal adjacency rings and dual polygon cells
- CLI demo that reports mesh counts for a chosen radius and subdivision frequency
- GUI viewer that renders Earth-colored dual cells plus the icosahedron, subdivision, primal, and dual debug overlays

## Dependencies

### Default build

- CMake 3.15+
- C++17 compiler
- Eigen3 3.3+

### Optional modules

- GLFW, OpenGL, GLM, GLAD, and ImGui for the GUI viewer

## Building

### Ubuntu/Debian

```bash
sudo apt-get install cmake libeigen3-dev
mkdir build
cd build
cmake ..
cmake --build .
```

### macOS

```bash
brew install cmake eigen
mkdir build
cd build
cmake ..
cmake --build .
```

### Optional CMake switches

```bash
-DSPHERICAL_TILING_BUILD_GUI=ON
```

## Usage

Run the demo with defaults:

```bash
./bin/spherical_tiling_demo
```

Set the subdivision frequency:

```bash
./bin/spherical_tiling_demo --q 4
```

Set both frequency and radius:

```bash
./bin/spherical_tiling_demo --q 4 --radius 1.5
```

Build and run the GUI viewer:

```bash
cmake -S . -B build-gui -DSPHERICAL_TILING_BUILD_GUI=ON
cmake --build build-gui
./bin/spherical_tiling_gui
```

Viewer controls:

- Left-drag to orbit
- Mouse wheel to zoom
- Rebuild with a new radius or subdivision frequency
- Toggle Earth tiles, morphing, and the icosahedron/subdivision/primal/dual overlays

Show usage:

```bash
./bin/spherical_tiling_demo --help
```

The frequency parameter controls the fineness of the subdivision:

- `q=1`: 12 primal vertices, 20 primal faces
- `q=2`: 42 primal vertices, 80 primal faces
- `q=3`: 92 primal vertices, 180 primal faces
- `q=4`: 162 primal vertices, 320 primal faces

## Notes

- `MeshConstructor` now validates its inputs and rejects non-positive radii or frequencies.
- The repository includes `assets/earth/blue_marble_5400x2700_december.jpg` as the default Earth raster for tile coloring.
- Spherical polygon area helpers return unsigned area magnitudes so metric-style callers are not sensitive to polygon winding.
- Long-term rendering and world-data direction is documented in `WORLD_RENDERING_ROADMAP.md`.
- Historical GUI notes remain in `GUI_IMPLEMENTATION.md` and `FEATURE_SUMMARY.md`; they describe an older, more ambitious GUI direction than the current minimal viewer.

## License

See LICENSE file for details.
