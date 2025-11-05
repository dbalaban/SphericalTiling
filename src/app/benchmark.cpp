#include "mesh_construction.h"
#include "optimization.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

using namespace spherical_tiling;

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
    // TODO: run benchmarks after optimization has been implemented
    
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
