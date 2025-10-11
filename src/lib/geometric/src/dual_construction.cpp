#include "dual_construction.h"
#include "templated_geometry.h"
#include <cmath>
#include <map>
#include <set>
#include <algorithm>

namespace spherical_tiling {

void constructDualCells(TileGraph& graph, double radius) {
    const auto& nodes = graph.getNodes();
    
    // For each node, compute dual cell using ordered neighbors
    // Each dual cell vertex is the circumcenter of a triangle formed by
    // the node and two consecutive neighbors
    
    for (size_t nodeIdx = 0; nodeIdx < nodes.size(); ++nodeIdx) {
        auto& node = graph.getNode(nodeIdx);
        const auto& neighbors = node.neighbors;
        
        if (neighbors.size() < 3) {
            continue;
        }
        
        std::vector<Eigen::Vector3d> dualVertices;
        
        // For each pair of consecutive neighbors, compute circumcenter
        // Neighbors are already ordered cyclically from buildFromTriangulation
        for (size_t i = 0; i < neighbors.size(); ++i) {
            int n1 = neighbors[i];
            int n2 = neighbors[(i + 1) % neighbors.size()];
            
            Eigen::Vector3d circumcenter = sphericalCircumcenterT<double>(
                node.center,
                nodes[n1].center,
                nodes[n2].center,
                radius
            );
            dualVertices.push_back(circumcenter);
        }
        
        // Compute area and angle variance for the dual cell
        if (dualVertices.size() >= 3) {
            double area = sphericalPolygonAreaT<double>(dualVertices, radius);
            node.area = std::abs(area);
            std::vector<double> angles = computeTangentSpaceAngles<double>(node.center, dualVertices, 1.0);
            node.angle_variance = computeAngleVariance<double>(angles);
        } else {
            node.area = 0.0;
            node.angle_variance = 0.0;
        }
    }
}

} // namespace spherical_tiling
