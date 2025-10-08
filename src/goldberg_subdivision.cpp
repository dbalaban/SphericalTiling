#include "goldberg_subdivision.h"
#include <map>
#include <cmath>

namespace spherical_tiling {

std::vector<Eigen::Vector3d> generateIcosahedron(double radius) {
    // Golden ratio
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    
    // Unnormalized vertices of icosahedron
    std::vector<Eigen::Vector3d> vertices = {
        {-1,  phi,  0}, { 1,  phi,  0}, {-1, -phi,  0}, { 1, -phi,  0},
        { 0, -1,  phi}, { 0,  1,  phi}, { 0, -1, -phi}, { 0,  1, -phi},
        { phi,  0, -1}, { phi,  0,  1}, {-phi,  0, -1}, {-phi,  0,  1}
    };
    
    // Normalize to sphere of given radius
    for (auto& v : vertices) {
        v = v.normalized() * radius;
    }
    
    return vertices;
}

std::vector<Eigen::Vector3i> generateIcosahedronFaces() {
    // 20 faces of the icosahedron
    return {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };
}

SubdivisionResult goldbergSubdivision(
    const std::vector<Eigen::Vector3d>& icosahedronVertices,
    const std::vector<Eigen::Vector3i>& icosahedronFaces,
    int frequency,
    double radius
) {
    SubdivisionResult result;
    
    // Map to avoid duplicate vertices
    std::map<std::tuple<int, int, int>, int> vertexMap;
    
    // Helper to add or get vertex index
    auto getOrAddVertex = [&](const Eigen::Vector3d& v) -> int {
        // Normalize and scale to radius
        Eigen::Vector3d normalized = v.normalized() * radius;
        
        // Simple rounding for map key
        int x = static_cast<int>(std::round(normalized.x() * 1e6));
        int y = static_cast<int>(std::round(normalized.y() * 1e6));
        int z = static_cast<int>(std::round(normalized.z() * 1e6));
        
        auto key = std::make_tuple(x, y, z);
        auto it = vertexMap.find(key);
        if (it != vertexMap.end()) {
            return it->second;
        }
        
        int index = result.vertices.size();
        result.vertices.push_back(normalized);
        vertexMap[key] = index;
        return index;
    };
    
    // Subdivide each face
    for (const auto& face : icosahedronFaces) {
        const Eigen::Vector3d& v0 = icosahedronVertices[face[0]];
        const Eigen::Vector3d& v1 = icosahedronVertices[face[1]];
        const Eigen::Vector3d& v2 = icosahedronVertices[face[2]];
        
        // Create a grid of vertices on the triangle
        // Use barycentric coordinates
        std::vector<std::vector<int>> grid(frequency + 1);
        
        for (int i = 0; i <= frequency; ++i) {
            for (int j = 0; j <= frequency - i; ++j) {
                int k = frequency - i - j;
                
                // Barycentric coordinates
                double u = static_cast<double>(i) / frequency;
                double v = static_cast<double>(j) / frequency;
                double w = static_cast<double>(k) / frequency;
                
                Eigen::Vector3d pos = u * v0 + v * v1 + w * v2;
                int idx = getOrAddVertex(pos);
                grid[i].push_back(idx);
            }
        }
        
        // Create triangular faces from the grid
        for (int i = 0; i < frequency; ++i) {
            for (int j = 0; j < frequency - i; ++j) {
                // Lower triangle
                result.faces.push_back(Eigen::Vector3i(
                    grid[i][j],
                    grid[i + 1][j],
                    grid[i][j + 1]
                ));
                
                // Upper triangle (if not at edge)
                if (j < frequency - i - 1) {
                    result.faces.push_back(Eigen::Vector3i(
                        grid[i][j + 1],
                        grid[i + 1][j],
                        grid[i + 1][j + 1]
                    ));
                }
            }
        }
    }
    
    return result;
}

} // namespace spherical_tiling
