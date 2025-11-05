#include "mesh_construction.h"

#include <unordered_set>
#include <numbers>

struct EdgeHash {
  size_t operator()(const Edge& e) const noexcept {
    // e[0] <= e[1] guaranteed by caller
    size_t h = std::hash<int>{}(e[0]);
    // boost-like hash_combine
    h ^= std::hash<int>{}(e[1]) + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
    return h;
  }
};

struct Key { int64_t x, y, z; };
struct KeyHash {
  size_t operator()(const Key& k) const noexcept {
    // 64-bit mix
    uint64_t h = uint64_t(k.x) * 0x9e3779b97f4a7c15ULL
                ^ (uint64_t(k.y) + 0x85ebca6b) + (uint64_t(k.x) << 6) + (uint64_t(k.x) >> 2);
    h ^= uint64_t(k.z) + 0xc2b2ae35 + (h << 6) + (h >> 2);
    return size_t(h ^ (h >> 33));
  }
};
struct KeyEq {
  bool operator()(const Key& a, const Key& b) const noexcept {
    return a.x==b.x && a.y==b.y && a.z==b.z;
  }
};

inline Key quantize_unit(const Eigen::Vector3d& u) {
  // Quantize unit vector to ~1e-8 grid; adjust if you need more/less merging strictness
  constexpr double s = 1e8;
  return Key{
    (int64_t)std::llround(u.x() * s),
    (int64_t)std::llround(u.y() * s),
    (int64_t)std::llround(u.z() * s)
  };
};

void MeshConstructor::getEdgesFromFaces(const Faces& faces, Edges& edges) {
  std::unordered_set<Edge, EdgeHash> edgeSet;
  for (const auto& face : faces) {
    size_t numVerts = static_cast<size_t>(face.size());
    for (size_t i = 0; i < numVerts; ++i) {
      size_t v1 = face[i];
      size_t v2 = face[(i + 1) % numVerts];
      if (v1 > v2) std::swap(v1, v2);
      edgeSet.insert(Edge(v1, v2));
    }
  }
  edges.assign(edgeSet.begin(), edgeSet.end());
}

void MeshConstructor::generateIcosahedron() {
  const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    
  // Unnormalized vertices of icosahedron in standard orientation
  Vertices& vertices = _icosahedron.vertices;
  // set 3X matrix to size 12 columns
  vertices.resize(3, 12);
  vertices.col(0) = Eigen::Vector3d(-1,  phi,  0);
  vertices.col(1) = Eigen::Vector3d( 1,  phi,  0);
  vertices.col(2) = Eigen::Vector3d(-1, -phi,  0);
  vertices.col(3) = Eigen::Vector3d( 1, -phi,  0);
  vertices.col(4) = Eigen::Vector3d( 0, -1,  phi);
  vertices.col(5) = Eigen::Vector3d( 0,  1,  phi);
  vertices.col(6) = Eigen::Vector3d( 0, -1, -phi);
  vertices.col(7) = Eigen::Vector3d( 0,  1, -phi);
  vertices.col(8) = Eigen::Vector3d( phi,  0, -1);
  vertices.col(9) = Eigen::Vector3d( phi,  0,  1);
  vertices.col(10)= Eigen::Vector3d(-phi,  0, -1);
  vertices.col(11)= Eigen::Vector3d(-phi,  0,  1);

  // vectorized normalization of all columns
  vertices.colwise().normalize();

  // Rotate the icosahedron so that vertex 0 is at the north pole (+Z axis)
  Eigen::Vector3d originalVertex = vertices.col(0);
  Eigen::Vector3d targetVertex(0, 0, 1);
  Eigen::Vector3d rotationAxis = originalVertex.cross(targetVertex);
  rotationAxis.normalize();
  const double cosAngle = originalVertex.dot(targetVertex);
  const double angle = std::acos(cosAngle);

  Eigen::AngleAxisd rotation(angle, rotationAxis);
  // matrix multiplication to rotate all vertices and scale to desired radius
  vertices = _radius * rotation.toRotationMatrix() * vertices;

  Faces& faces = _icosahedron.faces;
  faces.reserve(20);
  faces.push_back(Eigen::Vector3i(0, 11, 5));
  faces.push_back(Eigen::Vector3i(0, 5, 1));
  faces.push_back(Eigen::Vector3i(0, 1, 7));
  faces.push_back(Eigen::Vector3i(0, 7, 10));
  faces.push_back(Eigen::Vector3i(0, 10, 11));
  faces.push_back(Eigen::Vector3i(1, 5, 9));
  faces.push_back(Eigen::Vector3i(5, 11, 4));
  faces.push_back(Eigen::Vector3i(11, 10, 2));
  faces.push_back(Eigen::Vector3i(10, 7, 6));
  faces.push_back(Eigen::Vector3i(7, 1, 8));
  faces.push_back(Eigen::Vector3i(3, 9, 4));
  faces.push_back(Eigen::Vector3i(3, 4, 2));
  faces.push_back(Eigen::Vector3i(3, 2, 6));
  faces.push_back(Eigen::Vector3i(3, 6, 8));
  faces.push_back(Eigen::Vector3i(3, 8, 9));
  faces.push_back(Eigen::Vector3i(4, 9, 5));
  faces.push_back(Eigen::Vector3i(2, 4, 11));
  faces.push_back(Eigen::Vector3i(6, 2, 10));
  faces.push_back(Eigen::Vector3i(8, 6, 7));
  faces.push_back(Eigen::Vector3i(9, 8, 1));

  Edges& edges = _icosahedron.edges;
  getEdgesFromFaces(faces, edges);
}

void MeshConstructor::subdivideIcosahedron() {
  // expected number of vertices after subdivision
  const int nVertices = 10 * _frequency * _frequency + 2;
  // expected number of faces after subdivision
  const int nFaces = 20 * _frequency * _frequency;

  _subdivided.vertices.resize(3, nVertices);
  _subdivided.faces.reserve(nFaces);

  std::unordered_map<Key, int, KeyHash, KeyEq> indexOf;
  indexOf.reserve(size_t(nVertices * 1.1));

  Vertices& V = _subdivided.vertices;
  Faces& F = _subdivided.faces;
  V.resize(3, nVertices);
  F.reserve(nFaces);
  size_t col = 0;

  for (size_t f = 0; f < _icosahedron.faces.size(); ++f) {
    const Face& face = _icosahedron.faces[f];
    assert(face.size() == 3);
    const Eigen::Vector3d& v0 = _icosahedron.vertices.col(face[0]);
    const Eigen::Vector3d& v1 = _icosahedron.vertices.col(face[1]);
    const Eigen::Vector3d& v2 = _icosahedron.vertices.col(face[2]);

    // Create a grid of vertices on the triangle using barycentric coordinates
    std::vector<std::vector<int>> grid(_frequency + 1);

    for (int i = 0; i <= _frequency; ++i) {
      for (int j = 0; j <= _frequency - i; ++j) {
        int k = _frequency - i - j;

        double u = static_cast<double>(i) / _frequency;
        double v = static_cast<double>(j) / _frequency;
        double w = static_cast<double>(k) / _frequency;

        Eigen::Vector3d pos = u * v0 + v * v1 + w * v2;

        Key key = quantize_unit(pos.normalized());
        auto it = indexOf.find(key);
        int idx;
        if (it != indexOf.end()) {
          idx = it->second;
        } else {
          idx = col;
          V.col(idx) = pos;
          ++col;
          indexOf[key] = idx;
        }
        grid[i].push_back(idx);
      }
    }

    // Create triangular faces from the grid
    for (int i = 0; i < _frequency; ++i) {
      for (int j = 0; j < _frequency - i; ++j) {
        F.push_back(Eigen::Vector3i(
          grid[i][j],
          grid[i + 1][j],
          grid[i][j + 1]
        ));
        if (j < _frequency - i - 1) {
          F.push_back(Eigen::Vector3i(
            grid[i + 1][j],
            grid[i + 1][j + 1],
            grid[i][j + 1]
          ));
        }
      }
    }
  }
  _subdivided.vertices = V.leftCols(col);
  F.shrink_to_fit();
  getEdgesFromFaces(F, _subdivided.edges);
}

void MeshConstructor::sortFaceVertices(Faces& faces, const Vertices& V, const Vertices& Origin) {
  for (size_t i = 0; i < faces.size(); ++i) {
    // angular direction basis
    Eigen::Vector3d up = Origin.col(i).normalized();
    Eigen::Vector3d right;
    if (std::abs(up.x()) < 0.9) {
      right = up.cross(Eigen::Vector3d(1, 0, 0)).normalized();
    } else {
      right = up.cross(Eigen::Vector3d(0, 1, 0)).normalized();
    }
    // right x forward = up -> right x up = -forward
    Eigen::Vector3d forward = -right.cross(up);

    std::sort(faces[i].data(), faces[i].data() + faces[i].size(),
              [&](int a, int b) {
                Eigen::Vector3d Va = V.col(a).normalized();
                Eigen::Vector3d Vb = V.col(b).normalized();
                Eigen::Vector3d va = Va - up * (Va.dot(up));
                Eigen::Vector3d vb = Vb - up * (Vb.dot(up));
                double angleA = std::atan2(va.dot(forward), va.dot(right));
                double angleB = std::atan2(vb.dot(forward), vb.dot(right));
                return angleA < angleB;
              });
  }
}

void MeshConstructor::projectSubdivisions() {
  Vertices& V = _primal.vertices;
  Edges& E = _primal.edges;
  Faces& F = _primal.faces;

  V = _subdivided.vertices.colwise().normalized() * _radius;

  // Treat Faces as an inverted list vertexID->neighbors
  F.resize(V.cols());

  Edges& subdivEdges = _subdivided.edges;
  for (Edge edge : subdivEdges) {
    int v1 = edge[0];
    int v2 = edge[1];
    F[v1].conservativeResize(F[v1].size() + 1);
    F[v1][F[v1].size() - 1] = v2;
    F[v2].conservativeResize(F[v2].size() + 1);
    F[v2][F[v2].size() - 1] = v1;
  }

  sortFaceVertices(F, V, V);
  getEdgesFromFaces(F, E);
}

void MeshConstructor::constructDualCells() {
  Vertices& V = _dual.vertices;
  Faces& F = _dual.faces;
  Edges& E = _dual.edges;
  const Vertices& primalV = _primal.vertices;
  const Faces& triangleF = _subdivided.faces;

  // each triangleF gives a vertex in dualV
  // the indices in triangleF map to primalV
  // corresponding primalF indices gives the adjacency of primalV
  // each primalV maps to a face in dualF
  // the resulting dual vertex is an element of each face in dualF corresponding to a primalV in triangleF

  V.resize(3, triangleF.size());
  F.resize(primalV.cols());

  for (size_t i = 0; i < triangleF.size(); ++i) {
    const Eigen::Vector3i& tri = triangleF[i];
    Eigen::Vector3d A = primalV.col(tri[0]).normalized();
    Eigen::Vector3d B = primalV.col(tri[1]).normalized();
    Eigen::Vector3d C = primalV.col(tri[2]).normalized();
    Eigen::Vector3d D = A.cross(B) + B.cross(C) + C.cross(A);

    Eigen::Vector3d triN = (B - A).cross(C - A);
    if (D.dot(triN) < 0) D = -D;
    V.col(i) = _radius * D.normalized();
    // each vertex in tri contributes to a face in dualF
    for (int j = 0; j < 3; ++j) {
      int vIdx = tri[j];
      F[vIdx].conservativeResize(F[vIdx].size() + 1);
      F[vIdx][F[vIdx].size() - 1] = static_cast<int>(i);
    }
  }

  sortFaceVertices(F, V, primalV);
  getEdgesFromFaces(F, E);
}