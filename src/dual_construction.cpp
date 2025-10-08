#include "dual_construction.h"
#include <cmath>
#include <map>
#include <set>
#include <algorithm>

namespace spherical_tiling {

Eigen::Vector3d sphericalCircumcenter(
    const Eigen::Vector3d& p1,
    const Eigen::Vector3d& p2,
    const Eigen::Vector3d& p3,
    double radius
) {
    // Compute the circumcenter on a sphere
    // The circumcenter is perpendicular to the plane containing the three points
    
    // Get normals to the great circle arcs
    Eigen::Vector3d n1 = p1.cross(p2).normalized();
    Eigen::Vector3d n2 = p2.cross(p3).normalized();
    
    // The circumcenter is perpendicular to both normals
    Eigen::Vector3d center = n1.cross(n2).normalized();
    
    // Check which direction to use (should be on the same side as the triangle)
    Eigen::Vector3d midpoint = (p1 + p2 + p3).normalized();
    if (center.dot(midpoint) < 0) {
        center = -center;
    }
    
    return center * radius;
}

double sphericalPolygonArea(const std::vector<Eigen::Vector3d>& vertices, double radius) {
    if (vertices.size() < 3) return 0.0;
    
    // Use spherical excess formula: Area = R^2 * (sum of angles - (n-2)*π)
    // For a spherical polygon, we compute the sum of interior angles
    
    double angleSum = 0.0;
    int n = vertices.size();
    
    for (int i = 0; i < n; ++i) {
        const Eigen::Vector3d& prev = vertices[(i - 1 + n) % n];
        const Eigen::Vector3d& curr = vertices[i];
        const Eigen::Vector3d& next = vertices[(i + 1) % n];
        
        // Compute vectors from curr to prev and next
        Eigen::Vector3d v1 = (prev - curr).normalized();
        Eigen::Vector3d v2 = (next - curr).normalized();
        
        // Compute angle
        double cosAngle = v1.dot(v2);
        cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
        double angle = std::acos(cosAngle);
        
        angleSum += angle;
    }
    
    // Spherical excess
    double excess = angleSum - (n - 2) * M_PI;
    
    return radius * radius * excess;
}

double computePlanarAngleVariance(const std::vector<Eigen::Vector3d>& vertices) {
    if (vertices.size() < 3) return 0.0;
    
    int n = vertices.size();
    std::vector<double> angles;
    
    for (int i = 0; i < n; ++i) {
        const Eigen::Vector3d& prev = vertices[(i - 1 + n) % n];
        const Eigen::Vector3d& curr = vertices[i];
        const Eigen::Vector3d& next = vertices[(i + 1) % n];
        
        // Compute planar angle (treating vertices as 3D points in Euclidean space)
        Eigen::Vector3d v1 = (prev - curr).normalized();
        Eigen::Vector3d v2 = (next - curr).normalized();
        
        double cosAngle = v1.dot(v2);
        cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
        double angle = std::acos(cosAngle);
        
        angles.push_back(angle);
    }
    
    // Compute mean
    double mean = 0.0;
    for (double angle : angles) {
        mean += angle;
    }
    mean /= angles.size();
    
    // Compute variance
    double variance = 0.0;
    for (double angle : angles) {
        double diff = angle - mean;
        variance += diff * diff;
    }
    variance /= angles.size();
    
    return variance;
}

void constructDualCells(TileGraph& graph, double radius) {
    const auto& nodes = graph.getNodes();
    
    // For each node, compute dual cell using ordered neighbors
    // Each dual cell vertex is the circumcenter of a triangle formed by
    // the node and two consecutive neighbors
    
    for (size_t nodeIdx = 0; nodeIdx < nodes.size(); ++nodeIdx) {
        auto& node = graph.getNode(nodeIdx);
        const auto& neighbors = node.neighbors;
        
        if (neighbors.size() < 3) continue;
        
        std::vector<Eigen::Vector3d> dualVertices;
        
        // For each pair of consecutive neighbors, compute circumcenter
        // Neighbors are already ordered cyclically from buildFromTriangulation
        for (size_t i = 0; i < neighbors.size(); ++i) {
            int n1 = neighbors[i];
            int n2 = neighbors[(i + 1) % neighbors.size()];
            
            Eigen::Vector3d circumcenter = sphericalCircumcenter(
                node.center,
                nodes[n1].center,
                nodes[n2].center,
                radius
            );
            dualVertices.push_back(circumcenter);
        }
        
        // Compute area and angle variance for the dual cell
        if (dualVertices.size() >= 3) {
            node.area = std::abs(sphericalPolygonArea(dualVertices, radius));
            node.angle_variance = computePlanarAngleVariance(dualVertices);
        }
    }
}

} // namespace spherical_tiling
