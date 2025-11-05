#pragma once

#include <Eigen/Dense>
#include <vector>
#include <glad/gl.h>

namespace spherical_tiling {

class MeshRenderer {
public:
    MeshRenderer();
    ~MeshRenderer();
    
    // Set mesh data for primal (triangular) mesh
    void setPrimalMesh(const std::vector<Eigen::Vector3d>& vertices,
                       const std::vector<Eigen::Vector3i>& faces);
    
    // Set mesh data for primal mesh from TileGraph (adjacency graph)
    void setPrimalMesh(const TileGraph& graph, double radius);
    
    // Set mesh data for dual (Voronoi) mesh from TileGraph
    void setDualMesh(const TileGraph& graph, double radius);
    
    // Set mesh data for triangle mesh (subdivision face edges)
    void setTriangleMesh(const std::vector<Eigen::Vector3d>& vertices,
                         const std::vector<Eigen::Vector3i>& faces);
    
    // Render meshes
    void renderPrimal(const Eigen::Matrix4f& viewProj, const Eigen::Vector3f& color);
    void renderDual(const Eigen::Matrix4f& viewProj, const Eigen::Vector3f& color);
    void renderTriangles(const Eigen::Matrix4f& viewProj, const Eigen::Vector3f& color);
    
    // Clear mesh data
    void clearPrimal();
    void clearDual();
    void clearTriangles();
    
private:
    void setupShaders();
    void setupPrimalBuffers();
    void setupDualBuffers();
    void setupTriangleBuffers();
    
    // Shader program
    GLuint shaderProgram_;
    GLint mvpLocation_;
    GLint colorLocation_;
    
    // Primal mesh data
    std::vector<float> primalVertices_;
    std::vector<unsigned int> primalIndices_;
    GLuint primalVAO_;
    GLuint primalVBO_;
    GLuint primalEBO_;
    bool hasPrimalMesh_;
    
    // Dual mesh data (edges only)
    std::vector<float> dualVertices_;
    std::vector<unsigned int> dualIndices_;
    GLuint dualVAO_;
    GLuint dualVBO_;
    GLuint dualEBO_;
    bool hasDualMesh_;
    
    // Triangle mesh data (edges only)
    std::vector<float> triangleVertices_;
    std::vector<unsigned int> triangleIndices_;
    GLuint triangleVAO_;
    GLuint triangleVBO_;
    GLuint triangleEBO_;
    bool hasTriangleMesh_;
};

} // namespace spherical_tiling
