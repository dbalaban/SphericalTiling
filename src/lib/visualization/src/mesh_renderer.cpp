#include "mesh_renderer.h"
#include <iostream>
#include <cmath>

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
      dualVAO_(0), dualVBO_(0), dualEBO_(0), hasDualMesh_(false) {
    setupShaders();
}

MeshRenderer::~MeshRenderer() {
    if (primalVAO_) glDeleteVertexArrays(1, &primalVAO_);
    if (primalVBO_) glDeleteBuffers(1, &primalVBO_);
    if (primalEBO_) glDeleteBuffers(1, &primalEBO_);
    if (dualVAO_) glDeleteVertexArrays(1, &dualVAO_);
    if (dualVBO_) glDeleteBuffers(1, &dualVBO_);
    if (dualEBO_) glDeleteBuffers(1, &dualEBO_);
    if (shaderProgram_) glDeleteProgram(shaderProgram_);
}

void MeshRenderer::setupShaders() {
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
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

void MeshRenderer::setDualMesh(const TileGraph& graph, double radius) {
    dualVertices_.clear();
    dualIndices_.clear();
    
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();
    
    // Add all node positions as vertices
    dualVertices_.reserve(nodes.size() * 3);
    for (const auto& node : nodes) {
        dualVertices_.push_back(static_cast<float>(node.center.x()));
        dualVertices_.push_back(static_cast<float>(node.center.y()));
        dualVertices_.push_back(static_cast<float>(node.center.z()));
    }
    
    // Add all edges as line segments
    dualIndices_.reserve(edges.size() * 2);
    for (const auto& edge : edges) {
        dualIndices_.push_back(edge.node1);
        dualIndices_.push_back(edge.node2);
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

} // namespace spherical_tiling
