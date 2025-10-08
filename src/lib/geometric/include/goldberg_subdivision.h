#pragma once

#include <vector>
#include <Eigen/Dense>

namespace spherical_tiling {

// Generate vertices of a regular icosahedron on a sphere of radius R
std::vector<Eigen::Vector3d> generateIcosahedron(double radius);

// Generate triangular faces of the icosahedron (as triplets of vertex indices)
std::vector<Eigen::Vector3i> generateIcosahedronFaces();

// Class-I Goldberg subdivision: subdivide each triangular face into frequency^2 smaller triangles
// Returns the new vertex positions and triangular faces
struct SubdivisionResult {
    std::vector<Eigen::Vector3d> vertices;
    std::vector<Eigen::Vector3i> faces;
};

SubdivisionResult goldbergSubdivision(
    const std::vector<Eigen::Vector3d>& icosahedronVertices,
    const std::vector<Eigen::Vector3i>& icosahedronFaces,
    int frequency,
    double radius
);

} // namespace spherical_tiling
