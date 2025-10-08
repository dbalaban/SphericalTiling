#pragma once

#include <vector>
#include <map>
#include <Eigen/Dense>

namespace spherical_tiling {

struct Node {
    int id;
    Eigen::Vector3d center;
    std::vector<int> neighbors;
    
    // Parametrization (latitude, longitude) for optimization
    double latitude;   // radians: -π/2 to π/2
    double longitude;  // radians: -π to π
    
    // Per-node properties
    double energy;               // e
    double angle_defect;         // Angular defect from 2π
    double area;                 // Voronoi cell area
    double angle_variance;       // Variance of angles in the dual cell
};

struct Edge {
    int node1;
    int node2;
};

class TileGraph {
public:
    TileGraph() = default;
    
    // Build the graph from the primal triangulation
    void buildFromTriangulation(
        const std::vector<Eigen::Vector3d>& vertices,
        const std::vector<Eigen::Vector3i>& faces,
        double radius
    );
    
    // Access nodes and edges
    const std::vector<Node>& getNodes() const { return nodes_; }
    const std::vector<Edge>& getEdges() const { return edges_; }
    
    // Get a specific node
    const Node& getNode(int id) const { return nodes_[id]; }
    Node& getNode(int id) { return nodes_[id]; }
    
    // Get number of nodes
    size_t numNodes() const { return nodes_.size(); }
    
private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    double radius_;
};

} // namespace spherical_tiling
