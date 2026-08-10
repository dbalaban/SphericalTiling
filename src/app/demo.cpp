#include "mesh_construction.h"
#include <cstdlib>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace spherical_tiling;

namespace {

void printUsage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [--q <frequency 1..24>] [--radius <value>] [--help]\n";
}

bool parseIntArg(const std::string& text, int& value) {
    char* end = nullptr;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool parseDoubleArg(const std::string& text, double& value) {
    char* end = nullptr;
    double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    value = parsed;
    return true;
}

}

int main(int argc, char** argv) {
    std::cout << "Spherical Tiling Demo - Goldberg Subdivision\n";
    std::cout << "==============================================================\n";

    // Default parameters
    double radius = 1.0;
    int frequency = 3; // Subdivision frequency (q in Goldberg Class-I)

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        if ((arg == "--q" || arg == "-q") && i + 1 < argc) {
            if (!parseIntArg(argv[++i], frequency)) {
                std::cerr << "Invalid frequency value\n";
                printUsage(argv[0]);
                return 1;
            }
            continue;
        }
        if (arg == "--radius" && i + 1 < argc) {
            if (!parseDoubleArg(argv[++i], radius)) {
                std::cerr << "Invalid radius value\n";
                printUsage(argv[0]);
                return 1;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        printUsage(argv[0]);
        return 1;
    }

    if (frequency <= 0) {
        std::cerr << "Frequency must be positive\n";
        return 1;
    }
    if (!std::isfinite(radius) || radius <= 0.0) {
        std::cerr << "Radius must be a positive finite number\n";
        return 1;
    }

    // Calculate expected number of dual cells: N = 10*q^2 + 2
    int expectedDualCells = 10 * frequency * frequency + 2;

    std::cout << "\nParameters:\n";
    std::cout << "  Sphere radius: " << radius << "\n";
    std::cout << "  Goldberg frequency (q): " << frequency << "\n";
    std::cout << "  Expected dual cells: " << expectedDualCells << "\n";
    MeshConstructor constructor(radius, static_cast<uint16_t>(frequency));
    const Mesh& icosahedron = constructor.getIcosahedron();
    std::cout << "Icosahedron has " << icosahedron.vertices.cols() << " vertices, "
              << icosahedron.faces.size() << " faces, "
              << icosahedron.edges.size() << " edges\n";
    const Mesh& subdivided = constructor.getSubdividedMesh();
    std::cout << "Subdivided mesh has " << subdivided.vertices.cols() << " vertices, "
              << subdivided.faces.size() << " faces, "
              << subdivided.edges.size() << " edges\n";
    const Mesh& dualMesh = constructor.getDualMesh();
    const Faces& dualFaces = dualMesh.faces;
    const DualTopologyReport topology = validateDualTopology(constructor);
    std::size_t min_vertices = std::numeric_limits<std::size_t>::max();
    size_t max_vertices = 0;
    for (const auto& face : dualFaces) {
        const std::size_t face_size = static_cast<std::size_t>(face.size());
        if (face_size < min_vertices) min_vertices = face_size;
        if (face_size > max_vertices) max_vertices = face_size;
    }
    std::cout << "Dual mesh has " << dualFaces.size() << " faces\n";
    std::cout << "  Min vertices per face: " << min_vertices << "\n";
    std::cout << "  Max vertices per face: " << max_vertices << "\n";
    std::cout << "  Pentagons: " << topology.pentagons << "\n";
    std::cout << "  Hexagons: " << topology.hexagons << "\n";
    std::cout << "  Topology valid: " << (topology.valid ? "yes" : "no") << "\n";
    std::cout << "Done.\n";

    return 0;
}
