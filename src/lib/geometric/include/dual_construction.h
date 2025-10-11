#pragma once

#include "tile_graph.h"
#include <Eigen/Dense>

namespace spherical_tiling {

// Construct dual cells (Voronoi regions) for each node
// Computes dual cell vertices (circumcenters), areas, and planar angle variance
void constructDualCells(TileGraph& graph, double radius);

} // namespace spherical_tiling
