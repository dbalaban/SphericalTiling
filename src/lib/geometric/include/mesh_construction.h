#pragma once

#include "templated_geometry.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct Mesh;
class MeshConstructor;

struct DualTopologyReport {
  bool valid = true;
  std::size_t expectedDualCells = 0;
  std::size_t actualDualCells = 0;
  std::size_t pentagons = 0;
  std::size_t hexagons = 0;
  std::size_t otherCells = 0;
  std::vector<std::string> errors;
};

typedef std::shared_ptr<Mesh> MeshPtr;
typedef std::shared_ptr<const Mesh> ConstMeshPtr;
typedef std::shared_ptr<MeshConstructor> MeshConstructorPtr;
typedef std::shared_ptr<const MeshConstructor> ConstMeshConstructorPtr;

typedef Eigen::Vector3d Vertex;
// list of vertex indices, ccw ordered
typedef Eigen::VectorXi Face;
// two vertex indices, ordered (v1 < v2)
typedef Eigen::Vector2i Edge;
typedef Eigen::Matrix3Xd Vertices;
typedef std::vector<Face> Faces;
typedef std::vector<Edge> Edges;

struct Mesh {
  Vertices vertices; // 3xN matrix of vertex positions
  Edges edges;       // list of connected edges (pairs of vertex indices)
  Faces faces;       // polygon loops; primal mesh stores neighbor rings per vertex
};

class MeshConstructor {
public:
  MeshConstructor(double R, uint16_t q);
  ~MeshConstructor() = default;

  const Mesh& getIcosahedron() const {
    return _icosahedron;
  }

  const Mesh& getSubdividedMesh() const {
    return _subdivided;
  }

  const Mesh& getPrimalMesh() const {
    return _primal;
  }

  const Mesh& getDualMesh() const {
    return _dual;
  }

  double getRadius() const {
    return _radius;
  }

  uint16_t getFrequency() const {
    return _frequency;
  }

private:
  double _radius;
  uint16_t _frequency;

  Mesh _icosahedron;
  Mesh _subdivided;
  Mesh _primal;
  Mesh _dual;

  void getEdgesFromFaces(const Faces& faces, Edges& edges);
  void sortFaceVertices(Faces& faces, const Vertices& V, const Vertices& Origin);
  void generateIcosahedron();
  void subdivideIcosahedron();
  void projectSubdivisions();
  void constructDualCells();
};

DualTopologyReport validateDualTopology(const MeshConstructor& constructor);
