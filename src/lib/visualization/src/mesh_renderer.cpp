#include "mesh_renderer.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <utility>

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
    : shaderProgram_(0),
      mvpLocation_(-1),
      colorLocation_(-1),
      isIcosahedronVisible_(false),
      isSubdivisionVisible_(false),
      isPrimalMeshVisible_(false),
      isPrimalDebugMeshVisible_(false),
      isDualMeshVisible_(false),
      viewsCreated_(false) {
  setupShaders();
}

MeshRenderer::~MeshRenderer() {
  if (shaderProgram_) glDeleteProgram(shaderProgram_);
}

void MeshRenderer::setMeshConstruct(ConstMeshConstructorPtr mesh) {
  mesh_ = std::move(mesh);
  uploadMeshes();
}

void MeshRenderer::setMeshConstruct(MeshConstructorPtr mesh) {
  mesh_ = std::const_pointer_cast<const MeshConstructor>(std::move(mesh));
  uploadMeshes();
}

void MeshRenderer::createGLMesh(GLMesh& glMesh) {
  glGenVertexArrays(1, &glMesh.vao);
  glGenBuffers(1, &glMesh.vbo);
  glGenBuffers(1, &glMesh.ebo);
}

void MeshRenderer::setMeshVisibility(MeshType type, bool isVisible) {
  switch (type) {
    case ICOSAHEDRON:
      isIcosahedronVisible_ = isVisible;
       break;
    case SUBDIVISION:
      isSubdivisionVisible_ = isVisible;
      break;
    case PRIMAL:
      isPrimalMeshVisible_ = isVisible;
      break;
    case PRIMAL_DEBUG:
      isPrimalDebugMeshVisible_ = isVisible;
      break;
    case DUAL:
      isDualMeshVisible_ = isVisible;
      break;
    default:
      std::cerr << "Unknown MeshType in setMeshVisibility" << std::endl;
  }
}

void MeshRenderer::uploadMeshToGL(const Vertices& V, const Edges& E, GLMesh& glMesh) {
  if (V.cols()==0 || E.empty()) {
    glMesh.indexCount = 0;
    glBindVertexArray(0);
    return;
  }
  glBindVertexArray(glMesh.vao);

  // Upload vertex data
  Eigen::Matrix3Xf V_float = V.cast<float>();
  glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
  glBufferData(GL_ARRAY_BUFFER, V_float.size() * sizeof(float), V_float.data(), GL_STATIC_DRAW);

  // Upload index data
  std::vector<GLuint> indices;
  indices.reserve(E.size() * 2);
  for (const auto& edge : E) {
    indices.push_back(static_cast<GLuint>(edge(0)));
    indices.push_back(static_cast<GLuint>(edge(1)));
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
  glMesh.indexCount = static_cast<GLsizei>(indices.size());

  // Set vertex attribute pointers
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

void MeshRenderer::uploadMeshes() {
  if (!mesh_) {
    std::cerr << "No mesh data to upload." << std::endl;
    return;
  }
  if (!viewsCreated_) {
    createGLMesh(icosahedronMesh_);
    createGLMesh(subdivisionMesh_);
    createGLMesh(primalMesh_);
    createGLMesh(primalDebugMesh_);
    createGLMesh(dualMesh_);
    viewsCreated_ = true;
  }

  const Mesh& icosahedron = mesh_->getIcosahedron();
  const Mesh& subdivision = mesh_->getSubdividedMesh();
  const Mesh& primal = mesh_->getPrimalMesh();
  const Mesh& dual = mesh_->getDualMesh();

  uploadMeshToGL(icosahedron.vertices, icosahedron.edges, icosahedronMesh_);
  uploadMeshToGL(subdivision.vertices, subdivision.edges, subdivisionMesh_);
  // primal and subdivision edges are held in common, stored in subdivision
  uploadMeshToGL(primal.vertices, subdivision.edges, primalMesh_);
  // primal edges store neighborhood rings for debugging purposes
  uploadMeshToGL(primal.vertices, primal.edges, primalDebugMesh_);
  uploadMeshToGL(dual.vertices, dual.edges, dualMesh_);
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

void MeshRenderer::renderMesh(const GLMesh& mesh, const glm::mat4& mvpMatrix, const glm::vec3& color) {
  glUseProgram(shaderProgram_);
    
  glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, &mvpMatrix[0][0]);
  glUniform3fv(colorLocation_, 1, &color[0]);
    
  glBindVertexArray(mesh.vao);
  glDrawElements(GL_LINES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void MeshRenderer::renderAllMeshes(const glm::mat4& mvpMatrix) {
  if (isIcosahedronVisible_) {
    renderMesh(icosahedronMesh_, mvpMatrix, cIco);
  }
  if (isSubdivisionVisible_) {
    renderMesh(subdivisionMesh_, mvpMatrix, cSub);
  }
  if (isPrimalMeshVisible_) {
    renderMesh(primalMesh_, mvpMatrix, cPN);
  }
  if (isPrimalDebugMeshVisible_) {
    renderMesh(primalDebugMesh_, mvpMatrix, cPD);
  }
  if (isDualMeshVisible_) {
    renderMesh(dualMesh_, mvpMatrix, cDual);
  }
} // namespace spherical_tiling
