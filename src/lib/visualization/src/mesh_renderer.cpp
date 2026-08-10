#include "mesh_renderer.h"
#include "templated_geometry.h"

#include <algorithm>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace spherical_tiling {

namespace {

constexpr float kPiF = 3.14159265358979323846f;
constexpr double kPiD = 3.14159265358979323846;
constexpr int kFaceSamplesPerAxis = 12;

double unwrapLongitude(double lon, double reference) {
  while (lon - reference > kPiD) {
    lon -= 2.0 * kPiD;
  }
  while (lon - reference < -kPiD) {
    lon += 2.0 * kPiD;
  }
  return lon;
}

}

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

static const char* colorVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uMVP;

out vec3 vColor;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

static const char* colorFragmentShaderSource = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

MeshRenderer::MeshRenderer() 
    : shaderProgram_(0),
      colorShaderProgram_(0),
      mvpLocation_(-1),
      colorLocation_(-1),
      colorMvpLocation_(-1),
      viewsCreated_(false) {
  setupShaders();
}

MeshRenderer::~MeshRenderer() {
  if (shaderProgram_) glDeleteProgram(shaderProgram_);
  if (colorShaderProgram_) glDeleteProgram(colorShaderProgram_);
}

void MeshRenderer::setMeshConstruct(ConstMeshConstructorPtr mesh) {
  mesh_ = std::move(mesh);
  uploadMeshes();
}

void MeshRenderer::setMeshConstruct(MeshConstructorPtr mesh) {
  mesh_ = std::const_pointer_cast<const MeshConstructor>(std::move(mesh));
  uploadMeshes();
}

bool MeshRenderer::loadEarthTexture(const std::string& path, std::string& error) {
  const bool loaded = earthTexture_.load(path, error);
  if (loaded && mesh_) {
    bakeDualCellColors();
  }
  return loaded;
}

const std::string& MeshRenderer::getEarthTexturePath() const {
  return earthTexture_.path();
}

void MeshRenderer::createGLMesh(GLMesh& glMesh) {
  glGenVertexArrays(1, &glMesh.vao);
  glGenBuffers(1, &glMesh.vbo);
  glGenBuffers(1, &glMesh.ebo);
}

void MeshRenderer::createGLColorMesh(GLColorMesh& glMesh) {
  glGenVertexArrays(1, &glMesh.vao);
  glGenBuffers(1, &glMesh.vbo);
  glGenBuffers(1, &glMesh.cbo);
  glGenBuffers(1, &glMesh.ebo);
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

void MeshRenderer::uploadColorMeshToGL(const std::vector<Eigen::Vector3f>& positions,
                                       const std::vector<Eigen::Vector3f>& colors,
                                       const std::vector<GLuint>& indices,
                                       GLColorMesh& glMesh) {
  if (positions.empty() || colors.empty() || indices.empty()) {
    glMesh.indexCount = 0;
    glBindVertexArray(0);
    return;
  }

  glBindVertexArray(glMesh.vao);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(positions.size() * sizeof(Eigen::Vector3f)),
               positions.data(),
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector3f), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.cbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(colors.size() * sizeof(Eigen::Vector3f)),
               colors.data(),
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector3f), (void*)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)),
               indices.data(),
               GL_DYNAMIC_DRAW);
  glMesh.indexCount = static_cast<GLsizei>(indices.size());

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
    createGLColorMesh(earthMesh_);
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
  validation_ = validateDualTopology(*mesh_);
  bakeDualCellColors();
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
    throw std::runtime_error("Vertex shader compilation failed");
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
    throw std::runtime_error("Fragment shader compilation failed");
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
    throw std::runtime_error("Shader program linking failed");
  }
    
  // Clean up shaders
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
    
  // Get uniform locations
  mvpLocation_ = glGetUniformLocation(shaderProgram_, "uMVP");
  colorLocation_ = glGetUniformLocation(shaderProgram_, "uColor");

  GLuint colorVertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(colorVertexShader, 1, &colorVertexShaderSource, nullptr);
  glCompileShader(colorVertexShader);
  glGetShaderiv(colorVertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(colorVertexShader, 512, nullptr, infoLog);
    std::cerr << "Color vertex shader compilation failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Color vertex shader compilation failed");
  }

  GLuint colorFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(colorFragmentShader, 1, &colorFragmentShaderSource, nullptr);
  glCompileShader(colorFragmentShader);
  glGetShaderiv(colorFragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(colorFragmentShader, 512, nullptr, infoLog);
    std::cerr << "Color fragment shader compilation failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Color fragment shader compilation failed");
  }

  colorShaderProgram_ = glCreateProgram();
  glAttachShader(colorShaderProgram_, colorVertexShader);
  glAttachShader(colorShaderProgram_, colorFragmentShader);
  glLinkProgram(colorShaderProgram_);
  glGetProgramiv(colorShaderProgram_, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(colorShaderProgram_, 512, nullptr, infoLog);
    std::cerr << "Color shader program linking failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Color shader program linking failed");
  }

  glDeleteShader(colorVertexShader);
  glDeleteShader(colorFragmentShader);
  colorMvpLocation_ = glGetUniformLocation(colorShaderProgram_, "uMVP");
}

void MeshRenderer::renderMesh(const GLMesh& mesh, const glm::mat4& mvpMatrix, const glm::vec3& color) {
  glUseProgram(shaderProgram_);
    
  glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, &mvpMatrix[0][0]);
  glUniform3fv(colorLocation_, 1, &color[0]);
    
  glBindVertexArray(mesh.vao);
  glDrawElements(GL_LINES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void MeshRenderer::renderColoredMesh(const GLColorMesh& mesh, const glm::mat4& mvpMatrix) {
  glUseProgram(colorShaderProgram_);
  glUniformMatrix4fv(colorMvpLocation_, 1, GL_FALSE, &mvpMatrix[0][0]);

  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f, 1.0f);
  glBindVertexArray(mesh.vao);
  glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  glDisable(GL_POLYGON_OFFSET_FILL);
}

float MeshRenderer::smoothstep(float edge0, float edge1, float value) const {
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

bool MeshRenderer::pointInPolygon(const Eigen::Vector2d& point, const std::vector<Eigen::Vector2d>& polygon) const {
  bool inside = false;
  for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const bool intersect = ((polygon[i].y() > point.y()) != (polygon[j].y() > point.y()))
      && (point.x() < (polygon[j].x() - polygon[i].x()) * (point.y() - polygon[i].y()) / (polygon[j].y() - polygon[i].y() + 1e-12) + polygon[i].x());
    if (intersect) {
      inside = !inside;
    }
  }
  return inside;
}

Eigen::Vector3f MeshRenderer::sampleDualFaceColor(const Face& face) const {
  if (!mesh_) {
    return Eigen::Vector3f(0.6f, 0.6f, 0.6f);
  }

  const Mesh& dual = mesh_->getDualMesh();
  std::vector<Eigen::Vector3d> polygon3d;
  polygon3d.reserve(static_cast<std::size_t>(face.size()));
  Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
  for (int i = 0; i < face.size(); ++i) {
    Eigen::Vector3d vertex = dual.vertices.col(face[i]).normalized();
    polygon3d.push_back(vertex);
    centroid += vertex;
  }
  centroid.normalize();

  const Eigen::Vector2d centroidLatLon = cartesianToLatLon(centroid);
  const double refLon = centroidLatLon.y();

  std::vector<Eigen::Vector2d> polygon2d;
  polygon2d.reserve(polygon3d.size());
  double minLon = std::numeric_limits<double>::max();
  double maxLon = -std::numeric_limits<double>::max();
  double minLat = std::numeric_limits<double>::max();
  double maxLat = -std::numeric_limits<double>::max();

  for (const auto& vertex : polygon3d) {
    Eigen::Vector2d latLon = cartesianToLatLon(vertex);
    const double lon = unwrapLongitude(latLon.y(), refLon);
    polygon2d.emplace_back(lon, latLon.x());
    minLon = std::min(minLon, lon);
    maxLon = std::max(maxLon, lon);
    minLat = std::min(minLat, static_cast<double>(latLon.x()));
    maxLat = std::max(maxLat, static_cast<double>(latLon.x()));
  }

  Eigen::Vector3f sum = Eigen::Vector3f::Zero();
  int count = 0;
  for (int y = 0; y < kFaceSamplesPerAxis; ++y) {
    const double fy = (static_cast<double>(y) + 0.5) / static_cast<double>(kFaceSamplesPerAxis);
    const double lat = minLat + (maxLat - minLat) * fy;
    for (int x = 0; x < kFaceSamplesPerAxis; ++x) {
      const double fx = (static_cast<double>(x) + 0.5) / static_cast<double>(kFaceSamplesPerAxis);
      const double lon = minLon + (maxLon - minLon) * fx;
      if (!pointInPolygon(Eigen::Vector2d(lon, lat), polygon2d)) {
        continue;
      }

      const float u = static_cast<float>((lon + kPiD) / (2.0 * kPiD));
      const float v = static_cast<float>((kPiD * 0.5 - lat) / kPiD);
      sum += earthTexture_.sampleBilinear(u, v);
      ++count;
    }
  }

  if (count == 0) {
    const float u = static_cast<float>((refLon + kPiD) / (2.0 * kPiD));
    const float v = static_cast<float>((kPiD * 0.5 - centroidLatLon.x()) / kPiD);
    return earthTexture_.sampleBilinear(u, v);
  }

  return sum / static_cast<float>(count);
}

void MeshRenderer::bakeDualCellColors() {
  dualCellColors_.clear();
  if (!mesh_) {
    return;
  }

  const Mesh& dual = mesh_->getDualMesh();
  dualCellColors_.reserve(dual.faces.size());
  if (!earthTexture_.isLoaded()) {
    for (const auto& face : dual.faces) {
      const std::size_t degree = static_cast<std::size_t>(face.size());
      if (degree == 5) {
        dualCellColors_.emplace_back(0.78f, 0.66f, 0.34f);
      } else {
        dualCellColors_.emplace_back(0.23f, 0.48f, 0.31f);
      }
    }
    return;
  }

  for (const auto& face : dual.faces) {
    dualCellColors_.push_back(sampleDualFaceColor(face));
  }
}

Eigen::Vector3f MeshRenderer::morphPosition(const Eigen::Vector3d& position,
                                            const Eigen::Vector3f& anchor,
                                            const Eigen::Vector3f& east,
                                            const Eigen::Vector3f& north,
                                            float morphFactor) const {
  const Eigen::Vector3f spherical = position.cast<float>();
  const Eigen::Vector3f unit = spherical.normalized();
  const float anchorDot = std::clamp(unit.dot(anchor), -1.0f, 1.0f);
  const float angle = std::acos(anchorDot);
  const float locality = 1.0f - smoothstep(0.15f, 1.45f, angle);
  const float blend = morphFactor * locality;

  const float sphereRadius = static_cast<float>(mesh_->getRadius());
  const float x = sphereRadius * std::asin(std::clamp(unit.dot(east), -1.0f, 1.0f));
  const float y = sphereRadius * std::asin(std::clamp(unit.dot(north), -1.0f, 1.0f));
  const Eigen::Vector3f planar = anchor * sphereRadius + east * x + north * y;
  return spherical * (1.0f - blend) + planar * blend;
}

void MeshRenderer::updateDynamicEarthMeshes(const Eigen::Vector3f& cameraPosition, float morphFactor) {
  if (!mesh_) {
    return;
  }

  const Mesh& dual = mesh_->getDualMesh();
  Eigen::Vector3f anchor = cameraPosition.normalized();
  Eigen::Vector3f ref(0.0f, 0.0f, 1.0f);
  if (std::abs(anchor.dot(ref)) > 0.95f) {
    ref = Eigen::Vector3f(0.0f, 1.0f, 0.0f);
  }
  Eigen::Vector3f east = ref.cross(anchor).normalized();
  Eigen::Vector3f north = anchor.cross(east).normalized();

  Vertices morphedVertices(3, dual.vertices.cols());
  for (Eigen::Index i = 0; i < dual.vertices.cols(); ++i) {
    morphedVertices.col(i) = morphPosition(dual.vertices.col(i), anchor, east, north, morphFactor).cast<double>();
  }
  uploadMeshToGL(morphedVertices, dual.edges, dualMesh_);

  std::vector<Eigen::Vector3f> positions;
  std::vector<Eigen::Vector3f> colors;
  std::vector<GLuint> indices;

  for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
    const Face& face = dual.faces[faceIndex];
    if (face.size() < 3) {
      continue;
    }
    const GLuint baseIndex = static_cast<GLuint>(positions.size());
    const Eigen::Vector3f color = faceIndex < dualCellColors_.size()
      ? dualCellColors_[faceIndex]
      : Eigen::Vector3f(0.6f, 0.6f, 0.6f);

    for (int i = 0; i < face.size(); ++i) {
      positions.push_back(morphPosition(dual.vertices.col(face[i]), anchor, east, north, morphFactor));
      colors.push_back(color);
    }

    for (int i = 1; i < face.size() - 1; ++i) {
      indices.push_back(baseIndex);
      indices.push_back(baseIndex + static_cast<GLuint>(i));
      indices.push_back(baseIndex + static_cast<GLuint>(i + 1));
    }
  }

  uploadColorMeshToGL(positions, colors, indices, earthMesh_);
}

void MeshRenderer::renderAllMeshes(const glm::mat4& mvpMatrix, const Visibility& vis, const Eigen::Vector3f& cameraPosition, float morphFactor) {
  updateDynamicEarthMeshes(cameraPosition, morphFactor);
  if (vis.isEarthVisible_) {
    renderColoredMesh(earthMesh_, mvpMatrix);
  }
  if (vis.isIcosahedronVisible_) {
    renderMesh(icosahedronMesh_, mvpMatrix, cIco);
  }
  if (vis.isSubdivisionVisible_) {
    renderMesh(subdivisionMesh_, mvpMatrix, cSub);
  }
  if (vis.isPrimalMeshVisible_) {
    renderMesh(primalMesh_, mvpMatrix, cPN);
  }
  if (vis.isPrimalDebugMeshVisible_) {
    renderMesh(primalDebugMesh_, mvpMatrix, cPD);
  }
  if (vis.isDualMeshVisible_) {
    renderMesh(dualMesh_, mvpMatrix, cDual);
  }
}
} // namespace spherical_tiling
