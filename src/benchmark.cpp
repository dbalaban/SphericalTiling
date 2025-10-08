#include "goldberg_subdivision.h"
#include "tile_graph.h"
#include "dual_construction.h"
#include "optimization.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

using namespace spherical_tiling;

struct Metrics {
    double areaDeviation;    // Standard deviation of areas
    double angleDeviation;   // Average angle variance
};

Metrics computeMetrics(const TileGraph& graph) {
    double totalArea = 0.0;
    double totalAngleVar = 0.0;
    int count = 0;
    
    std::vector<double> areas;
    
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        const auto& node = graph.getNode(i);
        areas.push_back(node.area);
        totalArea += node.area;
        totalAngleVar += node.angle_variance;
        count++;
    }
    
    // Compute mean area
    double meanArea = totalArea / count;
    
    // Compute area standard deviation
    double areaVariance = 0.0;
    for (double area : areas) {
        double diff = area - meanArea;
        areaVariance += diff * diff;
    }
    areaVariance /= count;
    double areaStdDev = std::sqrt(areaVariance);
    
    // Average angle variance
    double avgAngleVar = totalAngleVar / count;
    
    return {areaStdDev, avgAngleVar};
}

Metrics runConfiguration(int frequency, double radius, bool optimize, WeightFunction weightFunc) {
    // Generate icosahedron
    auto icoVertices = generateIcosahedron(radius);
    auto icoFaces = generateIcosahedronFaces();
    
    // Perform Goldberg subdivision
    auto subdivision = goldbergSubdivision(icoVertices, icoFaces, frequency, radius);
    
    // Build tile graph
    TileGraph graph;
    graph.buildFromTriangulation(subdivision.vertices, subdivision.faces, radius);
    
    // Construct dual cells
    constructDualCells(graph, radius);
    
    // Optimize if requested
    if (optimize) {
        // Use fewer iterations for benchmark
        int maxIter = (frequency <= 2) ? 20 : 30;
        optimizeTileGraph(graph, radius, weightFunc, maxIter, true);  // silent=true
        // Recompute dual cells after optimization
        constructDualCells(graph, radius);
    }
    
    // Compute and return metrics
    return computeMetrics(graph);
}

int main() {
    std::cout << "Spherical Tiling Benchmark - Optimization Comparison\n";
    std::cout << "====================================================\n\n";
    
    double radius = 1.0;
    std::vector<int> frequencies = {1, 2, 3, 4, 5};
    std::vector<std::pair<std::string, bool>> noOptConfigs = {{"no-opt", false}};
    std::vector<std::pair<std::string, WeightFunction>> optConfigs = {
        {"f1", WeightFunction::F1},
        {"f2", WeightFunction::F2},
        {"f3", WeightFunction::F3},
        {"f4", WeightFunction::F4},
        {"f5", WeightFunction::F5},
        {"f6", WeightFunction::F6}
    };
    
    // Redirect Ceres output to suppress warnings
    std::cout << "Running benchmarks (this may take a few minutes)...\n\n";
    
    // Print header
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Metrics: (Area StdDev, Angle Variance)\n\n";
    
    std::cout << std::setw(5) << "q";
    for (const auto& [name, _] : noOptConfigs) {
        std::cout << std::setw(30) << name;
    }
    for (const auto& [name, _] : optConfigs) {
        std::cout << std::setw(30) << name;
    }
    std::cout << "\n";
    
    std::cout << std::string(5 + 30 * (noOptConfigs.size() + optConfigs.size()), '=') << "\n";
    
    // Run benchmarks
    for (int q : frequencies) {
        std::cerr << "Processing q=" << q << "..." << std::endl;
        std::cout << std::setw(5) << q;
        
        // No optimization
        for (const auto& [name, doOpt] : noOptConfigs) {
            Metrics m = runConfiguration(q, radius, doOpt, WeightFunction::F1);
            std::cout << std::setw(30) << ("(" + std::to_string(m.areaDeviation).substr(0, 8) + ", " + 
                                           std::to_string(m.angleDeviation).substr(0, 8) + ")");
        }
        
        // With optimization for each weight function
        for (const auto& [name, wf] : optConfigs) {
            Metrics m = runConfiguration(q, radius, true, wf);
            std::cout << std::setw(30) << ("(" + std::to_string(m.areaDeviation).substr(0, 8) + ", " + 
                                           std::to_string(m.angleDeviation).substr(0, 8) + ")");
        }
        
        std::cout << "\n";
    }
    
    std::cout << "\n";
    std::cout << "Legend:\n";
    std::cout << "  Area StdDev: Standard deviation of dual cell areas (lower is better)\n";
    std::cout << "  Angle Variance: Average angle variance in tangent space (lower is better)\n";
    std::cout << "\nWeight Functions:\n";
    std::cout << "  f1: 2|lat|/π (favors angles at poles)\n";
    std::cout << "  f2: 1-f1 (favors area at poles)\n";
    std::cout << "  f3: cos²(lat) (favors area at poles)\n";
    std::cout << "  f4: sin²(lat) (favors angles at poles)\n";
    std::cout << "  f5: 1 (pure area optimization)\n";
    std::cout << "  f6: 0 (pure angle optimization)\n";
    
    return 0;
}
