# GUI Mesh Options Explanation

This document explains what each mesh visualization option displays in the GUI.

## Background: Goldberg Polyhedron and Dual Construction

The spherical tiling creates a **Goldberg polyhedron** on the sphere, which consists of:
- **Dual cells**: Pentagon and hexagon faces (12 pentagons + varying number of hexagons)
- **Primal mesh**: The underlying triangular subdivision
- **Dual-Primal relationship**: The dual cells are Voronoi regions around vertices of the primal triangulation

## Mesh Visualization Options

### Show Primal (Red)
**What it displays**: The adjacency graph of dual cells (TileGraph edges)

This shows edges connecting the centers of adjacent pentagon/hexagon dual cells. Each edge represents a shared boundary between two dual cells. This is the "primal" graph in the dual-primal relationship.

**Technical details**:
- Vertices: Centers of dual cells (Node positions in TileGraph)
- Edges: Connections between adjacent dual cells (TileGraph edges)
- Color: Red
- Data source: `TileGraph::getEdges()`

### Show Dual (Black)
**What it displays**: The boundaries of dual cells (pentagon/hexagon edges)

This shows the actual edges of the pentagons and hexagons that tile the sphere. Each dual cell is a Voronoi region, and its boundary consists of edges connecting circumcenters of adjacent triangles from the primal triangulation.

**Technical details**:
- Vertices: Circumcenters of triangles formed by each node and pairs of consecutive neighbors
- Edges: Lines connecting consecutive circumcenters around each node, forming closed polygons
- Color: Black
- Computation: For each node, compute circumcenters and connect them in cyclic order

### Show Triangles (Green)
**What it displays**: The edges of the triangular subdivision

This shows the underlying triangular mesh that results from the Goldberg subdivision of the icosahedron. These triangles are the basis for computing the dual cells.

**Technical details**:
- Vertices: Points on the sphere from the Goldberg subdivision
- Edges: Edges of the triangular faces
- Color: Green
- Data source: `subdivision.vertices` and `subdivision.faces`

## Visual Comparison

When all three options are enabled:
- **Red edges** (Primal): Connect centers of adjacent pentagons/hexagons
- **Black edges** (Dual): Form the boundaries of pentagons/hexagons
- **Green edges** (Triangles): Form the triangular mesh underlying everything

The dual cell boundaries (black) should form closed pentagons and hexagons, while the primal graph (red) shows how these cells are connected to each other.

## Fix Applied

The bug was that both "Show Primal" and "Show Dual" were displaying the same thing (TileGraph edges). The fix modified `MeshRenderer::setDualMesh()` to correctly compute and display the dual cell boundaries instead of reusing the TileGraph edges.

### Before the fix:
- Show Primal: TileGraph edges ❌
- Show Dual: TileGraph edges (same as Primal) ❌
- Show Triangles: Triangle edges ✓

### After the fix:
- Show Primal: TileGraph edges (adjacency of dual cells) ✓
- Show Dual: Dual cell boundaries (pentagon/hexagon edges) ✓
- Show Triangles: Triangle edges ✓
