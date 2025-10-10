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

**Note**: This is fundamentally different from the dual mesh. The triangular mesh has many more, smaller triangles, while the dual mesh has fewer, larger polygons (pentagons and hexagons). Before the fix, "Show Dual" was incorrectly showing the same edges as "Show Primal" instead of the dual cell boundaries.

## Visual Comparison

When all three options are enabled:
- **Red edges** (Primal): Connect centers of adjacent pentagons/hexagons
- **Black edges** (Dual): Form the boundaries of pentagons/hexagons
- **Green edges** (Triangles): Form the triangular mesh underlying everything

The dual cell boundaries (black) should form closed pentagons and hexagons, while the primal graph (red) shows how these cells are connected to each other.

## Answer to Issue Question: "How/why is Show Triangle different from Show Dual?"

**Show Triangles** and **Show Dual** display completely different geometric structures:

### Show Triangles (Green)
- **What**: Edges of small triangles from Goldberg subdivision
- **Count**: For frequency=2, there are 80 triangular faces
- **Size**: Many small triangles uniformly distributed
- **Purpose**: Shows the base tessellation used to construct dual cells

### Show Dual (Black)
- **What**: Edges of large pentagons and hexagons (dual cells)
- **Count**: For frequency=2, there are 42 dual cells (12 pentagons + 30 hexagons)
- **Size**: Fewer, larger polygons
- **Purpose**: Shows the actual Goldberg polyhedron structure

**Key difference**: Triangles are the primal tessellation (many small faces), while dual cells are the Voronoi regions around primal vertices (fewer large faces). Each dual cell contains multiple triangles from the primal tessellation.

**Before the fix**, "Show Dual" was incorrectly showing TileGraph edges (same as Primal), which is why it looked like triangles - but those were not the actual triangle edges either, they were the adjacency graph edges.

## Fix Applied

The bug was that both "Show Primal" and "Show Dual" were displaying the same thing (TileGraph edges). The fix modified `MeshRenderer::setDualMesh()` to correctly compute and display the dual cell boundaries instead of reusing the TileGraph edges.

### Before the fix:
- Show Primal: TileGraph edges ❌ (but correct visualization)
- Show Dual: TileGraph edges (same as Primal) ❌ (incorrect - should show dual cell boundaries)
- Show Triangles: Triangle edges ✓

### After the fix:
- Show Primal: TileGraph edges (adjacency of dual cells) ✓
- Show Dual: Dual cell boundaries (pentagon/hexagon edges) ✓
- Show Triangles: Triangle edges ✓
