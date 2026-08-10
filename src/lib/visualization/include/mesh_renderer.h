#pragma once

#include <Eigen/Dense>
#include <glad/gl.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "earth_texture.h"
#include "mesh_construction.h"

namespace spherical_tiling {

enum MeshType {
  ICOSAHEDRON,
  SUBDIVISION,
  PRIMAL,
  PRIMAL_DEBUG,
  DUAL
};

enum class PerformanceMode {
  Quality = 0,
  Balanced,
  Performance
};

struct Visibility {
  bool isEarthVisible_;
  bool isEarthDebugVisible_;
  bool isReferenceGlobeVisible_;
  bool isIcosahedronVisible_;
  bool isSubdivisionVisible_;
  bool isPrimalMeshVisible_;
  bool isPrimalDebugMeshVisible_;
  bool isDualMeshVisible_;
};

struct GLMesh {
  GLuint vao=0, vbo=0, ebo=0;
  GLsizei indexCount=0;

  GLMesh() = default;
  GLMesh(const GLMesh&) = delete;
  GLMesh& operator=(const GLMesh&) = delete;

  GLMesh(GLMesh&& o) noexcept { *this = std::move(o); }
  GLMesh& operator=(GLMesh&& o) noexcept {
    if (this == &o) return *this;
    destroy();
    vao=o.vao; vbo=o.vbo; ebo=o.ebo; indexCount=o.indexCount;
    o.vao=o.vbo=o.ebo=0; o.indexCount=0;
    return *this;
  }

  ~GLMesh(){ destroy(); }
  void destroy(){
    if (vao) glDeleteVertexArrays(1,&vao);
    if (vbo) glDeleteBuffers(1,&vbo);
    if (ebo) glDeleteBuffers(1,&ebo);
    vao=vbo=ebo=0; indexCount=0;
  }
};

struct GLColorMesh {
  GLuint vao=0, vbo=0, cbo=0, lbo=0, pbo=0, ebo=0;
  GLsizei indexCount=0;

  GLColorMesh() = default;
  GLColorMesh(const GLColorMesh&) = delete;
  GLColorMesh& operator=(const GLColorMesh&) = delete;

  GLColorMesh(GLColorMesh&& o) noexcept { *this = std::move(o); }
  GLColorMesh& operator=(GLColorMesh&& o) noexcept {
    if (this == &o) return *this;
    destroy();
    vao=o.vao; vbo=o.vbo; cbo=o.cbo; lbo=o.lbo; pbo=o.pbo; ebo=o.ebo; indexCount=o.indexCount;
    o.vao=o.vbo=o.cbo=o.lbo=o.pbo=o.ebo=0; o.indexCount=0;
    return *this;
  }

  ~GLColorMesh(){ destroy(); }
  void destroy(){
    if (vao) glDeleteVertexArrays(1,&vao);
    if (vbo) glDeleteBuffers(1,&vbo);
    if (cbo) glDeleteBuffers(1,&cbo);
    if (lbo) glDeleteBuffers(1,&lbo);
    if (pbo) glDeleteBuffers(1,&pbo);
    if (ebo) glDeleteBuffers(1,&ebo);
    vao=vbo=cbo=lbo=pbo=ebo=0; indexCount=0;
  }
};

struct TileVisual {
  Eigen::Vector3f baseColor;
  float waterBlend = 0.0f;
  float luminanceBias = 0.0f;
  float contrast = 1.0f;
  float edgeDarkening = 0.12f;
  float noiseScale = 3.0f;
  float noiseAmplitude = 0.08f;
  float seed = 0.0f;
};

struct GLTexturedMesh {
  GLuint vao=0, vbo=0, tbo=0, ebo=0;
  GLsizei indexCount=0;

  GLTexturedMesh() = default;
  GLTexturedMesh(const GLTexturedMesh&) = delete;
  GLTexturedMesh& operator=(const GLTexturedMesh&) = delete;

  GLTexturedMesh(GLTexturedMesh&& o) noexcept { *this = std::move(o); }
  GLTexturedMesh& operator=(GLTexturedMesh&& o) noexcept {
    if (this == &o) return *this;
    destroy();
    vao=o.vao; vbo=o.vbo; tbo=o.tbo; ebo=o.ebo; indexCount=o.indexCount;
    o.vao=o.vbo=o.tbo=o.ebo=0; o.indexCount=0;
    return *this;
  }

  ~GLTexturedMesh(){ destroy(); }
  void destroy(){
    if (vao) glDeleteVertexArrays(1,&vao);
    if (vbo) glDeleteBuffers(1,&vbo);
    if (tbo) glDeleteBuffers(1,&tbo);
    if (ebo) glDeleteBuffers(1,&ebo);
    vao=vbo=tbo=ebo=0; indexCount=0;
  }
};

class MeshRenderer {
public:
  MeshRenderer();
  ~MeshRenderer();
    
  // Set mesh data for primal (triangular) mesh
  void setMeshConstruct(ConstMeshConstructorPtr mesh);
  void setMeshConstruct(MeshConstructorPtr mesh);

  bool loadEarthTexture(const std::string& path, std::string& error);
  const std::string& getEarthTexturePath() const;
  const DualTopologyReport& getValidationReport() const { return validation_; }
  void setPerformanceMode(PerformanceMode mode);
  PerformanceMode getPerformanceMode() const { return performanceMode_; }
  int getEarthBakeSamplesPerAxis() const;
    
  // Render meshes
  void renderMesh(const GLMesh& mesh, const glm::mat4& mvpMatrix, const glm::vec3& color);
  void renderColoredMesh(const GLColorMesh& mesh, const glm::mat4& mvpMatrix, float shadingStrength);
  void renderTexturedMesh(const GLTexturedMesh& mesh, const glm::mat4& mvpMatrix);
  void renderAllMeshes(const glm::mat4& mvpMatrix, const Visibility& vis, const Eigen::Vector3f& cameraPosition, float morphFactor);
    
private:
  void setupShaders();

  void createGLMesh(GLMesh& glMesh);
  void createGLColorMesh(GLColorMesh& glMesh);
  void createGLTexturedMesh(GLTexturedMesh& glMesh);
  void uploadMeshes();
  void uploadMeshToGL(const Vertices& V, const Edges& E, GLMesh& glMesh);
  void uploadColorMeshToGL(const std::vector<Eigen::Vector3f>& positions,
                           const std::vector<Eigen::Vector3f>& colors,
                           const std::vector<Eigen::Vector2f>& localCoords,
                           const std::vector<Eigen::Vector4f>& paramsA,
                           const std::vector<Eigen::Vector4f>& paramsB,
                           const std::vector<GLuint>& indices,
                           GLColorMesh& glMesh);
  void uploadTexturedMeshToGL(const std::vector<Eigen::Vector3f>& positions,
                              const std::vector<Eigen::Vector2f>& uvs,
                              const std::vector<GLuint>& indices,
                              GLTexturedMesh& glMesh);
  void buildEarthMesh();
  void buildEarthSurfaceMesh();
  void bakeDualCellColors();
  Eigen::Vector3f sampleDualFaceColor(const Face& face) const;
  bool pointInPolygon(const Eigen::Vector2d& point, const std::vector<Eigen::Vector2d>& polygon) const;
  float smoothstep(float edge0, float edge1, float value) const;
  void applyMorphUniforms(GLuint program, const glm::mat4& mvpMatrix, const Eigen::Vector3f& cameraPosition, float morphFactor) const;
  void uploadEarthTextureToGL();

  ConstMeshConstructorPtr mesh_;
  EarthTexture earthTexture_;
  DualTopologyReport validation_;
  std::vector<TileVisual> tileVisuals_;
  std::vector<Eigen::Vector3f> dualCellColors_;
  PerformanceMode performanceMode_;
    
  // Shader program
  GLuint shaderProgram_;
  GLuint colorShaderProgram_;
  GLuint texturedShaderProgram_;
  GLuint earthTextureId_;
  GLint mvpLocation_;
  GLint colorLocation_;
  GLint colorMvpLocation_;
  GLint colorShadingStrengthLocation_;
  GLint colorLightDirLocation_;
  GLint colorGeneratedModeLocation_;
  GLint texturedMvpLocation_;
  GLint morphMvpLocation_;
  GLint morphFactorLocation_;
  GLint morphAnchorLocation_;
  GLint morphEastLocation_;
  GLint morphNorthLocation_;
  GLint morphRadiusLocation_;
  GLint morphStartAngleLocation_;
  GLint morphEndAngleLocation_;
  GLint colorMorphFactorLocation_;
  GLint colorMorphAnchorLocation_;
  GLint colorMorphEastLocation_;
  GLint colorMorphNorthLocation_;
  GLint colorMorphRadiusLocation_;
  GLint colorMorphStartAngleLocation_;
  GLint colorMorphEndAngleLocation_;
  GLint texturedMorphFactorLocation_;
  GLint texturedMorphAnchorLocation_;
  GLint texturedMorphEastLocation_;
  GLint texturedMorphNorthLocation_;
  GLint texturedMorphRadiusLocation_;
  GLint texturedMorphStartAngleLocation_;
  GLint texturedMorphEndAngleLocation_;
  GLint texturedSamplerLocation_;
  GLint texturedUseTextureLocation_;
  GLint texturedFallbackColorLocation_;

  // GL Meshes
  GLMesh icosahedronMesh_;
  GLMesh subdivisionMesh_;
  GLMesh primalMesh_;
  GLMesh primalDebugMesh_;
  GLMesh dualMesh_;
  GLColorMesh earthMesh_;
  GLTexturedMesh earthSurfaceMesh_;

  glm::vec3 cIco{0.9f,0.8f,0.2f};
  glm::vec3 cSub{0.1f,0.7f,0.9f};
  glm::vec3 cPN{0.9f,0.2f,0.2f};
  glm::vec3 cPD{0.9f,0.4f,0.9f};
  glm::vec3 cDual{0.1f,0.1f,0.1f}; 

  bool viewsCreated_;
};

} // namespace spherical_tiling
