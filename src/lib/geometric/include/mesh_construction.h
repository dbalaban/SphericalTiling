#include "templated_geometry.h"

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
  Faces faces; // list of triangular faces (triplets of vertex indices)
};

class MeshConstructor {
public:
  MeshConstructor(double R, uint16_t q) : _radius(R), _frequency(q) {
    assert(_frequency > 0);
    generateIcosahedron();
    subdivideIcosahedron();
    projectSubdivisions();
    constructDualCells();
  }
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