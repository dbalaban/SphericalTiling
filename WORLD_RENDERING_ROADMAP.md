# World Rendering Roadmap

## Summary

This document defines the intended long-term rendering and data architecture for SphericalTiling. The project direction is:

`world generation -> high-fidelity spherical world fields -> derived render assets -> runtime globe/close-view renderer`

The main design rule is that simulation data is the source of truth. The dual tile graph carries gameplay topology and per-cell state, but it is not the highest-resolution representation of terrain. Visual richness should come from dense continuous world fields and derived textures, masks, and shader inputs rather than from pushing the gameplay tile count to extreme values.

The immediate priority is not fantasy world generation yet. The immediate priority is to use the Earth test case to build the right rendering architecture: continuous Earth data as the base surface, dual cells as overlays and interaction regions, and a smooth transition between space view and close tactical/strategy view.

## Canonical Data Model

The runtime world should be split into three layers.

### 1. Tile Graph

This is the gameplay substrate.

It should hold:

- adjacency
- movement/pathfinding structure
- region ownership/control
- per-cell terrain summaries
- per-cell simulation summaries
- identifiers used for selection and interaction

It should not be the only place where terrain detail lives.

### 2. Spherical World Fields

These are the continuous high-fidelity descriptions of the world.

The project should support field sets such as:

- elevation
- ocean/land mask
- temperature
- precipitation / moisture
- runoff / flow accumulation
- erosion / ruggedness
- biome / landcover
- ice / snow
- optional soil, vegetation, sediment, wind, and climate bands

These fields may be raster-backed, procedural, or produced by offline simulation. The renderer and tile systems should not care how they were generated as long as they can be sampled on the sphere.

### 3. Derived Render Assets

These are rendering products, not simulation truth.

They should include:

- albedo / base color textures
- normal maps
- roughness or material masks
- biome blend masks
- river masks
- coastline masks
- snow/ice masks
- close-view stylization masks for roads, contour emphasis, terrain separation, and Paradox-like readability

These assets should be regenerable from the spherical world fields.

## Long-Term Fantasy World Pipeline

World generation remains intentionally general for now. The project should define the output contract of a world generator, not lock itself to one algorithm.

Any future generator should be able to emit a `WorldFieldSet` equivalent that provides:

- spherical elevation
- land/water mask
- climate fields
- hydrology fields
- biome or landcover classification

From that field set, the engine should derive both gameplay summaries and visual assets.

### Generator Output Contract

A compliant generator must be able to provide sampled values for:

- position on the sphere
- elevation
- whether the point is land or water
- temperature
- moisture / precipitation
- biome or terrain class

Optional but expected later:

- erosion
- river intensity
- drainage basins
- snow/ice coverage
- vegetation density

### Derived Gameplay Data

The tile graph should be populated by sampling or integrating over the world fields.

Examples:

- average elevation in the cell
- dominant biome
- passability
- water access
- river presence
- agricultural potential
- climate zone

### Derived Visual Data

The renderer should build assets from the same world fields.

Examples:

- global albedo texture
- normals from elevation
- coastline masks
- river overlays
- biome blend masks
- local material controls for close zoom

This is the mechanism that allows one world to support both a realistic view-from-space and a clean close-up strategy presentation.

## Immediate Next Steps: Earth Test Case

Earth is the first rendering reference pipeline. It is not the final product, but it is the right place to prove the architecture.

The current viewer already supports:

- a dual-cell spherical tiling
- Earth-colored cell visualization
- a first GPU-side globe-to-local-flat morph

The next rendering steps should move Earth from "cell-colored debug globe" to "continuous world surface with tile overlays."

### Earth Source Layers

Use Earth assets as layered inputs:

- Blue Marble albedo as the required first layer
- optional elevation map as the next terrain truth layer
- optional land/water mask
- optional bathymetry
- optional river/coastline datasets later

The current per-cell average color bake should remain available as a debug visualization mode, not the primary final Earth rendering mode.

### Immediate Rendering Goals

1. Add a dedicated textured Earth surface mesh.
   - One sphere/globe render layer should use the Earth albedo texture directly.
   - This becomes the primary surface in zoomed-out mode.

2. Keep dual cells as a separate overlay/interaction pass.
   - Tile outlines
   - Hover/select tinting
   - Debug topology display
   - Optional per-cell average-color mode

3. Preserve the current morph architecture.
   - Morphing remains visual-only.
   - Tile IDs, adjacency, and gameplay semantics never change during the transition.

4. Add zoom-dependent style blending.
   - Zoomed out: continuous Earth texture dominates.
   - Zoomed in: blend toward cleaner terrain separation and more readable tile structure.

5. Keep the GPU-first render path.
   - Static geometry uploads
   - Shader-side morphing
   - Minimal per-frame CPU work

### Immediate Implementation Order

Implement the Earth rendering work in this order:

1. Textured Earth base sphere
2. Dual overlay pass on top of textured Earth
3. Tile hover/select tinting without replacing the terrain surface
4. Zoom-based blend between globe presentation and close-view stylization
5. Better Earth data layers beyond Blue Marble

Do not use higher and higher `q` as the main route to visual richness.

## Runtime Rendering Strategy

The project should support one world with two visual regimes.

### Zoomed-Out Globe

- true spherical presentation
- continuous terrain textures
- optional lighting/material response
- cell overlays muted or hidden by default

### Zoomed-In Local View

- visually flatter local presentation
- stronger terrain separation and stylization
- tile overlays more prominent
- interaction feedback emphasized

The transition between these modes should be smooth and continuous. The same world data should drive both views.

The close-up "Paradox-like" look should be treated as a rendering style derived from world fields, not as a reason to increase the gameplay tile graph to absurd density.

## Recommended Engine Abstractions

The following conceptual interfaces should exist as the project grows:

- `WorldFieldSet`
  - sampled access to continuous world fields on the sphere
- `TileSampler`
  - derives gameplay summaries from world fields for each tile
- `RenderAssetBuilder`
  - builds textures, masks, and style inputs from world fields
- `TileOverlayRenderer`
  - renders borders, highlighting, ownership, and selection independently from the base terrain surface

The exact class names can change, but the responsibility split should remain.

## Practical Guidance

If a future change proposes more visible detail, prefer this order of solutions:

1. derive more render detail from fields and textures
2. improve shader-based close-view styling
3. add local overlays or decals
4. increase tile density only when gameplay needs it

Extreme `q` values should be considered experimental. The primary path to "dense-looking" worlds should be generated render detail, not a runaway increase in gameplay cells.

## Acceptance Criteria

This roadmap is realized when:

- terrain truth is clearly separated from gameplay topology
- Earth can be rendered as a continuous base surface independent of dual-cell coloring
- the same world data supports both space view and close-view map presentation
- future fantasy generators can plug in by producing world fields rather than bespoke renderer-specific assets
- visual density comes mostly from derived assets and shaders, not from extreme tile counts
