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
    if (center.dot(p1) < 0) {
        center = -center;
    }
    
    return center * radius;
}

double sphericalPolygonArea(const std::vector<Eigen::Vector3d>& vertices, double radius) {
    if (vertices.size() < 3) return 0.0;
    
    // Compute solid angle using Oosterom and Strackee method
    // This is more numerically stable than L'Huilier
    
    double solidAngle = 0.0;
    int n = vertices.size();
    
    // Normalize all vertices to unit sphere
    std::vector<Eigen::Vector3d> v;
    for (const auto& vert : vertices) {
        v.push_back(vert.normalized());
    }
    
    // Sum over triangular fans from origin
    for (int i = 0; i < n; ++i) {
        const Eigen::Vector3d& a = v[i];
        const Eigen::Vector3d& b = v[(i + 1) % n];
        
        // Contribution from edge (a, b)
        double numerator = a.cross(b).dot(Eigen::Vector3d(0, 0, 1)); // any reference vector
        double denominator = 1.0 + a.dot(Eigen::Vector3d(0, 0, 1)) + b.dot(Eigen::Vector3d(0, 0, 1)) + a.dot(b);
        
        // Actually, use simpler formula
        // For a polygon, we can use sum of dihedral angles
    }
    
    // Better approach: Use the fact that the solid angle is the spherical excess
    // Triangulate from first vertex and sum solid angles
    double area = 0.0;
    
    for (int i = 1; i < n - 1; ++i) {
        // Triangle with vertices 0, i, i+1
        Eigen::Vector3d a = v[0];
        Eigen::Vector3d b = v[i];
        Eigen::Vector3d c = v[i + 1];
        
        // Use formula: tan(E/4) = sqrt(tan(s/2) * tan((s-a)/2) * tan((s-b)/2) * tan((s-c)/2))
        // where a, b, c are the side lengths and s = (a+b+c)/2
        
        double la = std::acos(std::max(-1.0, std::min(1.0, b.dot(c))));
        double lb = std::acos(std::max(-1.0, std::min(1.0, a.dot(c))));
        double lc = std::acos(std::max(-1.0, std::min(1.0, a.dot(b))));
        
        double s = (la + lb + lc) / 2.0;
        
        // Check for degenerate triangle
        if (s < 1e-10 || s - la < 1e-10 || s - lb < 1e-10 || s - lc < 1e-10) {
            continue;
        }
        
        double tan_e4 = std::sqrt(
            std::abs(std::tan(s/2) * std::tan((s-la)/2) * std::tan((s-lb)/2) * std::tan((s-lc)/2))
        );
        
        double excess = 4.0 * std::atan(tan_e4);
        
        // Check winding order
        if (a.dot(b.cross(c)) < 0) {
            excess = -excess;
        }
        
        area += excess;
    }
    
    return area * radius * radius;
}

double computePlanarAngleVariance(const std::vector<Eigen::Vector3d>& vertices, const Eigen::Vector3d& center) {
    if (vertices.size() < 3) return 0.0;
    
    // Normal at the center (for tangent plane)
    Eigen::Vector3d normal = center.normalized();
    
    int n = vertices.size();
    std::vector<double> angles;
    
    for (int i = 0; i < n; ++i) {
        const Eigen::Vector3d& prev = vertices[(i - 1 + n) % n];
        const Eigen::Vector3d& curr = vertices[i];
        const Eigen::Vector3d& next = vertices[(i + 1) % n];
        
        // Project to tangent plane at center
        Eigen::Vector3d v1 = prev - curr;
        Eigen::Vector3d v2 = next - curr;
        
        // Project onto tangent plane at center
        v1 = v1 - (v1.dot(normal)) * normal;
        v2 = v2 - (v2.dot(normal)) * normal;
        
        v1 = v1.normalized();
        v2 = v2.normalized();
        
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
        
        if (neighbors.size() < 3) {
            continue;
        }
        
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
            double area = sphericalPolygonArea(dualVertices, radius);
            node.area = std::abs(area);
            node.angle_variance = computePlanarAngleVariance(dualVertices, node.center);
        } else {
            node.area = 0.0;
            node.angle_variance = 0.0;
        }
    }
}

} // namespace spherical_tiling
