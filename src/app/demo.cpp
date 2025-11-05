#include "mesh_construction.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <map>
#include <vector>

using namespace spherical_tiling;

int main(int argc, char** argv) {
    std::cout << "Spherical Tiling Demo - Goldberg Subdivision\n";
    std::cout << "==============================================================\n";

    // Default parameters
    double radius = 1.0;
    int frequency = 3; // Subdivision frequency (q in Goldberg Class-I)

    // Calculate expected number of dual cells: N = 10*q^2 + 2
    int expectedDualCells = 10 * frequency * frequency + 2;

    std::cout << "\nParameters:\n";
    std::cout << "  Sphere radius: " << radius << "\n";
    std::cout << "  Goldberg frequency (q): " << frequency << "\n";
    std::cout << "  Expected dual cells: " << expectedDualCells << "\n";
    MeshConstructor constructor(radius, frequency);
    const Mesh& icosahedron = constructor.getIcosahedron();
    std::cout << "Icosahedron has " << icosahedron.vertices.cols() << " vertices, "
              << icosahedron.faces.size() << " faces, "
              << icosahedron.edges.size() << " edges\n";
    const Mesh& subdivided = constructor.getSubdividedMesh();
    std::cout << "Subdivided mesh has " << subdivided.vertices.cols() << " vertices, "
              << subdivided.faces.size() << " faces, "
              << subdivided.edges.size() << " edges\n";
    const Mesh& primalMesh = constructor.getPrimalMesh();
    const Mesh& dualMesh = constructor.getDualMesh();
    const Faces& dualFaces = dualMesh.faces;
    size_t min_vertices = SIZE_MAX;
    size_t max_vertices = 0;
    for (const auto& face : dualFaces) {
        if (face.size() < min_vertices) min_vertices = face.size();
        if (face.size() > max_vertices) max_vertices = face.size();
    }
    std::cout << "Dual mesh has " << dualFaces.size() << " faces\n";
    std::cout << "  Min vertices per face: " << min_vertices << "\n";
    std::cout << "  Max vertices per face: " << max_vertices << "\n";
    std::cout << "Done.\n";

    return 0;
}
