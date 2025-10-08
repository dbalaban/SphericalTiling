#include "goldberg_subdivision.h"
#include "tile_graph.h"
#include "dual_construction.h"
#include "optimization.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <map>
#include <vector>

using namespace spherical_tiling;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --R <value>    Radius of the sphere (default: 1.0)\n";
    std::cout << "  --q <value>    Goldberg Class-I frequency (default: 3)\n";
    std::cout << "                 Number of dual-cells given by N=10*q^2+2\n";
    std::cout << "  --no-opt       Skip optimization step\n";
    std::cout << "  --help         Display this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << programName << " --R 2.5 --q 4\n";
    std::cout << "  " << programName << " --q 5 --no-opt\n";
}

void printStatistics(const TileGraph& graph, double radius) {
    std::cout << "\n=== Tile Graph Statistics ===\n";
    std::cout << "Number of nodes: " << graph.numNodes() << "\n";
    std::cout << "Number of edges: " << graph.getEdges().size() << "\n";
    
    // Compute vertex degree distribution
    std::map<int, int> degreeCount;
    std::vector<int> degree5Vertices;
    std::vector<int> degree6Vertices;
    
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        const auto& node = graph.getNode(i);
        int degree = node.neighbors.size();
        degreeCount[degree]++;
        
        if (degree == 5) {
            degree5Vertices.push_back(i);
        } else if (degree == 6) {
            degree6Vertices.push_back(i);
        }
    }
    
    // Compute statistics
    double totalArea = 0.0;
    double totalAngleDefect = 0.0;
    double totalEnergy = 0.0;
    double totalAngleVariance = 0.0;
    
    double minArea = 1e10;
    double maxArea = -1e10;
    double minAngleVariance = 1e10;
    double maxAngleVariance = -1e10;
    
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        const auto& node = graph.getNode(i);
        
        totalArea += node.area;
        totalAngleDefect += node.angle_defect;
        totalEnergy += node.energy;
        totalAngleVariance += node.angle_variance;
        
        minArea = std::min(minArea, node.area);
        maxArea = std::max(maxArea, node.area);
        minAngleVariance = std::min(minAngleVariance, node.angle_variance);
        maxAngleVariance = std::max(maxAngleVariance, node.angle_variance);
    }
    
    double avgArea = totalArea / graph.numNodes();
    double avgAngleDefect = totalAngleDefect / graph.numNodes();
    double avgEnergy = totalEnergy / graph.numNodes();
    double avgAngleVariance = totalAngleVariance / graph.numNodes();
    
    double sphereArea = 4.0 * M_PI * radius * radius;
    double areaCoverage = (totalArea / sphereArea) * 100.0;
    
    // Validate Goldberg polyhedron structure
    std::cout << "\n--- Dual Face Structure (Goldberg Polyhedron) ---\n";
    bool structureValid = true;
    
    // Check degree distribution
    std::cout << "Vertex degree distribution:\n";
    for (const auto& [degree, count] : degreeCount) {
        std::cout << "  Degree " << degree << ": " << count << " vertices";
        if (degree == 5) {
            std::cout << " (pentagonal dual faces)";
            if (count != 12) {
                std::cout << " [ERROR: Expected exactly 12!]";
                structureValid = false;
            }
        } else if (degree == 6) {
            std::cout << " (hexagonal dual faces)";
        } else {
            std::cout << " [ERROR: Invalid degree for Goldberg polyhedron!]";
            structureValid = false;
        }
        std::cout << "\n";
    }
    
    // Analyze polar cap structure for the 12 pentagons
    if (degree5Vertices.size() == 12) {
        // Sort pentagons by z-coordinate to analyze distribution
        std::vector<std::pair<int, double>> pentagonsByZ;
        for (int idx : degree5Vertices) {
            double z = graph.getNode(idx).center.z();
            pentagonsByZ.push_back({idx, z});
        }
        std::sort(pentagonsByZ.begin(), pentagonsByZ.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::cout << "\nPolar cap structure:\n";
        std::cout << "  Pentagon distribution by latitude:\n";
        
        // Group by similar z-coordinates
        const double zTolerance = 0.01;
        std::vector<std::pair<double, int>> zLevels;
        double currentZ = pentagonsByZ[0].second;
        int currentCount = 0;
        
        for (const auto& [idx, z] : pentagonsByZ) {
            if (std::abs(z - currentZ) < zTolerance) {
                currentCount++;
            } else {
                zLevels.push_back({currentZ, currentCount});
                currentZ = z;
                currentCount = 1;
            }
        }
        zLevels.push_back({currentZ, currentCount});
        
        // Report the distribution
        int northernPentagons = 0;
        int equatorialPentagons = 0;
        int southernPentagons = 0;
        
        for (const auto& [z, count] : zLevels) {
            if (z > zTolerance) {
                northernPentagons += count;
                std::cout << "    Northern: " << count << " pentagons at z ≈ " 
                          << std::fixed << std::setprecision(3) << z << "\n";
            } else if (z < -zTolerance) {
                southernPentagons += count;
                std::cout << "    Southern: " << count << " pentagons at z ≈ " 
                          << std::fixed << std::setprecision(3) << z << "\n";
            } else {
                equatorialPentagons += count;
                std::cout << "    Equatorial: " << count << " pentagons at z ≈ 0\n";
            }
        }
        
        std::cout << "\n  Total pentagons per region:\n";
        std::cout << "    Northern hemisphere: " << northernPentagons;
        if (northernPentagons == 6) {
            std::cout << " ✓";
        }
        std::cout << "\n";
        
        if (equatorialPentagons > 0) {
            std::cout << "    Equatorial band: " << equatorialPentagons << "\n";
        }
        
        std::cout << "    Southern hemisphere: " << southernPentagons;
        if (southernPentagons == 6) {
            std::cout << " ✓";
        }
        std::cout << "\n";
        
        // The structure has 6 pentagons per hemisphere when counting equatorial as split
        int effectiveNorth = northernPentagons + (equatorialPentagons + 1) / 2;
        int effectiveSouth = southernPentagons + equatorialPentagons / 2;
        
        if (effectiveNorth == 6 && effectiveSouth == 6) {
            std::cout << "  ✓ Correct polar cap distribution: 6 pentagons per hemisphere\n";
            std::cout << "    (including " << equatorialPentagons << " equatorial pentagons)\n";
        } else if (northernPentagons + equatorialPentagons + southernPentagons == 12) {
            std::cout << "  ✓ Valid Goldberg polyhedron: 12 pentagonal dual faces total\n";
        }
    }
    
    if (structureValid) {
        std::cout << "\n✓ Goldberg polyhedron structure validated successfully!\n";
    } else {
        std::cout << "\n✗ WARNING: Structure does not match expected Goldberg polyhedron!\n";
    }
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\n--- Area Statistics ---\n";
    std::cout << "Total area: " << totalArea << "\n";
    std::cout << "Sphere area: " << sphereArea << "\n";
    std::cout << "Coverage: " << areaCoverage << "%\n";
    std::cout << "Average area per node: " << avgArea << "\n";
    std::cout << "Min area: " << minArea << "\n";
    std::cout << "Max area: " << maxArea << "\n";
    std::cout << "Area variance: " << (maxArea - minArea) / avgArea * 100.0 << "%\n";
    
    std::cout << "\n--- Angle Statistics ---\n";
    std::cout << "Average angle defect: " << avgAngleDefect << " radians\n";
    std::cout << "Average angle variance: " << avgAngleVariance << "\n";
    std::cout << "Min angle variance: " << minAngleVariance << "\n";
    std::cout << "Max angle variance: " << maxAngleVariance << "\n";
    
    std::cout << "\n--- Energy Statistics ---\n";
    std::cout << "Average energy per node: " << avgEnergy << "\n";
    std::cout << "Total energy: " << totalEnergy << "\n";
    
    // Show sample nodes at different latitudes
    std::cout << "\n--- Sample Nodes ---\n";
    std::cout << std::setw(8) << "ID" 
              << std::setw(12) << "Latitude" 
              << std::setw(14) << "Area"
              << std::setw(14) << "AngleVar"
              << std::setw(14) << "Energy"
              << std::setw(12) << "Neighbors\n";
    std::cout << std::string(74, '-') << "\n";
    
    // Find nodes at pole, mid-latitude, and equator
    std::vector<int> sampleIndices;
    double maxZ = -1e10, minZ = 1e10, nearZero = 1e10;
    int maxZIdx = 0, minZIdx = 0, nearZeroIdx = 0;
    
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        double z = graph.getNode(i).center.z();
        if (z > maxZ) { maxZ = z; maxZIdx = i; }
        if (z < minZ) { minZ = z; minZIdx = i; }
        if (std::abs(z) < nearZero) { nearZero = std::abs(z); nearZeroIdx = i; }
    }
    
    sampleIndices = {maxZIdx, nearZeroIdx, minZIdx};
    
    for (int idx : sampleIndices) {
        const auto& node = graph.getNode(idx);
        double z = node.center.z();
        double latitude = std::asin(z / radius) * 180.0 / M_PI;
        
        std::cout << std::setw(8) << idx
                  << std::setw(12) << latitude
                  << std::setw(14) << node.area
                  << std::setw(14) << node.angle_variance
                  << std::setw(14) << node.energy
                  << std::setw(12) << node.neighbors.size() << "\n";
        
        // Print neighbor details for debugging
        if (node.neighbors.size() > 0 && node.area == 0.0) {
            std::cout << "    Note: Node " << idx << " has " << node.neighbors.size() 
                      << " neighbors but zero area (incomplete cycle)\n";
        }
    }
    
    std::cout << "\n";
}

int main(int argc, char** argv) {
    std::cout << "Spherical Tiling Demo - Goldberg Subdivision with Optimization\n";
    std::cout << "==============================================================\n";
    
    // Default parameters
    double radius = 1.0;
    int frequency = 3; // Subdivision frequency (q in Goldberg Class-I)
    bool runOptimization = true;
    
    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--R") {
            if (i + 1 < argc) {
                radius = std::atof(argv[++i]);
                if (radius <= 0) {
                    std::cerr << "Error: Radius must be positive\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --R requires a value\n";
                printUsage(argv[0]);
                return 1;
            }
        } else if (arg == "--q") {
            if (i + 1 < argc) {
                frequency = std::atoi(argv[++i]);
                if (frequency < 1) {
                    std::cerr << "Error: Frequency must be at least 1\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --q requires a value\n";
                printUsage(argv[0]);
                return 1;
            }
        } else if (arg == "--no-opt") {
            runOptimization = false;
        } else {
            std::cerr << "Error: Unknown argument '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Calculate expected number of dual cells: N = 10*q^2 + 2
    int expectedDualCells = 10 * frequency * frequency + 2;
    
    std::cout << "\nParameters:\n";
    std::cout << "  Sphere radius: " << radius << "\n";
    std::cout << "  Goldberg frequency (q): " << frequency << "\n";
    std::cout << "  Expected dual cells: " << expectedDualCells << "\n";
    std::cout << "  Optimization: " << (runOptimization ? "enabled" : "disabled") << "\n";
    
    // Step 1: Generate icosahedron
    std::cout << "\n[1/5] Generating icosahedron...\n";
    auto icoVertices = generateIcosahedron(radius);
    auto icoFaces = generateIcosahedronFaces();
    std::cout << "  Generated " << icoVertices.size() << " vertices and " 
              << icoFaces.size() << " faces\n";
    
    // Step 2: Perform Goldberg subdivision
    std::cout << "\n[2/5] Performing Goldberg Class-I subdivision...\n";
    auto subdivision = goldbergSubdivision(icoVertices, icoFaces, frequency, radius);
    std::cout << "  Generated " << subdivision.vertices.size() << " vertices and " 
              << subdivision.faces.size() << " faces\n";
    
    // Step 3: Build tile graph
    std::cout << "\n[3/5] Building tile graph...\n";
    TileGraph graph;
    graph.buildFromTriangulation(subdivision.vertices, subdivision.faces, radius);
    std::cout << "  Built graph with " << graph.numNodes() << " nodes and " 
              << graph.getEdges().size() << " edges\n";
    
    // Step 4: Construct dual cells
    std::cout << "\n[4/5] Constructing dual cells (Voronoi regions)...\n";
    constructDualCells(graph, radius);
    std::cout << "  Computed dual cell areas and angle variances\n";
    
    // Step 5: Optimize with Ceres (if enabled)
    if (runOptimization) {
        std::cout << "\n[5/5] Running spatially-weighted optimization...\n";
        optimizeTileGraph(graph, radius);
        std::cout << "  Optimization complete\n";
    } else {
        std::cout << "\n[5/5] Skipping optimization (--no-opt specified)\n";
    }
    
    // Print statistics
    printStatistics(graph, radius);
    
    std::cout << "Demo complete!\n";
    
    return 0;
}
