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

struct Visibility {
  bool isEarthVisible_;
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
  GLuint vao=0, vbo=0, cbo=0, ebo=0;
  GLsizei indexCount=0;

  GLColorMesh() = default;
  GLColorMesh(const GLColorMesh&) = delete;
  GLColorMesh& operator=(const GLColorMesh&) = delete;

  GLColorMesh(GLColorMesh&& o) noexcept { *this = std::move(o); }
  GLColorMesh& operator=(GLColorMesh&& o) noexcept {
    if (this == &o) return *this;
    destroy();
    vao=o.vao; vbo=o.vbo; cbo=o.cbo; ebo=o.ebo; indexCount=o.indexCount;
    o.vao=o.vbo=o.cbo=o.ebo=0; o.indexCount=0;
    return *this;
  }

  ~GLColorMesh(){ destroy(); }
  void destroy(){
    if (vao) glDeleteVertexArrays(1,&vao);
    if (vbo) glDeleteBuffers(1,&vbo);
    if (cbo) glDeleteBuffers(1,&cbo);
    if (ebo) glDeleteBuffers(1,&ebo);
    vao=vbo=cbo=ebo=0; indexCount=0;
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
    
  // Render meshes
  void renderMesh(const GLMesh& mesh, const glm::mat4& mvpMatrix, const glm::vec3& color);
  void renderColoredMesh(const GLColorMesh& mesh, const glm::mat4& mvpMatrix);
  void renderAllMeshes(const glm::mat4& mvpMatrix, const Visibility& vis, const Eigen::Vector3f& cameraPosition, float morphFactor);
    
private:
  void setupShaders();

  void createGLMesh(GLMesh& glMesh);
  void createGLColorMesh(GLColorMesh& glMesh);
  void uploadMeshes();
  void uploadMeshToGL(const Vertices& V, const Edges& E, GLMesh& glMesh);
  void uploadColorMeshToGL(const std::vector<Eigen::Vector3f>& positions,
                           const std::vector<Eigen::Vector3f>& colors,
                           const std::vector<GLuint>& indices,
                           GLColorMesh& glMesh);
  void bakeDualCellColors();
  void updateDynamicEarthMeshes(const Eigen::Vector3f& cameraPosition, float morphFactor);
  Eigen::Vector3f sampleDualFaceColor(const Face& face) const;
  Eigen::Vector3f morphPosition(const Eigen::Vector3d& position,
                                const Eigen::Vector3f& anchor,
                                const Eigen::Vector3f& east,
                                const Eigen::Vector3f& north,
                                float morphFactor) const;
  bool pointInPolygon(const Eigen::Vector2d& point, const std::vector<Eigen::Vector2d>& polygon) const;
  float smoothstep(float edge0, float edge1, float value) const;

  ConstMeshConstructorPtr mesh_;
  EarthTexture earthTexture_;
  DualTopologyReport validation_;
  std::vector<Eigen::Vector3f> dualCellColors_;
    
  // Shader program
  GLuint shaderProgram_;
  GLuint colorShaderProgram_;
  GLint mvpLocation_;
  GLint colorLocation_;
  GLint colorMvpLocation_;

  // GL Meshes
  GLMesh icosahedronMesh_;
  GLMesh subdivisionMesh_;
  GLMesh primalMesh_;
  GLMesh primalDebugMesh_;
  GLMesh dualMesh_;
  GLColorMesh earthMesh_;

  glm::vec3 cIco{0.9f,0.8f,0.2f};
  glm::vec3 cSub{0.1f,0.7f,0.9f};
  glm::vec3 cPN{0.9f,0.2f,0.2f};
  glm::vec3 cPD{0.9f,0.4f,0.9f};
  glm::vec3 cDual{0.1f,0.1f,0.1f}; 

  bool viewsCreated_;
};

} // namespace spherical_tiling
