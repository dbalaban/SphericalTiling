#include "mesh_renderer.h"
#include "templated_geometry.h"

#include <algorithm>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace spherical_tiling {

namespace {

constexpr double kPiD = 3.14159265358979323846;
constexpr float kMorphStartAngle = 0.20f;
constexpr float kMorphEndAngle = 1.60f;
constexpr int kTileTextureResolution = 32;
constexpr int kTileTexturePadding = 1;
constexpr int kTileTextureStride = kTileTextureResolution + 2 * kTileTexturePadding;
constexpr int kTileAtlasSize = 4096;
constexpr int kMaxTileAtlases = 8;

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
uniform float uMorphFactor;
uniform vec3 uMorphAnchor;
uniform vec3 uMorphEast;
uniform vec3 uMorphNorth;
uniform float uSphereRadius;
uniform float uMorphStartAngle;
uniform float uMorphEndAngle;

vec3 morphPosition(vec3 spherical) {
    vec3 unit = normalize(spherical);
    float anchorDot = clamp(dot(unit, uMorphAnchor), -1.0, 1.0);
    float angle = acos(anchorDot);
    float locality = 1.0 - smoothstep(uMorphStartAngle, uMorphEndAngle, angle);
    float blend = uMorphFactor * locality;
    float x = uSphereRadius * asin(clamp(dot(unit, uMorphEast), -1.0, 1.0));
    float y = uSphereRadius * asin(clamp(dot(unit, uMorphNorth), -1.0, 1.0));
    vec3 planar = uMorphAnchor * uSphereRadius + uMorphEast * x + uMorphNorth * y;
    return mix(spherical, planar, blend);
}

void main() {
    gl_Position = uMVP * vec4(morphPosition(aPos), 1.0);
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
uniform float uMorphFactor;
uniform vec3 uMorphAnchor;
uniform vec3 uMorphEast;
uniform vec3 uMorphNorth;
uniform float uSphereRadius;
uniform float uMorphStartAngle;
uniform float uMorphEndAngle;

out vec3 vColor;
out vec3 vNormal;

vec3 morphPosition(vec3 spherical) {
    vec3 unit = normalize(spherical);
    float anchorDot = clamp(dot(unit, uMorphAnchor), -1.0, 1.0);
    float angle = acos(anchorDot);
    float locality = 1.0 - smoothstep(uMorphStartAngle, uMorphEndAngle, angle);
    float blend = uMorphFactor * locality;
    float x = uSphereRadius * asin(clamp(dot(unit, uMorphEast), -1.0, 1.0));
    float y = uSphereRadius * asin(clamp(dot(unit, uMorphNorth), -1.0, 1.0));
    vec3 planar = uMorphAnchor * uSphereRadius + uMorphEast * x + uMorphNorth * y;
    return mix(spherical, planar, blend);
}

void main() {
    gl_Position = uMVP * vec4(morphPosition(aPos), 1.0);
    vColor = aColor;
    vNormal = normalize(aPos);
}
)";

static const char* colorFragmentShaderSource = R"(
#version 330 core
in vec3 vColor;
in vec3 vNormal;
out vec4 FragColor;

uniform float uShadingStrength;
uniform vec3 uLightDir;

void main() {
    float lambert = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    float shade = mix(1.0, 0.72 + 0.28 * lambert, uShadingStrength);
    FragColor = vec4(vColor * shade, 1.0);
}
)";

static const char* atlasVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv;
layout (location = 2) in float aAtlasIndex;
layout (location = 3) in vec3 aFallbackColor;

uniform mat4 uMVP;
uniform float uMorphFactor;
uniform vec3 uMorphAnchor;
uniform vec3 uMorphEast;
uniform vec3 uMorphNorth;
uniform float uSphereRadius;
uniform float uMorphStartAngle;
uniform float uMorphEndAngle;

out vec2 vUv;
out vec3 vNormal;
out vec3 vFallbackColor;
flat out int vAtlasIndex;

vec3 morphPosition(vec3 spherical) {
    vec3 unit = normalize(spherical);
    float anchorDot = clamp(dot(unit, uMorphAnchor), -1.0, 1.0);
    float angle = acos(anchorDot);
    float locality = 1.0 - smoothstep(uMorphStartAngle, uMorphEndAngle, angle);
    float blend = uMorphFactor * locality;
    float x = uSphereRadius * asin(clamp(dot(unit, uMorphEast), -1.0, 1.0));
    float y = uSphereRadius * asin(clamp(dot(unit, uMorphNorth), -1.0, 1.0));
    vec3 planar = uMorphAnchor * uSphereRadius + uMorphEast * x + uMorphNorth * y;
    return mix(spherical, planar, blend);
}

void main() {
    gl_Position = uMVP * vec4(morphPosition(aPos), 1.0);
    vUv = aUv;
    vNormal = normalize(aPos);
    vFallbackColor = aFallbackColor;
    vAtlasIndex = int(aAtlasIndex + 0.5);
}
)";

static const char* atlasFragmentShaderSource = R"(
#version 330 core
in vec2 vUv;
in vec3 vNormal;
in vec3 vFallbackColor;
flat in int vAtlasIndex;
out vec4 FragColor;

uniform sampler2D uAtlasTextures[8];
uniform int uAtlasTextureCount;
uniform vec3 uLightDir;

vec3 sampleAtlas(int atlasIndex, vec2 uv) {
    if (atlasIndex == 0) return texture(uAtlasTextures[0], uv).rgb;
    if (atlasIndex == 1) return texture(uAtlasTextures[1], uv).rgb;
    if (atlasIndex == 2) return texture(uAtlasTextures[2], uv).rgb;
    if (atlasIndex == 3) return texture(uAtlasTextures[3], uv).rgb;
    if (atlasIndex == 4) return texture(uAtlasTextures[4], uv).rgb;
    if (atlasIndex == 5) return texture(uAtlasTextures[5], uv).rgb;
    if (atlasIndex == 6) return texture(uAtlasTextures[6], uv).rgb;
    if (atlasIndex == 7) return texture(uAtlasTextures[7], uv).rgb;
    return vFallbackColor;
}

void main() {
    vec3 color = (vAtlasIndex >= 0 && vAtlasIndex < uAtlasTextureCount) ? sampleAtlas(vAtlasIndex, vUv) : vFallbackColor;
    float lambert = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    float shade = 0.72 + 0.28 * lambert;
    FragColor = vec4(color * shade, 1.0);
}
)";

static const char* texturedVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUv;

uniform mat4 uMVP;
uniform float uMorphFactor;
uniform vec3 uMorphAnchor;
uniform vec3 uMorphEast;
uniform vec3 uMorphNorth;
uniform float uSphereRadius;
uniform float uMorphStartAngle;
uniform float uMorphEndAngle;

out vec2 vUv;

vec3 morphPosition(vec3 spherical) {
    vec3 unit = normalize(spherical);
    float anchorDot = clamp(dot(unit, uMorphAnchor), -1.0, 1.0);
    float angle = acos(anchorDot);
    float locality = 1.0 - smoothstep(uMorphStartAngle, uMorphEndAngle, angle);
    float blend = uMorphFactor * locality;
    float x = uSphereRadius * asin(clamp(dot(unit, uMorphEast), -1.0, 1.0));
    float y = uSphereRadius * asin(clamp(dot(unit, uMorphNorth), -1.0, 1.0));
    vec3 planar = uMorphAnchor * uSphereRadius + uMorphEast * x + uMorphNorth * y;
    return mix(spherical, planar, blend);
}

void main() {
    gl_Position = uMVP * vec4(morphPosition(aPos), 1.0);
    vUv = aUv;
}
)";

static const char* texturedFragmentShaderSource = R"(
#version 330 core
in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uEarthTexture;
uniform int uUseTexture;
uniform vec3 uFallbackColor;

void main() {
    vec3 color = (uUseTexture != 0) ? texture(uEarthTexture, vUv).rgb : uFallbackColor;
    FragColor = vec4(color, 1.0);
}
)";

MeshRenderer::MeshRenderer() 
    : performanceMode_(PerformanceMode::Balanced),
      shaderProgram_(0),
      colorShaderProgram_(0),
      atlasShaderProgram_(0),
      texturedShaderProgram_(0),
      earthTextureId_(0),
      mvpLocation_(-1),
      colorLocation_(-1),
      colorMvpLocation_(-1),
      atlasMvpLocation_(-1),
      atlasTextureCountLocation_(-1),
      atlasLightDirLocation_(-1),
      texturedMvpLocation_(-1),
      morphMvpLocation_(-1),
      morphFactorLocation_(-1),
      morphAnchorLocation_(-1),
      morphEastLocation_(-1),
      morphNorthLocation_(-1),
      morphRadiusLocation_(-1),
      morphStartAngleLocation_(-1),
      morphEndAngleLocation_(-1),
      colorMorphFactorLocation_(-1),
      colorMorphAnchorLocation_(-1),
      colorMorphEastLocation_(-1),
      colorMorphNorthLocation_(-1),
      colorMorphRadiusLocation_(-1),
      colorMorphStartAngleLocation_(-1),
      colorMorphEndAngleLocation_(-1),
      atlasMorphFactorLocation_(-1),
      atlasMorphAnchorLocation_(-1),
      atlasMorphEastLocation_(-1),
      atlasMorphNorthLocation_(-1),
      atlasMorphRadiusLocation_(-1),
      atlasMorphStartAngleLocation_(-1),
      atlasMorphEndAngleLocation_(-1),
      texturedMorphFactorLocation_(-1),
      texturedMorphAnchorLocation_(-1),
      texturedMorphEastLocation_(-1),
      texturedMorphNorthLocation_(-1),
      texturedMorphRadiusLocation_(-1),
      texturedMorphStartAngleLocation_(-1),
      texturedMorphEndAngleLocation_(-1),
      texturedSamplerLocation_(-1),
      texturedUseTextureLocation_(-1),
      texturedFallbackColorLocation_(-1),
      viewsCreated_(false) {
  std::fill(std::begin(atlasSamplerLocations_), std::end(atlasSamplerLocations_), -1);
  setupShaders();
}

MeshRenderer::~MeshRenderer() {
  if (shaderProgram_) glDeleteProgram(shaderProgram_);
  if (colorShaderProgram_) glDeleteProgram(colorShaderProgram_);
  if (atlasShaderProgram_) glDeleteProgram(atlasShaderProgram_);
  if (texturedShaderProgram_) glDeleteProgram(texturedShaderProgram_);
  if (earthTextureId_) glDeleteTextures(1, &earthTextureId_);
  if (!earthTileAtlasIds_.empty()) {
    glDeleteTextures(static_cast<GLsizei>(earthTileAtlasIds_.size()), earthTileAtlasIds_.data());
  }
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
  if (loaded) {
    uploadEarthTextureToGL();
  }
  if (mesh_) {
    bakeDualCellColors();
    buildEarthMesh();
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

void MeshRenderer::createGLAtlasMesh(GLAtlasMesh& glMesh) {
  glGenVertexArrays(1, &glMesh.vao);
  glGenBuffers(1, &glMesh.vbo);
  glGenBuffers(1, &glMesh.uvbo);
  glGenBuffers(1, &glMesh.abo);
  glGenBuffers(1, &glMesh.cbo);
  glGenBuffers(1, &glMesh.ebo);
}

void MeshRenderer::createGLTexturedMesh(GLTexturedMesh& glMesh) {
  glGenVertexArrays(1, &glMesh.vao);
  glGenBuffers(1, &glMesh.vbo);
  glGenBuffers(1, &glMesh.tbo);
  glGenBuffers(1, &glMesh.ebo);
}

int MeshRenderer::getEarthBakeSamplesPerAxis() const {
  switch (performanceMode_) {
    case PerformanceMode::Quality:
      return 12;
    case PerformanceMode::Balanced:
      return 8;
    case PerformanceMode::Performance:
      return 4;
  }
  return 8;
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
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector3f), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.cbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(colors.size() * sizeof(Eigen::Vector3f)),
               colors.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector3f), (void*)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)),
               indices.data(),
               GL_STATIC_DRAW);
  glMesh.indexCount = static_cast<GLsizei>(indices.size());

  glBindVertexArray(0);
}

void MeshRenderer::uploadAtlasMeshToGL(const std::vector<Eigen::Vector3f>& positions,
                                       const std::vector<Eigen::Vector2f>& atlasUvs,
                                       const std::vector<float>& atlasIndices,
                                       const std::vector<Eigen::Vector3f>& fallbackColors,
                                       const std::vector<GLuint>& indices,
                                       GLAtlasMesh& glMesh) {
  if (positions.empty() || atlasUvs.empty() || atlasIndices.empty() || fallbackColors.empty() || indices.empty()) {
    glMesh.indexCount = 0;
    glBindVertexArray(0);
    return;
  }

  glBindVertexArray(glMesh.vao);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(positions.size() * sizeof(Eigen::Vector3f)), positions.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector3f), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.uvbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(atlasUvs.size() * sizeof(Eigen::Vector2f)), atlasUvs.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector2f), (void*)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.abo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(atlasIndices.size() * sizeof(float)), atlasIndices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.cbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(fallbackColors.size() * sizeof(Eigen::Vector3f)), fallbackColors.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector3f), (void*)0);
  glEnableVertexAttribArray(3);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)), indices.data(), GL_STATIC_DRAW);
  glMesh.indexCount = static_cast<GLsizei>(indices.size());

  glBindVertexArray(0);
}

void MeshRenderer::uploadTexturedMeshToGL(const std::vector<Eigen::Vector3f>& positions,
                                          const std::vector<Eigen::Vector2f>& uvs,
                                          const std::vector<GLuint>& indices,
                                          GLTexturedMesh& glMesh) {
  if (positions.empty() || uvs.empty() || indices.empty()) {
    glMesh.indexCount = 0;
    glBindVertexArray(0);
    return;
  }

  glBindVertexArray(glMesh.vao);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(positions.size() * sizeof(Eigen::Vector3f)),
               positions.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector3f), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.tbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(uvs.size() * sizeof(Eigen::Vector2f)),
               uvs.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector2f), (void*)0);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)),
               indices.data(),
               GL_STATIC_DRAW);
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
    createGLAtlasMesh(earthAtlasMesh_);
    createGLTexturedMesh(earthSurfaceMesh_);
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
  buildEarthSurfaceMesh();
  bakeDualCellColors();
  rebuildEarthTileAtlases();
  buildEarthAtlasMesh();
  buildEarthMesh();
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
  morphMvpLocation_ = mvpLocation_;
  morphFactorLocation_ = glGetUniformLocation(shaderProgram_, "uMorphFactor");
  morphAnchorLocation_ = glGetUniformLocation(shaderProgram_, "uMorphAnchor");
  morphEastLocation_ = glGetUniformLocation(shaderProgram_, "uMorphEast");
  morphNorthLocation_ = glGetUniformLocation(shaderProgram_, "uMorphNorth");
  morphRadiusLocation_ = glGetUniformLocation(shaderProgram_, "uSphereRadius");
  morphStartAngleLocation_ = glGetUniformLocation(shaderProgram_, "uMorphStartAngle");
  morphEndAngleLocation_ = glGetUniformLocation(shaderProgram_, "uMorphEndAngle");

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
  colorMorphFactorLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphFactor");
  colorMorphAnchorLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphAnchor");
  colorMorphEastLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphEast");
  colorMorphNorthLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphNorth");
  colorMorphRadiusLocation_ = glGetUniformLocation(colorShaderProgram_, "uSphereRadius");
  colorMorphStartAngleLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphStartAngle");
  colorMorphEndAngleLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphEndAngle");

  GLuint atlasVertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(atlasVertexShader, 1, &atlasVertexShaderSource, nullptr);
  glCompileShader(atlasVertexShader);
  glGetShaderiv(atlasVertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(atlasVertexShader, 512, nullptr, infoLog);
    std::cerr << "Atlas vertex shader compilation failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Atlas vertex shader compilation failed");
  }

  GLuint atlasFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(atlasFragmentShader, 1, &atlasFragmentShaderSource, nullptr);
  glCompileShader(atlasFragmentShader);
  glGetShaderiv(atlasFragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(atlasFragmentShader, 512, nullptr, infoLog);
    std::cerr << "Atlas fragment shader compilation failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Atlas fragment shader compilation failed");
  }

  atlasShaderProgram_ = glCreateProgram();
  glAttachShader(atlasShaderProgram_, atlasVertexShader);
  glAttachShader(atlasShaderProgram_, atlasFragmentShader);
  glLinkProgram(atlasShaderProgram_);
  glGetProgramiv(atlasShaderProgram_, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(atlasShaderProgram_, 512, nullptr, infoLog);
    std::cerr << "Atlas shader program linking failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Atlas shader program linking failed");
  }

  glDeleteShader(atlasVertexShader);
  glDeleteShader(atlasFragmentShader);
  atlasMvpLocation_ = glGetUniformLocation(atlasShaderProgram_, "uMVP");
  atlasTextureCountLocation_ = glGetUniformLocation(atlasShaderProgram_, "uAtlasTextureCount");
  atlasLightDirLocation_ = glGetUniformLocation(atlasShaderProgram_, "uLightDir");
  for (int i = 0; i < kMaxTileAtlases; ++i) {
    std::string uniformName = "uAtlasTextures[" + std::to_string(i) + "]";
    atlasSamplerLocations_[i] = glGetUniformLocation(atlasShaderProgram_, uniformName.c_str());
  }
  atlasMorphFactorLocation_ = glGetUniformLocation(atlasShaderProgram_, "uMorphFactor");
  atlasMorphAnchorLocation_ = glGetUniformLocation(atlasShaderProgram_, "uMorphAnchor");
  atlasMorphEastLocation_ = glGetUniformLocation(atlasShaderProgram_, "uMorphEast");
  atlasMorphNorthLocation_ = glGetUniformLocation(atlasShaderProgram_, "uMorphNorth");
  atlasMorphRadiusLocation_ = glGetUniformLocation(atlasShaderProgram_, "uSphereRadius");
  atlasMorphStartAngleLocation_ = glGetUniformLocation(atlasShaderProgram_, "uMorphStartAngle");
  atlasMorphEndAngleLocation_ = glGetUniformLocation(atlasShaderProgram_, "uMorphEndAngle");

  GLuint texturedVertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(texturedVertexShader, 1, &texturedVertexShaderSource, nullptr);
  glCompileShader(texturedVertexShader);
  glGetShaderiv(texturedVertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(texturedVertexShader, 512, nullptr, infoLog);
    std::cerr << "Textured vertex shader compilation failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Textured vertex shader compilation failed");
  }

  GLuint texturedFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(texturedFragmentShader, 1, &texturedFragmentShaderSource, nullptr);
  glCompileShader(texturedFragmentShader);
  glGetShaderiv(texturedFragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(texturedFragmentShader, 512, nullptr, infoLog);
    std::cerr << "Textured fragment shader compilation failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Textured fragment shader compilation failed");
  }

  texturedShaderProgram_ = glCreateProgram();
  glAttachShader(texturedShaderProgram_, texturedVertexShader);
  glAttachShader(texturedShaderProgram_, texturedFragmentShader);
  glLinkProgram(texturedShaderProgram_);
  glGetProgramiv(texturedShaderProgram_, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(texturedShaderProgram_, 512, nullptr, infoLog);
    std::cerr << "Textured shader program linking failed:\n" << infoLog << std::endl;
    throw std::runtime_error("Textured shader program linking failed");
  }

  glDeleteShader(texturedVertexShader);
  glDeleteShader(texturedFragmentShader);
  texturedMvpLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMVP");
  texturedMorphFactorLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMorphFactor");
  texturedMorphAnchorLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMorphAnchor");
  texturedMorphEastLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMorphEast");
  texturedMorphNorthLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMorphNorth");
  texturedMorphRadiusLocation_ = glGetUniformLocation(texturedShaderProgram_, "uSphereRadius");
  texturedMorphStartAngleLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMorphStartAngle");
  texturedMorphEndAngleLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMorphEndAngle");
  texturedSamplerLocation_ = glGetUniformLocation(texturedShaderProgram_, "uEarthTexture");
  texturedUseTextureLocation_ = glGetUniformLocation(texturedShaderProgram_, "uUseTexture");
  texturedFallbackColorLocation_ = glGetUniformLocation(texturedShaderProgram_, "uFallbackColor");
}

void MeshRenderer::applyMorphUniforms(GLuint program,
                                      const glm::mat4& mvpMatrix,
                                      const Eigen::Vector3f& cameraPosition,
                                      float morphFactor) const {
  Eigen::Vector3f anchor = cameraPosition.normalized();
  Eigen::Vector3f ref(0.0f, 0.0f, 1.0f);
  if (std::abs(anchor.dot(ref)) > 0.95f) {
    ref = Eigen::Vector3f(0.0f, 1.0f, 0.0f);
  }
  Eigen::Vector3f east = ref.cross(anchor).normalized();
  Eigen::Vector3f north = anchor.cross(east).normalized();
  const float sphereRadius = mesh_ ? static_cast<float>(mesh_->getRadius()) : 1.0f;

  GLint mvpLocation = morphMvpLocation_;
  GLint factorLocation = morphFactorLocation_;
  GLint anchorLocation = morphAnchorLocation_;
  GLint eastLocation = morphEastLocation_;
  GLint northLocation = morphNorthLocation_;
  GLint radiusLocation = morphRadiusLocation_;
  GLint startAngleLocation = morphStartAngleLocation_;
  GLint endAngleLocation = morphEndAngleLocation_;
  if (program == colorShaderProgram_) {
    mvpLocation = colorMvpLocation_;
    factorLocation = colorMorphFactorLocation_;
    anchorLocation = colorMorphAnchorLocation_;
    eastLocation = colorMorphEastLocation_;
    northLocation = colorMorphNorthLocation_;
    radiusLocation = colorMorphRadiusLocation_;
    startAngleLocation = colorMorphStartAngleLocation_;
    endAngleLocation = colorMorphEndAngleLocation_;
  } else if (program == atlasShaderProgram_) {
    mvpLocation = atlasMvpLocation_;
    factorLocation = atlasMorphFactorLocation_;
    anchorLocation = atlasMorphAnchorLocation_;
    eastLocation = atlasMorphEastLocation_;
    northLocation = atlasMorphNorthLocation_;
    radiusLocation = atlasMorphRadiusLocation_;
    startAngleLocation = atlasMorphStartAngleLocation_;
    endAngleLocation = atlasMorphEndAngleLocation_;
  } else if (program == texturedShaderProgram_) {
    mvpLocation = texturedMvpLocation_;
    factorLocation = texturedMorphFactorLocation_;
    anchorLocation = texturedMorphAnchorLocation_;
    eastLocation = texturedMorphEastLocation_;
    northLocation = texturedMorphNorthLocation_;
    radiusLocation = texturedMorphRadiusLocation_;
    startAngleLocation = texturedMorphStartAngleLocation_;
    endAngleLocation = texturedMorphEndAngleLocation_;
  }

  glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, &mvpMatrix[0][0]);
  glUniform1f(factorLocation, morphFactor);
  glUniform3fv(anchorLocation, 1, anchor.data());
  glUniform3fv(eastLocation, 1, east.data());
  glUniform3fv(northLocation, 1, north.data());
  glUniform1f(radiusLocation, sphereRadius);
  glUniform1f(startAngleLocation, kMorphStartAngle);
  glUniform1f(endAngleLocation, kMorphEndAngle);
}

void MeshRenderer::renderMesh(const GLMesh& mesh, const glm::mat4& mvpMatrix, const glm::vec3& color) {
  (void)mvpMatrix;
  glUseProgram(shaderProgram_);
  glUniform3fv(colorLocation_, 1, &color[0]);
  glBindVertexArray(mesh.vao);
  glDrawElements(GL_LINES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void MeshRenderer::renderColoredMesh(const GLColorMesh& mesh, const glm::mat4& mvpMatrix, float shadingStrength) {
  (void)mvpMatrix;
  (void)shadingStrength;
  glUseProgram(colorShaderProgram_);

  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f, 1.0f);
  glBindVertexArray(mesh.vao);
  glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  glDisable(GL_POLYGON_OFFSET_FILL);
}

void MeshRenderer::renderAtlasMesh(const GLAtlasMesh& mesh, const glm::mat4& mvpMatrix) {
  (void)mvpMatrix;
  glUseProgram(atlasShaderProgram_);
  glUniform1i(atlasTextureCountLocation_, static_cast<int>(earthTileAtlasIds_.size()));
  glUniform3f(atlasLightDirLocation_, 0.35f, 0.2f, 0.92f);
  for (std::size_t i = 0; i < earthTileAtlasIds_.size() && i < static_cast<std::size_t>(kMaxTileAtlases); ++i) {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
    glBindTexture(GL_TEXTURE_2D, earthTileAtlasIds_[i]);
    glUniform1i(atlasSamplerLocations_[i], static_cast<GLint>(i));
  }
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f, 1.0f);
  glBindVertexArray(mesh.vao);
  glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  glDisable(GL_POLYGON_OFFSET_FILL);
}

void MeshRenderer::renderTexturedMesh(const GLTexturedMesh& mesh, const glm::mat4& mvpMatrix) {
  (void)mvpMatrix;
  glUseProgram(texturedShaderProgram_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, earthTextureId_);
  glUniform1i(texturedSamplerLocation_, 0);
  glUniform1i(texturedUseTextureLocation_, earthTexture_.isLoaded() ? 1 : 0);
  glUniform3f(texturedFallbackColorLocation_, 0.2f, 0.25f, 0.3f);
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
  const int samplesPerAxis = getEarthBakeSamplesPerAxis();
  for (int y = 0; y < samplesPerAxis; ++y) {
    const double fy = (static_cast<double>(y) + 0.5) / static_cast<double>(samplesPerAxis);
    const double lat = minLat + (maxLat - minLat) * fy;
    for (int x = 0; x < samplesPerAxis; ++x) {
      const double fx = (static_cast<double>(x) + 0.5) / static_cast<double>(samplesPerAxis);
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
    for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
      const auto& face = dual.faces[faceIndex];
      const std::size_t degree = static_cast<std::size_t>(face.size());
      if (degree == 5) {
        dualCellColors_.emplace_back(0.78f, 0.66f, 0.34f);
      } else {
        dualCellColors_.emplace_back(0.23f, 0.48f, 0.31f);
      }
    }
    return;
  }

  for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
    const auto& face = dual.faces[faceIndex];
    dualCellColors_.push_back(sampleDualFaceColor(face));
  }
}

void MeshRenderer::buildEarthMesh() {
  if (!mesh_) {
    earthMesh_.indexCount = 0;
    return;
  }

  const Mesh& dual = mesh_->getDualMesh();
  std::vector<Eigen::Vector3f> positions;
  std::vector<Eigen::Vector3f> colors;
  std::vector<GLuint> indices;
  positions.reserve(dual.faces.size() * 6);
  colors.reserve(dual.faces.size() * 6);
  indices.reserve(dual.faces.size() * 12);

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
      positions.push_back(dual.vertices.col(face[i]).cast<float>());
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

Eigen::Vector3f MeshRenderer::sampleEarthAtDirection(const Eigen::Vector3f& direction) const {
  const Eigen::Vector3d point = direction.cast<double>();
  const Eigen::Vector2d latLon = cartesianToLatLon(point);
  const float u = static_cast<float>((latLon.y() + kPiD) / (2.0 * kPiD));
  const float v = static_cast<float>((kPiD * 0.5 - latLon.x()) / kPiD);
  return earthTexture_.sampleBilinear(u, v);
}

void MeshRenderer::rebuildEarthTileAtlases() {
  tileVisuals_.clear();
  if (!mesh_) {
    return;
  }
  const Mesh& dual = mesh_->getDualMesh();
  tileVisuals_.resize(dual.faces.size());

  if (!earthTileAtlasIds_.empty()) {
    glDeleteTextures(static_cast<GLsizei>(earthTileAtlasIds_.size()), earthTileAtlasIds_.data());
    earthTileAtlasIds_.clear();
  }

  if (!earthTexture_.isLoaded()) {
    for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
      tileVisuals_[faceIndex].baseColor = faceIndex < dualCellColors_.size() ? dualCellColors_[faceIndex] : Eigen::Vector3f(0.4f, 0.45f, 0.5f);
    }
    return;
  }

  const int atlasTilesPerAxis = kTileAtlasSize / kTileTextureStride;
  const int tilesPerAtlas = atlasTilesPerAxis * atlasTilesPerAxis;
  const int atlasCount = static_cast<int>((dual.faces.size() + tilesPerAtlas - 1) / tilesPerAtlas);
  if (atlasCount > kMaxTileAtlases) {
    throw std::runtime_error("Tile atlas count exceeds supported limit");
  }

  std::vector<std::vector<unsigned char>> atlasPixels(static_cast<std::size_t>(atlasCount),
                                                      std::vector<unsigned char>(static_cast<std::size_t>(kTileAtlasSize * kTileAtlasSize * 3), 0));

  for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
    const Face& face = dual.faces[faceIndex];
    TileVisual visual;
    visual.baseColor = faceIndex < dualCellColors_.size() ? dualCellColors_[faceIndex] : Eigen::Vector3f(0.4f, 0.45f, 0.5f);
    const int atlasIndex = static_cast<int>(faceIndex / tilesPerAtlas);
    const int tileSlot = static_cast<int>(faceIndex % tilesPerAtlas);
    const int tileX = tileSlot % atlasTilesPerAxis;
    const int tileY = tileSlot / atlasTilesPerAxis;
    const int pixelX = tileX * kTileTextureStride + kTileTexturePadding;
    const int pixelY = tileY * kTileTextureStride + kTileTexturePadding;

    Eigen::Vector3f centroid = Eigen::Vector3f::Zero();
    for (int i = 0; i < face.size(); ++i) {
      centroid += dual.vertices.col(face[i]).cast<float>();
    }
    centroid.normalize();
    Eigen::Vector3f ref(0.0f, 0.0f, 1.0f);
    if (std::abs(centroid.dot(ref)) > 0.95f) {
      ref = Eigen::Vector3f(0.0f, 1.0f, 0.0f);
    }
    const Eigen::Vector3f tangentX = ref.cross(centroid).normalized();
    const Eigen::Vector3f tangentY = centroid.cross(tangentX).normalized();

    float maxRadius = 1e-4f;
    for (int i = 0; i < face.size(); ++i) {
      const Eigen::Vector3f position = dual.vertices.col(face[i]).cast<float>();
      const Eigen::Vector3f delta = position - centroid * position.dot(centroid);
      const Eigen::Vector2f local(delta.dot(tangentX), delta.dot(tangentY));
      maxRadius = std::max(maxRadius, local.norm());
    }

    auto& pixels = atlasPixels[static_cast<std::size_t>(atlasIndex)];
    for (int py = 0; py < kTileTextureResolution; ++py) {
      for (int px = 0; px < kTileTextureResolution; ++px) {
        const float u = (static_cast<float>(px) + 0.5f) / static_cast<float>(kTileTextureResolution);
        const float v = (static_cast<float>(py) + 0.5f) / static_cast<float>(kTileTextureResolution);
        const float localX = (u * 2.0f - 1.0f) * maxRadius;
        const float localY = (v * 2.0f - 1.0f) * maxRadius;
        Eigen::Vector3f direction = (centroid + tangentX * localX + tangentY * localY).normalized();
        const Eigen::Vector3f sample = sampleEarthAtDirection(direction);
        const int atlasPx = pixelX + px;
        const int atlasPy = pixelY + py;
        const std::size_t offset = static_cast<std::size_t>((atlasPy * kTileAtlasSize + atlasPx) * 3);
        pixels[offset] = static_cast<unsigned char>(std::clamp(sample.x(), 0.0f, 1.0f) * 255.0f);
        pixels[offset + 1] = static_cast<unsigned char>(std::clamp(sample.y(), 0.0f, 1.0f) * 255.0f);
        pixels[offset + 2] = static_cast<unsigned char>(std::clamp(sample.z(), 0.0f, 1.0f) * 255.0f);
      }
    }

    for (int px = 0; px < kTileTextureResolution; ++px) {
      for (int c = 0; c < 3; ++c) {
        const std::size_t srcTop = static_cast<std::size_t>(((pixelY) * kTileAtlasSize + (pixelX + px)) * 3 + c);
        const std::size_t dstTop = static_cast<std::size_t>(((pixelY - 1) * kTileAtlasSize + (pixelX + px)) * 3 + c);
        const std::size_t srcBottom = static_cast<std::size_t>(((pixelY + kTileTextureResolution - 1) * kTileAtlasSize + (pixelX + px)) * 3 + c);
        const std::size_t dstBottom = static_cast<std::size_t>(((pixelY + kTileTextureResolution) * kTileAtlasSize + (pixelX + px)) * 3 + c);
        pixels[dstTop] = pixels[srcTop];
        pixels[dstBottom] = pixels[srcBottom];
      }
    }
    for (int py = -1; py <= kTileTextureResolution; ++py) {
      for (int c = 0; c < 3; ++c) {
        const int clampedPy = std::clamp(pixelY + py, pixelY, pixelY + kTileTextureResolution - 1);
        const std::size_t srcLeft = static_cast<std::size_t>((clampedPy * kTileAtlasSize + pixelX) * 3 + c);
        const std::size_t dstLeft = static_cast<std::size_t>((clampedPy * kTileAtlasSize + (pixelX - 1)) * 3 + c);
        const std::size_t srcRight = static_cast<std::size_t>((clampedPy * kTileAtlasSize + (pixelX + kTileTextureResolution - 1)) * 3 + c);
        const std::size_t dstRight = static_cast<std::size_t>((clampedPy * kTileAtlasSize + (pixelX + kTileTextureResolution)) * 3 + c);
        pixels[dstLeft] = pixels[srcLeft];
        pixels[dstRight] = pixels[srcRight];
      }
    }

    visual.atlasIndex = atlasIndex;
    visual.atlasUvMin = Eigen::Vector2f(
      static_cast<float>(pixelX) / static_cast<float>(kTileAtlasSize),
      static_cast<float>(pixelY) / static_cast<float>(kTileAtlasSize));
    visual.atlasUvMax = Eigen::Vector2f(
      static_cast<float>(pixelX + kTileTextureResolution) / static_cast<float>(kTileAtlasSize),
      static_cast<float>(pixelY + kTileTextureResolution) / static_cast<float>(kTileAtlasSize));
    tileVisuals_[faceIndex] = visual;
  }

  earthTileAtlasIds_.resize(static_cast<std::size_t>(atlasCount), 0);
  glGenTextures(atlasCount, earthTileAtlasIds_.data());
  for (int atlasIndex = 0; atlasIndex < atlasCount; ++atlasIndex) {
    glBindTexture(GL_TEXTURE_2D, earthTileAtlasIds_[static_cast<std::size_t>(atlasIndex)]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, kTileAtlasSize, kTileAtlasSize, 0, GL_RGB, GL_UNSIGNED_BYTE, atlasPixels[static_cast<std::size_t>(atlasIndex)].data());
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

void MeshRenderer::buildEarthAtlasMesh() {
  if (!mesh_) {
    earthAtlasMesh_.indexCount = 0;
    return;
  }

  const Mesh& dual = mesh_->getDualMesh();
  std::vector<Eigen::Vector3f> positions;
  std::vector<Eigen::Vector2f> atlasUvs;
  std::vector<float> atlasIndices;
  std::vector<Eigen::Vector3f> fallbackColors;
  std::vector<GLuint> indices;
  positions.reserve(dual.faces.size() * 6);
  atlasUvs.reserve(dual.faces.size() * 6);
  atlasIndices.reserve(dual.faces.size() * 6);
  fallbackColors.reserve(dual.faces.size() * 6);
  indices.reserve(dual.faces.size() * 12);

  for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
    const Face& face = dual.faces[faceIndex];
    if (face.size() < 3) {
      continue;
    }
    const GLuint baseIndex = static_cast<GLuint>(positions.size());
    const TileVisual visual = faceIndex < tileVisuals_.size() ? tileVisuals_[faceIndex] : TileVisual{};
    Eigen::Vector3f centroid = Eigen::Vector3f::Zero();
    for (int i = 0; i < face.size(); ++i) {
      centroid += dual.vertices.col(face[i]).cast<float>();
    }
    centroid.normalize();
    Eigen::Vector3f ref(0.0f, 0.0f, 1.0f);
    if (std::abs(centroid.dot(ref)) > 0.95f) {
      ref = Eigen::Vector3f(0.0f, 1.0f, 0.0f);
    }
    const Eigen::Vector3f tangentX = ref.cross(centroid).normalized();
    const Eigen::Vector3f tangentY = centroid.cross(tangentX).normalized();

    std::vector<Eigen::Vector2f> projected;
    projected.reserve(static_cast<std::size_t>(face.size()));
    float maxRadius = 1e-4f;
    for (int i = 0; i < face.size(); ++i) {
      const Eigen::Vector3f position = dual.vertices.col(face[i]).cast<float>();
      const Eigen::Vector3f delta = position - centroid * position.dot(centroid);
      const Eigen::Vector2f uv(delta.dot(tangentX), delta.dot(tangentY));
      projected.push_back(uv);
      maxRadius = std::max(maxRadius, uv.norm());
    }

    for (int i = 0; i < face.size(); ++i) {
      const Eigen::Vector2f local = projected[static_cast<std::size_t>(i)] / maxRadius;
      const Eigen::Vector2f uv01 = (local + Eigen::Vector2f::Ones()) * 0.5f;
      positions.push_back(dual.vertices.col(face[i]).cast<float>());
      atlasUvs.push_back(visual.atlasUvMin + uv01.cwiseProduct(visual.atlasUvMax - visual.atlasUvMin));
      atlasIndices.push_back(static_cast<float>(visual.atlasIndex));
      fallbackColors.push_back(visual.baseColor);
    }

    for (int i = 1; i < face.size() - 1; ++i) {
      indices.push_back(baseIndex);
      indices.push_back(baseIndex + static_cast<GLuint>(i));
      indices.push_back(baseIndex + static_cast<GLuint>(i + 1));
    }
  }

  uploadAtlasMeshToGL(positions, atlasUvs, atlasIndices, fallbackColors, indices, earthAtlasMesh_);
}

void MeshRenderer::buildEarthSurfaceMesh() {
  if (!mesh_) {
    earthSurfaceMesh_.indexCount = 0;
    return;
  }

  const Mesh& subdivision = mesh_->getSubdividedMesh();
  std::vector<Eigen::Vector3f> positions;
  std::vector<Eigen::Vector2f> uvs;
  std::vector<GLuint> indices;
  positions.reserve(subdivision.faces.size() * 3);
  uvs.reserve(subdivision.faces.size() * 3);
  indices.reserve(subdivision.faces.size() * 3);

  for (const auto& face : subdivision.faces) {
    if (face.size() != 3) {
      continue;
    }
    const GLuint baseIndex = static_cast<GLuint>(positions.size());
    Eigen::Vector2f triUvs[3];
    for (int i = 0; i < 3; ++i) {
      const Eigen::Vector3d vertex = subdivision.vertices.col(face[i]);
      const Eigen::Vector3f position = vertex.cast<float>();
      const Eigen::Vector2d latLon = cartesianToLatLon(vertex);
      const float u = static_cast<float>((latLon.y() + kPiD) / (2.0 * kPiD));
      const float v = static_cast<float>((kPiD * 0.5 - latLon.x()) / kPiD);
      positions.push_back(position);
      triUvs[i] = Eigen::Vector2f(u, v);
    }
    float minU = triUvs[0].x();
    float maxU = triUvs[0].x();
    for (int i = 1; i < 3; ++i) {
      minU = std::min(minU, triUvs[i].x());
      maxU = std::max(maxU, triUvs[i].x());
    }
    if (maxU - minU > 0.5f) {
      for (auto& triUv : triUvs) {
        if (triUv.x() < 0.5f) {
          triUv.x() += 1.0f;
        }
      }
    }
    for (int i = 0; i < 3; ++i) {
      uvs.push_back(triUvs[i]);
      indices.push_back(baseIndex + static_cast<GLuint>(i));
    }
  }

  uploadTexturedMeshToGL(positions, uvs, indices, earthSurfaceMesh_);
}

void MeshRenderer::uploadEarthTextureToGL() {
  if (!earthTextureId_) {
    glGenTextures(1, &earthTextureId_);
  }
  glBindTexture(GL_TEXTURE_2D, earthTextureId_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (earthTexture_.isLoaded() && earthTexture_.data()) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB8,
                 earthTexture_.width(),
                 earthTexture_.height(),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 earthTexture_.data());
  } else {
    const unsigned char fallback[3] = {51, 64, 77};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, fallback);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

void MeshRenderer::renderAllMeshes(const glm::mat4& mvpMatrix, const Visibility& vis, const Eigen::Vector3f& cameraPosition, float morphFactor) {
  if (vis.isReferenceGlobeVisible_) {
    glUseProgram(texturedShaderProgram_);
    applyMorphUniforms(texturedShaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderTexturedMesh(earthSurfaceMesh_, mvpMatrix);
  }
  if (vis.isEarthVisible_) {
    glUseProgram(atlasShaderProgram_);
    applyMorphUniforms(atlasShaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderAtlasMesh(earthAtlasMesh_, mvpMatrix);
  }
  if (vis.isEarthDebugVisible_) {
    glUseProgram(colorShaderProgram_);
    applyMorphUniforms(colorShaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderColoredMesh(earthMesh_, mvpMatrix, 0.0f);
  }
  if (vis.isIcosahedronVisible_) {
    glUseProgram(shaderProgram_);
    applyMorphUniforms(shaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderMesh(icosahedronMesh_, mvpMatrix, cIco);
  }
  if (vis.isSubdivisionVisible_) {
    glUseProgram(shaderProgram_);
    applyMorphUniforms(shaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderMesh(subdivisionMesh_, mvpMatrix, cSub);
  }
  if (vis.isPrimalMeshVisible_) {
    glUseProgram(shaderProgram_);
    applyMorphUniforms(shaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderMesh(primalMesh_, mvpMatrix, cPN);
  }
  if (vis.isPrimalDebugMeshVisible_) {
    glUseProgram(shaderProgram_);
    applyMorphUniforms(shaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderMesh(primalDebugMesh_, mvpMatrix, cPD);
  }
  if (vis.isDualMeshVisible_) {
    glUseProgram(shaderProgram_);
    applyMorphUniforms(shaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderMesh(dualMesh_, mvpMatrix, cDual);
  }
}

void MeshRenderer::setPerformanceMode(PerformanceMode mode) {
  if (performanceMode_ == mode) {
    return;
  }
  performanceMode_ = mode;
  if (mesh_) {
    bakeDualCellColors();
    rebuildEarthTileAtlases();
    buildEarthAtlasMesh();
    buildEarthMesh();
  }
}
} // namespace spherical_tiling
