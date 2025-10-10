#pragma once

#include "tile_graph.h"
#include <Eigen/Dense>

namespace spherical_tiling {

// Compute spherical circumcenter of a triangle on a sphere
Eigen::Vector3d sphericalCircumcenter(
    const Eigen::Vector3d& p1,
    const Eigen::Vector3d& p2,
    const Eigen::Vector3d& p3,
    double radius
);

// Construct dual cells (Voronoi regions) for each node
// Computes dual cell vertices (circumcenters), areas, and planar angle variance
void constructDualCells(TileGraph& graph, double radius);

// Compute the area of a spherical polygon
double sphericalPolygonArea(const std::vector<Eigen::Vector3d>& vertices, double radius);

// Compute planar angle variance for a polygon in tangent space at center
double computePlanarAngleVariance(const std::vector<Eigen::Vector3d>& vertices, const Eigen::Vector3d& center);

} // namespace spherical_tiling
