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
layout (location = 2) in vec2 aLocalCoord;
layout (location = 3) in vec4 aParamsA;
layout (location = 4) in vec4 aParamsB;

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
out vec2 vLocalCoord;
out vec4 vParamsA;
out vec4 vParamsB;

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
    vLocalCoord = aLocalCoord;
    vParamsA = aParamsA;
    vParamsB = aParamsB;
}
)";

static const char* colorFragmentShaderSource = R"(
#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec2 vLocalCoord;
in vec4 vParamsA;
in vec4 vParamsB;
out vec4 FragColor;

uniform float uShadingStrength;
uniform vec3 uLightDir;
uniform int uGeneratedMode;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p) {
    float sum = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 4; ++i) {
        sum += amplitude * valueNoise(p * frequency);
        frequency *= 2.03;
        amplitude *= 0.5;
    }
    return sum;
}

void main() {
    float lambert = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    float shade = mix(1.0, 0.72 + 0.28 * lambert, uShadingStrength);
    vec3 color = vColor;
    if (uGeneratedMode != 0) {
        float waterBlend = vParamsA.x;
        float luminanceBias = vParamsA.y;
        float contrast = vParamsA.z;
        float edgeDarkening = vParamsA.w;
        float noiseScale = vParamsB.x;
        float noiseAmplitude = vParamsB.y;
        float seed = vParamsB.z;

        vec2 centered = vLocalCoord;
        float radius = length(centered);
        float edge = smoothstep(0.72, 1.0, radius);
        vec2 warped = centered * noiseScale + vec2(seed, seed * 0.618);
        float coarse = fbm(warped);
        float fine = fbm(warped * 2.4 + vec2(3.1, -1.7));
        float ridges = 1.0 - abs(2.0 * coarse - 1.0);
        float contour = smoothstep(0.35, 0.85, sin((centered.x + centered.y) * 8.0 + seed * 6.28318) * 0.5 + 0.5);
        float signedNoise = (fine - 0.5) * 2.0;
        float landDetail = mix(contour * 0.04 + ridges * 0.14, coarse * 0.05, waterBlend);
        float variation = signedNoise * noiseAmplitude + landDetail;
        color = color + vec3(variation);
        color = mix(color, color * vec3(0.92, 0.98, 1.05), waterBlend * 0.35);
        color = mix(color, color * vec3(1.04, 1.01, 0.96), (1.0 - waterBlend) * 0.25);
        color = (color - vec3(0.5)) * contrast + vec3(0.5 + luminanceBias);
        color *= 1.0 - edge * edgeDarkening;
        color = clamp(color, 0.0, 1.0);
    }
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
      texturedShaderProgram_(0),
      earthTextureId_(0),
      mvpLocation_(-1),
      colorLocation_(-1),
      colorMvpLocation_(-1),
      colorShadingStrengthLocation_(-1),
      colorLightDirLocation_(-1),
      colorGeneratedModeLocation_(-1),
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
  setupShaders();
}

MeshRenderer::~MeshRenderer() {
  if (shaderProgram_) glDeleteProgram(shaderProgram_);
  if (colorShaderProgram_) glDeleteProgram(colorShaderProgram_);
  if (texturedShaderProgram_) glDeleteProgram(texturedShaderProgram_);
  if (earthTextureId_) glDeleteTextures(1, &earthTextureId_);
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
  glGenBuffers(1, &glMesh.lbo);
  glGenBuffers(1, &glMesh.pbo);
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
                                       const std::vector<Eigen::Vector2f>& localCoords,
                                       const std::vector<Eigen::Vector4f>& paramsA,
                                       const std::vector<Eigen::Vector4f>& paramsB,
                                       const std::vector<GLuint>& indices,
                                       GLColorMesh& glMesh) {
  if (positions.empty() || colors.empty() || localCoords.empty() || paramsA.empty() || paramsB.empty() || indices.empty()) {
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

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.lbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(localCoords.size() * sizeof(Eigen::Vector2f)),
               localCoords.data(),
               GL_STATIC_DRAW);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector2f), (void*)0);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, glMesh.pbo);
  const std::size_t paramsSize = paramsA.size() * sizeof(Eigen::Vector4f);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(paramsSize * 2),
               nullptr,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(paramsSize), paramsA.data());
  glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(paramsSize), static_cast<GLsizeiptr>(paramsSize), paramsB.data());
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector4f), (void*)0);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Eigen::Vector4f), reinterpret_cast<void*>(static_cast<uintptr_t>(paramsSize)));
  glEnableVertexAttribArray(4);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glMesh.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)),
               indices.data(),
               GL_STATIC_DRAW);
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
  colorShadingStrengthLocation_ = glGetUniformLocation(colorShaderProgram_, "uShadingStrength");
  colorLightDirLocation_ = glGetUniformLocation(colorShaderProgram_, "uLightDir");
  colorGeneratedModeLocation_ = glGetUniformLocation(colorShaderProgram_, "uGeneratedMode");
  colorMorphFactorLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphFactor");
  colorMorphAnchorLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphAnchor");
  colorMorphEastLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphEast");
  colorMorphNorthLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphNorth");
  colorMorphRadiusLocation_ = glGetUniformLocation(colorShaderProgram_, "uSphereRadius");
  colorMorphStartAngleLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphStartAngle");
  colorMorphEndAngleLocation_ = glGetUniformLocation(colorShaderProgram_, "uMorphEndAngle");

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
  glUseProgram(colorShaderProgram_);
  glUniform1f(colorShadingStrengthLocation_, shadingStrength);
  glUniform3f(colorLightDirLocation_, 0.35f, 0.2f, 0.92f);
  glUniform1i(colorGeneratedModeLocation_, shadingStrength > 0.0f ? 1 : 0);

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
  tileVisuals_.clear();
  if (!mesh_) {
    return;
  }

  const Mesh& dual = mesh_->getDualMesh();
  dualCellColors_.reserve(dual.faces.size());
  tileVisuals_.reserve(dual.faces.size());
  if (!earthTexture_.isLoaded()) {
    for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
      const auto& face = dual.faces[faceIndex];
      const std::size_t degree = static_cast<std::size_t>(face.size());
      TileVisual visual;
      if (degree == 5) {
        dualCellColors_.emplace_back(0.78f, 0.66f, 0.34f);
        visual.baseColor = dualCellColors_.back();
        visual.waterBlend = 0.0f;
      } else {
        dualCellColors_.emplace_back(0.23f, 0.48f, 0.31f);
        visual.baseColor = dualCellColors_.back();
        visual.waterBlend = 0.0f;
      }
      visual.seed = static_cast<float>((faceIndex * 37) % 997) / 997.0f * 11.0f;
      tileVisuals_.push_back(visual);
    }
    return;
  }

  for (std::size_t faceIndex = 0; faceIndex < dual.faces.size(); ++faceIndex) {
    const auto& face = dual.faces[faceIndex];
    const Eigen::Vector3f color = sampleDualFaceColor(face);
    dualCellColors_.push_back(color);

    TileVisual visual;
    visual.baseColor = color;
    const float brightness = color.dot(Eigen::Vector3f(0.2126f, 0.7152f, 0.0722f));
    const float blueExcess = std::max(0.0f, color.z() - 0.5f * (color.x() + color.y()));
    visual.waterBlend = std::clamp((blueExcess + (0.42f - brightness)) * 1.5f, 0.0f, 1.0f);
    visual.luminanceBias = visual.waterBlend > 0.5f ? -0.02f : 0.03f;
    visual.contrast = visual.waterBlend > 0.5f ? 1.08f : 1.14f;
    visual.edgeDarkening = visual.waterBlend > 0.5f ? 0.08f : 0.15f;
    visual.noiseScale = visual.waterBlend > 0.5f ? 4.5f : 3.2f;
    visual.noiseAmplitude = visual.waterBlend > 0.5f ? 0.035f : 0.065f;
    visual.seed = static_cast<float>((faceIndex * 37) % 997) / 997.0f * 11.0f;
    tileVisuals_.push_back(visual);
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
  std::vector<Eigen::Vector2f> localCoords;
  std::vector<Eigen::Vector4f> paramsA;
  std::vector<Eigen::Vector4f> paramsB;
  std::vector<GLuint> indices;
  positions.reserve(dual.faces.size() * 6);
  colors.reserve(dual.faces.size() * 6);
  localCoords.reserve(dual.faces.size() * 6);
  paramsA.reserve(dual.faces.size() * 6);
  paramsB.reserve(dual.faces.size() * 6);
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
    const TileVisual visual = faceIndex < tileVisuals_.size()
      ? tileVisuals_[faceIndex]
      : TileVisual{color};

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
      positions.push_back(dual.vertices.col(face[i]).cast<float>());
      colors.push_back(color);
      localCoords.push_back(projected[static_cast<std::size_t>(i)] / maxRadius);
      paramsA.emplace_back(visual.waterBlend, visual.luminanceBias, visual.contrast, visual.edgeDarkening);
      paramsB.emplace_back(visual.noiseScale, visual.noiseAmplitude, visual.seed, 0.0f);
    }

    for (int i = 1; i < face.size() - 1; ++i) {
      indices.push_back(baseIndex);
      indices.push_back(baseIndex + static_cast<GLuint>(i));
      indices.push_back(baseIndex + static_cast<GLuint>(i + 1));
    }
  }

  uploadColorMeshToGL(positions, colors, localCoords, paramsA, paramsB, indices, earthMesh_);
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
    glUseProgram(colorShaderProgram_);
    applyMorphUniforms(colorShaderProgram_, mvpMatrix, cameraPosition, morphFactor);
    renderColoredMesh(earthMesh_, mvpMatrix, 1.0f);
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
    buildEarthMesh();
  }
}
} // namespace spherical_tiling
