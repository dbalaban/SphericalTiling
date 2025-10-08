#pragma once

#include "tile_graph.h"

namespace spherical_tiling {

// Spatially weighted Ceres optimization that interpolates between equal-area 
// and equal-angles criteria based on latitude (poles vs equator)
void optimizeTileGraph(TileGraph& graph, double radius);

} // namespace spherical_tiling
