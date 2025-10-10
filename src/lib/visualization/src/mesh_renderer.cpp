#include "mesh_renderer.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <map>
#include <tuple>
#include <vector>
#include <algorithm>

namespace spherical_tiling {

// Simple vertex shader
static const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

// Simple fragment shader
static const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 uColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

MeshRenderer::MeshRenderer() 
    : shaderProgram_(0), mvpLocation_(-1), colorLocation_(-1),
      primalVAO_(0), primalVBO_(0), primalEBO_(0), hasPrimalMesh_(false),
      dualVAO_(0), dualVBO_(0), dualEBO_(0), hasDualMesh_(false),
      triangleVAO_(0), triangleVBO_(0), triangleEBO_(0), hasTriangleMesh_(false) {
    setupShaders();
}

MeshRenderer::~MeshRenderer() {
    if (primalVAO_) glDeleteVertexArrays(1, &primalVAO_);
    if (primalVBO_) glDeleteBuffers(1, &primalVBO_);
    if (primalEBO_) glDeleteBuffers(1, &primalEBO_);
    if (dualVAO_) glDeleteVertexArrays(1, &dualVAO_);
    if (dualVBO_) glDeleteBuffers(1, &dualVBO_);
    if (dualEBO_) glDeleteBuffers(1, &dualEBO_);
    if (triangleVAO_) glDeleteVertexArrays(1, &triangleVAO_);
    if (triangleVBO_) glDeleteBuffers(1, &triangleVBO_);
    if (triangleEBO_) glDeleteBuffers(1, &triangleEBO_);
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
}

void MeshRenderer::setupShaders() {
    // Check that OpenGL is initialized
    if (!glCreateShader) {
        std::cerr << "ERROR: OpenGL not initialized! glCreateShader function pointer is null." << std::endl;
        std::cerr << "Make sure GLAD is initialized before creating MeshRenderer." << std::endl;
        throw std::runtime_error("OpenGL not initialized");
    }
    
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        std::cerr << "ERROR: Failed to create vertex shader" << std::endl;
        throw std::runtime_error("Failed to create vertex shader");
    }
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    
    // Check for compile errors
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
    }
    
    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
    }
    
    // Link shader program
    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vertexShader);
    glAttachShader(shaderProgram_, fragmentShader);
    glLinkProgram(shaderProgram_);
    
    glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram_, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
    }
    
    // Clean up shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    // Get uniform locations
    mvpLocation_ = glGetUniformLocation(shaderProgram_, "uMVP");
    colorLocation_ = glGetUniformLocation(shaderProgram_, "uColor");
}

void MeshRenderer::setPrimalMesh(const std::vector<Eigen::Vector3d>& vertices,
                                  const std::vector<Eigen::Vector3i>& faces) {
    // Convert vertices to float array
    primalVertices_.clear();
    primalVertices_.reserve(vertices.size() * 3);
    for (const auto& v : vertices) {
        primalVertices_.push_back(static_cast<float>(v.x()));
        primalVertices_.push_back(static_cast<float>(v.y()));
        primalVertices_.push_back(static_cast<float>(v.z()));
    }
    
    // Convert face indices to line segments (edges)
    primalIndices_.clear();
    primalIndices_.reserve(faces.size() * 6);
    for (const auto& face : faces) {
        // Three edges per triangle
        primalIndices_.push_back(face[0]);
        primalIndices_.push_back(face[1]);
        primalIndices_.push_back(face[1]);
        primalIndices_.push_back(face[2]);
        primalIndices_.push_back(face[2]);
        primalIndices_.push_back(face[0]);
    }
    
    hasPrimalMesh_ = true;
    setupPrimalBuffers();
}

void MeshRenderer::setPrimalMesh(const TileGraph& graph, double radius) {
    primalVertices_.clear();
    primalIndices_.clear();
    
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();
    
    // Add all node positions as vertices
    primalVertices_.reserve(nodes.size() * 3);
    for (const auto& node : nodes) {
        primalVertices_.push_back(static_cast<float>(node.center.x()));
        primalVertices_.push_back(static_cast<float>(node.center.y()));
        primalVertices_.push_back(static_cast<float>(node.center.z()));
    }
    
    // Add all edges as line segments
    primalIndices_.reserve(edges.size() * 2);
    for (const auto& edge : edges) {
        primalIndices_.push_back(edge.node1);
        primalIndices_.push_back(edge.node2);
    }
    
    hasPrimalMesh_ = true;
    setupPrimalBuffers();
}

void MeshRenderer::setDualMesh(const TileGraph& graph, double radius) {
    dualVertices_.clear();
    dualIndices_.clear();
    
    const auto& nodes = graph.getNodes();
    
    // NEW DUAL MESH IMPLEMENTATION (per user suggestion):
    // Build dual edges from primal edges, not by looping around vertices.
    // 
    // 1. For each triangle in the primal mesh, compute its circumcenter (dual vertex)
    // 2. For each primal edge, find the two triangles that share it
    // 3. Create a dual edge connecting the circumcenters of those two triangles
    //
    // This creates the proper dual mesh where each primal edge → one dual edge.
    
    // Map each triangle (sorted triple of vertex indices) to its circumcenter index
    std::map<std::tuple<int, int, int>, int> triangleToVertexIndex;
    
    // Step 1: Identify all triangles and compute their circumcenters
    for (size_t nodeIdx = 0; nodeIdx < nodes.size(); ++nodeIdx) {
        const auto& node = nodes[nodeIdx];
        const auto& neighbors = node.neighbors;
        
        if (neighbors.size() < 3) {
            continue;
        }
        
        // Each pair of consecutive neighbors forms a triangle with this node
        for (size_t i = 0; i < neighbors.size(); ++i) {
            int n1 = neighbors[i];
            int n2 = neighbors[(i + 1) % neighbors.size()];
            
            // Create sorted triangle key
            std::vector<int> tri = {(int)nodeIdx, n1, n2};
            std::sort(tri.begin(), tri.end());
            auto triKey = std::make_tuple(tri[0], tri[1], tri[2]);
            
            // Skip if we've already processed this triangle
            if (triangleToVertexIndex.find(triKey) != triangleToVertexIndex.end()) {
                continue;
            }
            
            // Compute circumcenter of this triangle
            const Eigen::Vector3d& p1 = node.center;
            const Eigen::Vector3d& p2 = nodes[n1].center;
            const Eigen::Vector3d& p3 = nodes[n2].center;
            
            // Get normals to the great circle arcs
            Eigen::Vector3d normal1 = p1.cross(p2).normalized();
            Eigen::Vector3d normal2 = p2.cross(p3).normalized();
            
            // The circumcenter is perpendicular to both normals
            Eigen::Vector3d center = normal1.cross(normal2).normalized();
            
            // Check which direction to use
            Eigen::Vector3d midpoint = (p1 + p2 + p3).normalized();
            if (center.dot(midpoint) < 0) {
                center = -center;
            }
            
            Eigen::Vector3d circumcenter = center * radius;
            
            // Add this circumcenter as a dual vertex
            int vertexIndex = dualVertices_.size() / 3;
            dualVertices_.push_back(static_cast<float>(circumcenter.x()));
            dualVertices_.push_back(static_cast<float>(circumcenter.y()));
            dualVertices_.push_back(static_cast<float>(circumcenter.z()));
            
            triangleToVertexIndex[triKey] = vertexIndex;
        }
    }
    
    // Step 2: For each primal edge, create a dual edge
    const auto& edges = graph.getEdges();
    for (const auto& edge : edges) {
        int v1 = edge.node1;
        int v2 = edge.node2;
        
        // Find the two triangles that share this edge
        std::vector<std::tuple<int, int, int>> matchingTriangles;
        
        for (const auto& [tri, idx] : triangleToVertexIndex) {
            int t0 = std::get<0>(tri);
            int t1 = std::get<1>(tri);
            int t2 = std::get<2>(tri);
            
            // Check if this triangle contains both v1 and v2
            int count = 0;
            if (t0 == v1 || t0 == v2) count++;
            if (t1 == v1 || t1 == v2) count++;
            if (t2 == v1 || t2 == v2) count++;
            
            if (count == 2) {
                matchingTriangles.push_back(tri);
            }
        }
        
        // There should be exactly 2 triangles sharing this edge
        if (matchingTriangles.size() == 2) {
            int idx1 = triangleToVertexIndex[matchingTriangles[0]];
            int idx2 = triangleToVertexIndex[matchingTriangles[1]];
            
            dualIndices_.push_back(idx1);
            dualIndices_.push_back(idx2);
        }
    }
    
    hasDualMesh_ = true;
    setupDualBuffers();
}

void MeshRenderer::setupPrimalBuffers() {
    if (!primalVAO_) {
        glGenVertexArrays(1, &primalVAO_);
        glGenBuffers(1, &primalVBO_);
        glGenBuffers(1, &primalEBO_);
    }
    
    glBindVertexArray(primalVAO_);
    
    glBindBuffer(GL_ARRAY_BUFFER, primalVBO_);
    glBufferData(GL_ARRAY_BUFFER, primalVertices_.size() * sizeof(float),
                 primalVertices_.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primalEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, primalIndices_.size() * sizeof(unsigned int),
                 primalIndices_.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void MeshRenderer::setupDualBuffers() {
    if (!dualVAO_) {
        glGenVertexArrays(1, &dualVAO_);
        glGenBuffers(1, &dualVBO_);
        glGenBuffers(1, &dualEBO_);
    }
    
    glBindVertexArray(dualVAO_);
    
    glBindBuffer(GL_ARRAY_BUFFER, dualVBO_);
    glBufferData(GL_ARRAY_BUFFER, dualVertices_.size() * sizeof(float),
                 dualVertices_.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dualEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, dualIndices_.size() * sizeof(unsigned int),
                 dualIndices_.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void MeshRenderer::renderPrimal(const Eigen::Matrix4f& viewProj, const Eigen::Vector3f& color) {
    if (!hasPrimalMesh_) return;
    
    glUseProgram(shaderProgram_);
    glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, viewProj.data());
    glUniform3f(colorLocation_, color.x(), color.y(), color.z());
    
    glBindVertexArray(primalVAO_);
    glDrawElements(GL_LINES, primalIndices_.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void MeshRenderer::renderDual(const Eigen::Matrix4f& viewProj, const Eigen::Vector3f& color) {
    if (!hasDualMesh_) return;
    
    glUseProgram(shaderProgram_);
    glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, viewProj.data());
    glUniform3f(colorLocation_, color.x(), color.y(), color.z());
    
    glBindVertexArray(dualVAO_);
    glDrawElements(GL_LINES, dualIndices_.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void MeshRenderer::clearPrimal() {
    hasPrimalMesh_ = false;
    primalVertices_.clear();
    primalIndices_.clear();
}

void MeshRenderer::clearDual() {
    hasDualMesh_ = false;
    dualVertices_.clear();
    dualIndices_.clear();
}

void MeshRenderer::setTriangleMesh(const std::vector<Eigen::Vector3d>& vertices,
                                    const std::vector<Eigen::Vector3i>& faces) {
    // Convert vertices to float array
    triangleVertices_.clear();
    triangleVertices_.reserve(vertices.size() * 3);
    for (const auto& v : vertices) {
        triangleVertices_.push_back(static_cast<float>(v.x()));
        triangleVertices_.push_back(static_cast<float>(v.y()));
        triangleVertices_.push_back(static_cast<float>(v.z()));
    }
    
    // Convert face indices to line segments (edges)
    triangleIndices_.clear();
    triangleIndices_.reserve(faces.size() * 6);
    for (const auto& face : faces) {
        // Three edges per triangle
        triangleIndices_.push_back(face[0]);
        triangleIndices_.push_back(face[1]);
        triangleIndices_.push_back(face[1]);
        triangleIndices_.push_back(face[2]);
        triangleIndices_.push_back(face[2]);
        triangleIndices_.push_back(face[0]);
    }
    
    hasTriangleMesh_ = true;
    setupTriangleBuffers();
}

void MeshRenderer::setupTriangleBuffers() {
    if (!triangleVAO_) {
        glGenVertexArrays(1, &triangleVAO_);
        glGenBuffers(1, &triangleVBO_);
        glGenBuffers(1, &triangleEBO_);
    }
    
    glBindVertexArray(triangleVAO_);
    
    glBindBuffer(GL_ARRAY_BUFFER, triangleVBO_);
    glBufferData(GL_ARRAY_BUFFER, triangleVertices_.size() * sizeof(float),
                 triangleVertices_.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangleEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangleIndices_.size() * sizeof(unsigned int),
                 triangleIndices_.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void MeshRenderer::renderTriangles(const Eigen::Matrix4f& viewProj, const Eigen::Vector3f& color) {
    if (!hasTriangleMesh_) return;
    
    glUseProgram(shaderProgram_);
    glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, viewProj.data());
    glUniform3f(colorLocation_, color.x(), color.y(), color.z());
    
    glBindVertexArray(triangleVAO_);
    glDrawElements(GL_LINES, triangleIndices_.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void MeshRenderer::clearTriangles() {
    hasTriangleMesh_ = false;
    triangleVertices_.clear();
    triangleIndices_.clear();
}

} // namespace spherical_tiling
