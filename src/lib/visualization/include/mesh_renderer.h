#pragma once

#include <Eigen/Dense>
#include <vector>
#include <glad/gl.h>
#include <glm/glm.hpp>

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

class MeshRenderer {
public:
  MeshRenderer();
  ~MeshRenderer();
    
  // Set mesh data for primal (triangular) mesh
  void setMeshConstruct(ConstMeshConstructorPtr mesh);
  void setMeshConstruct(MeshConstructorPtr mesh);
    
  // Render meshes
  void renderMesh(const GLMesh& mesh, const glm::mat4& mvpMatrix, const glm::vec3& color);
  void renderAllMeshes(const glm::mat4& mvpMatrix, const Visibility& vis);
    
private:
  void setupShaders();

  void createGLMesh(GLMesh& glMesh);
  void uploadMeshes();
  void uploadMeshToGL(const Vertices& V, const Edges& E, GLMesh& glMesh);

  ConstMeshConstructorPtr mesh_;
    
  // Shader program
  GLuint shaderProgram_;
  GLint mvpLocation_;
  GLint colorLocation_;

  // GL Meshes
  GLMesh icosahedronMesh_;
  GLMesh subdivisionMesh_;
  GLMesh primalMesh_;
  GLMesh primalDebugMesh_;
  GLMesh dualMesh_;

  glm::vec3 cIco{0.9f,0.8f,0.2f};
  glm::vec3 cSub{0.1f,0.7f,0.9f};
  glm::vec3 cPN{0.9f,0.2f,0.2f};
  glm::vec3 cPD{0.9f,0.4f,0.9f};
  glm::vec3 cDual{0.1f,0.1f,0.1f}; 

  bool viewsCreated_;
};

} // namespace spherical_tiling
