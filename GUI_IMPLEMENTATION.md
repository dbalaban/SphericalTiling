# GUI Viewer Implementation Guide

## Overview

The GUI viewer provides interactive 3D visualization of spherical tilings using OpenGL, GLFW, and ImGui.

## Architecture

### Libraries

1. **GLFW**: Window management and input handling
2. **GLAD**: OpenGL function loader (core profile 3.3)
3. **ImGui**: Immediate mode GUI for controls and dialogs

### Visualization Library Components

#### Camera (`camera.h`, `camera.cpp`)

Implements a spherical camera controller that always looks at the origin (center of the sphere).

- **Position**: Stored in spherical coordinates (distance, yaw, pitch)
- **View Matrix**: Computed using look-at transformation
- **Projection**: Perspective projection with configurable FOV
- **Controls**:
  - `rotate(deltaYaw, deltaPitch)`: Rotate camera around sphere
  - `zoom(deltaDistance)`: Move camera closer/farther from sphere
  - `reset()`: Return to default position

#### Mesh Renderer (`mesh_renderer.h`, `mesh_renderer.cpp`)

Handles rendering of both primal (triangular) and dual (Voronoi) meshes.

**Primal Mesh**:
- Input: Vertex positions and triangle faces
- Rendering: Wireframe (edges only) in configurable color
- Data: Vertices stored as float array, edges extracted from faces

**Dual Mesh**:
- Input: TileGraph with node positions and edge connectivity
- Rendering: Wireframe (edges only) in configurable color
- Data: Vertices from node centers, edges from graph edges

**Shader Pipeline**:
- Simple vertex shader: Transforms positions by MVP matrix
- Simple fragment shader: Outputs uniform color
- No lighting or advanced effects (wireframe visualization)

### GUI Application (`gui_main.cpp`)

Main application that integrates all components.

#### Application State

```cpp
struct AppState {
    // Sphere parameters
    double radius;
    int frequency;
    WeightFunction weightFunc;
    bool runOptimization;
    
    // Mesh data
    std::vector<Eigen::Vector3d> primalVertices;
    std::vector<Eigen::Vector3i> primalFaces;
    std::unique_ptr<TileGraph> graph;
    
    // Display options
    bool showPrimal;
    bool showDual;
    
    // UI state
    bool showCreateDialog;
    bool needsRebuild;
};
```

#### Event Handling

- **Mouse Button**: Left-click starts camera rotation
- **Mouse Move**: Dragging rotates camera
- **Mouse Wheel**: Zooms camera in/out
- **ImGui Integration**: Checks `io.WantCaptureMouse` to avoid conflicts

#### UI Components

1. **Main Menu Bar**:
   - Sphere > Create New Sphere
   - Sphere > Reset Camera
   - Sphere > Exit

2. **Display Options Panel**:
   - Checkboxes for primal/dual visibility
   - Camera information display

3. **Sphere Info Panel**:
   - Current parameters
   - Mesh statistics

4. **Create Sphere Dialog**:
   - Q-frequency slider (1-8)
   - Optimization toggle
   - Weight function selection
   - Create/Cancel buttons

#### Rendering Loop

```
1. Poll events (GLFW)
2. Rebuild sphere if needed
3. Update camera projection
4. Clear framebuffer
5. Render primal mesh (if enabled)
6. Render dual mesh (if enabled)
7. Render ImGui UI
8. Swap buffers
```

## Building Spheres

The `buildSphere()` function recreates the mesh:

1. Generate base icosahedron
2. Perform Goldberg subdivision
3. Build TileGraph
4. Construct dual cells
5. Run optimization (if enabled)
6. Update renderer with new mesh data

Creating a new sphere **overwrites** the current sphere (as specified in requirements).

## Color Scheme

- **Background**: Dark blue-gray (0.2, 0.2, 0.25)
- **Primal Mesh**: Red (1.0, 0.0, 0.0)
- **Dual Mesh**: Black (0.0, 0.0, 0.0)

## Dependencies Setup

Third-party libraries are not included in the repository. Users must run `./setup_deps.sh` to:
1. Clone ImGui v1.90.1 from GitHub
2. Generate GLAD files using glad2 Python package

This keeps the repository size small and allows flexibility in library versions.

## Known Limitations

1. **Headless Mode**: GUI requires a display server and will crash in headless environments
2. **OpenGL Version**: Requires OpenGL 3.3 or higher
3. **Single Sphere**: Only one sphere can be visualized at a time (per requirements)
4. **No Save Feature**: Currently no ability to save/load spheres (noted for future work)

## Future Enhancements

Potential improvements mentioned in requirements:
- Add save/load functionality for spheres
- Allow multiple spheres to be managed
- Export mesh data to standard formats
- Add more visualization options (filled polygons, vertex markers, etc.)
