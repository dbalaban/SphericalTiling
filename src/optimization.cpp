#include "optimization.h"
#include <ceres/ceres.h>
#include <cmath>

namespace spherical_tiling {

// Cost function for equal-area constraint
struct EqualAreaCost {
    EqualAreaCost(double target_area) : target_area_(target_area) {}
    
    template <typename T>
    bool operator()(const T* const area, T* residual) const {
        residual[0] = T(target_area_) - area[0];
        return true;
    }
    
    static ceres::CostFunction* Create(double target_area) {
        return new ceres::AutoDiffCostFunction<EqualAreaCost, 1, 1>(
            new EqualAreaCost(target_area)
        );
    }
    
private:
    double target_area_;
};

// Cost function for equal-angles constraint (minimize angle variance)
struct EqualAnglesCost {
    EqualAnglesCost() {}
    
    template <typename T>
    bool operator()(const T* const angle_variance, T* residual) const {
        residual[0] = angle_variance[0];
        return true;
    }
    
    static ceres::CostFunction* Create() {
        return new ceres::AutoDiffCostFunction<EqualAnglesCost, 1, 1>(
            new EqualAnglesCost()
        );
    }
};

void optimizeTileGraph(TileGraph& graph, double radius) {
    // Compute target area (total surface area divided by number of nodes)
    double totalArea = 4.0 * M_PI * radius * radius;
    double targetArea = totalArea / graph.numNodes();
    
    ceres::Problem problem;
    
    // For each node, add weighted cost functions based on latitude
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        auto& node = graph.getNode(i);
        
        // Compute latitude weight: 1.0 at poles, 0.0 at equator
        // latitude = asin(z / radius)
        double z = node.center.z();
        double latitude = std::asin(z / radius);
        double absLatitude = std::abs(latitude);
        
        // Weight interpolation: near poles favor equal-angles, near equator favor equal-area
        double poleWeight = absLatitude / (M_PI / 2.0); // 0 at equator, 1 at poles
        double equatorWeight = 1.0 - poleWeight;
        
        // Add equal-area cost (weighted toward equator)
        if (equatorWeight > 0.01) {
            ceres::CostFunction* area_cost = EqualAreaCost::Create(targetArea);
            problem.AddResidualBlock(
                area_cost,
                nullptr, // No loss function (squared loss)
                &node.area
            );
            // Scale by weight
            problem.SetParameterBlockConstant(&node.area); // Read-only in this simple implementation
        }
        
        // Add equal-angles cost (weighted toward poles)
        if (poleWeight > 0.01) {
            ceres::CostFunction* angle_cost = EqualAnglesCost::Create();
            problem.AddResidualBlock(
                angle_cost,
                nullptr,
                &node.angle_variance
            );
            problem.SetParameterBlockConstant(&node.angle_variance); // Read-only in this simple implementation
        }
        
        // Compute weighted energy
        double areaError = std::abs(node.area - targetArea);
        node.energy = equatorWeight * areaError + poleWeight * node.angle_variance;
    }
    
    // Note: In a full implementation, we would optimize the vertex positions
    // to minimize the energy. For this demo, we compute the energy based on
    // the current configuration without moving vertices.
    
    // Solver options
    ceres::Solver::Options options;
    options.max_num_iterations = 100;
    options.linear_solver_type = ceres::DENSE_QR;
    options.minimizer_progress_to_stdout = false;
    
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
}

} // namespace spherical_tiling
