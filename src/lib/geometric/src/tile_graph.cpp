#include "tile_graph.h"
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

namespace spherical_tiling {

void TileGraph::buildFromTriangulation(
    const std::vector<Eigen::Vector3d>& vertices,
    const std::vector<Eigen::Vector3i>& faces,
    double radius
) {
    radius_ = radius;
    
    // Initialize nodes from vertices
    nodes_.clear();
    nodes_.resize(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        nodes_[i].id = i;
        nodes_[i].center = vertices[i];
        
        // Initialize latitude-longitude parametrization from center
        double x = vertices[i].x();
        double y = vertices[i].y();
        double z = vertices[i].z();
        double r = vertices[i].norm();
        
        nodes_[i].latitude = std::asin(z / r);  // -π/2 to π/2
        nodes_[i].longitude = std::atan2(y, x);  // -π to π
        
        nodes_[i].energy = 0.0;
        nodes_[i].angle_defect = 0.0;
        nodes_[i].area = 0.0;
        nodes_[i].angle_variance = 0.0;
    }
    
    // Build adjacency information from faces
    std::map<std::pair<int, int>, int> edgeCount;
    
    // Store faces for each vertex with adjacent vertices in order
    std::map<int, std::vector<std::pair<int, int>>> vertexFaces;
    
    for (const auto& face : faces) {
        int v0 = face[0];
        int v1 = face[1];
        int v2 = face[2];
        
        // For each vertex, store the pair of adjacent vertices in the order they appear in the face
        vertexFaces[v0].push_back({v1, v2});
        vertexFaces[v1].push_back({v2, v0});
        vertexFaces[v2].push_back({v0, v1});
        
        // Add edges (store in sorted order to avoid duplicates)
        auto addEdge = [&](int a, int b) {
            if (a > b) std::swap(a, b);
            edgeCount[{a, b}]++;
        };
        
        addEdge(v0, v1);
        addEdge(v1, v2);
        addEdge(v2, v0);
    }
    
    // Build edge list
    edges_.clear();
    for (const auto& [edge, count] : edgeCount) {
        edges_.push_back({edge.first, edge.second});
    }
    
    // Order neighbors cyclically for each vertex using azimuth sorting
    for (size_t i = 0; i < nodes_.size(); ++i) {
        auto& faceList = vertexFaces[i];
        if (faceList.empty()) continue;
        
        // Collect unique neighbors
        std::set<int> neighborSet;
        for (const auto& [a, b] : faceList) {
            neighborSet.insert(a);
            neighborSet.insert(b);
        }
        
        std::vector<int> neighbors(neighborSet.begin(), neighborSet.end());
        if (neighbors.empty()) continue;
        
        // Get the center vertex position and normal
        Eigen::Vector3d center = vertices[i];
        Eigen::Vector3d normal = center.normalized();
        
        // Create a local tangent frame at the vertex
        // Choose an arbitrary tangent vector perpendicular to the normal
        Eigen::Vector3d tangent;
        if (std::abs(normal.z()) < 0.9) {
            tangent = Eigen::Vector3d(0, 0, 1).cross(normal).normalized();
        } else {
            tangent = Eigen::Vector3d(1, 0, 0).cross(normal).normalized();
        }
        Eigen::Vector3d bitangent = normal.cross(tangent);
        
        // Sort neighbors by azimuth in the tangent frame
        std::vector<std::pair<double, int>> azimuthNeighbors;
        for (int neighbor : neighbors) {
            Eigen::Vector3d dir = vertices[neighbor] - center;
            // Project to tangent plane
            double u = dir.dot(tangent);
            double v = dir.dot(bitangent);
            double azimuth = std::atan2(v, u);
            azimuthNeighbors.push_back({azimuth, neighbor});
        }
        
        // Sort by azimuth
        std::sort(azimuthNeighbors.begin(), azimuthNeighbors.end());
        
        // Extract ordered neighbors
        std::vector<int> orderedNeighbors;
        for (const auto& [azimuth, neighbor] : azimuthNeighbors) {
            orderedNeighbors.push_back(neighbor);
        }
        
        nodes_[i].neighbors = orderedNeighbors;
    }
    
    // Compute angle defects (sum of angles around vertex)
    for (const auto& face : faces) {
        for (int i = 0; i < 3; ++i) {
            int v0 = face[i];
            int v1 = face[(i + 1) % 3];
            int v2 = face[(i + 2) % 3];
            
            // Compute spherical angle at v0 using spherical law of cosines
            // First, get the central angles (arc lengths on unit sphere)
            Eigen::Vector3d p0 = vertices[v0].normalized();
            Eigen::Vector3d p1 = vertices[v1].normalized();
            Eigen::Vector3d p2 = vertices[v2].normalized();
            
            // Central angles
            double b = std::acos(std::max(-1.0, std::min(1.0, p0.dot(p2)))); // arc from v0 to v2
            double c = std::acos(std::max(-1.0, std::min(1.0, p0.dot(p1)))); // arc from v0 to v1
            double a = std::acos(std::max(-1.0, std::min(1.0, p1.dot(p2)))); // arc from v1 to v2
            
            // Spherical law of cosines to get angle at v0
            // cos(a) = cos(b)*cos(c) + sin(b)*sin(c)*cos(A)
            // => cos(A) = (cos(a) - cos(b)*cos(c)) / (sin(b)*sin(c))
            double sin_b = std::sin(b);
            double sin_c = std::sin(c);
            
            if (sin_b > 1e-10 && sin_c > 1e-10) {
                double cos_angle = (std::cos(a) - std::cos(b) * std::cos(c)) / (sin_b * sin_c);
                cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
                double angle = std::acos(cos_angle);
                nodes_[v0].angle_defect += angle;
            }
        }
    }
    
    // Convert to actual defect from 2π
    const double TWO_PI = 2.0 * M_PI;
    for (auto& node : nodes_) {
        node.angle_defect = TWO_PI - node.angle_defect;
    }
}

} // namespace spherical_tiling
