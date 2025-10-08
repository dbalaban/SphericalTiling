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
    
    // Order neighbors cyclically for each vertex
    for (size_t i = 0; i < nodes_.size(); ++i) {
        auto& faceList = vertexFaces[i];
        if (faceList.empty()) continue;
        
        std::vector<int> orderedNeighbors;
        std::map<int, int> nextNeighbor; // Map from one neighbor to the next
        
        // Build a map of neighbor transitions
        for (const auto& [a, b] : faceList) {
            nextNeighbor[a] = b;
        }
        
        // Start from any neighbor
        if (nextNeighbor.empty()) continue;
        
        int start = nextNeighbor.begin()->first;
        int current = start;
        
        // Follow the chain until we complete the cycle
        for (size_t count = 0; count < faceList.size(); ++count) {
            orderedNeighbors.push_back(current);
            
            auto it = nextNeighbor.find(current);
            if (it == nextNeighbor.end()) {
                // Chain broken - this shouldn't happen for a closed mesh
                break;
            }
            current = it->second;
        }
        
        nodes_[i].neighbors = orderedNeighbors;
    }
    
    // Compute angle defects (sum of angles around vertex)
    for (const auto& face : faces) {
        for (int i = 0; i < 3; ++i) {
            int v0 = face[i];
            int v1 = face[(i + 1) % 3];
            int v2 = face[(i + 2) % 3];
            
            // Compute angle at v0
            Eigen::Vector3d edge1 = (vertices[v1] - vertices[v0]).normalized();
            Eigen::Vector3d edge2 = (vertices[v2] - vertices[v0]).normalized();
            
            double cosAngle = edge1.dot(edge2);
            cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
            double angle = std::acos(cosAngle);
            
            nodes_[v0].angle_defect += angle;
        }
    }
    
    // Convert to actual defect from 2π
    const double TWO_PI = 2.0 * M_PI;
    for (auto& node : nodes_) {
        node.angle_defect = TWO_PI - node.angle_defect;
    }
}

} // namespace spherical_tiling
