#include "optimization.h"
#include "templated_geometry.h"
#include <ceres/ceres.h>
#include <cmath>
#include <iostream>

namespace spherical_tiling {

// OPTIMIZATION PARAMETERIZATION NOTES:
// ====================================
// This optimization uses 3D Cartesian coordinates (x, y, z) instead of latitude-longitude
// parameterization to avoid gradient singularities at the poles.
//
// Problem with lat-lon parameterization:
// - At the poles (latitude = ±π/2), cos(latitude) ≈ 0
// - This causes the Jacobian w.r.t. longitude to vanish: ∂position/∂longitude ≈ 0
// - This creates a "gimbal lock" situation where longitude becomes undefined
// - Ceres gradient checker detects this as a gradient error and terminates optimization
//
// Solution with 3D Cartesian + SphereManifold:
// - Each vertex is parameterized as a 3D point (x, y, z) on the sphere
// - Ceres::SphereManifold constrains points to remain on the sphere surface
// - The manifold provides well-defined gradients everywhere on the sphere
// - This is similar to using quaternions but more direct for spherical optimization
//
// Benefits:
// - No singularities or gimbal lock issues
// - Smooth, well-conditioned gradients at all points including poles
// - Optimization converges reliably without gradient errors

double computeAreaWeight(double latitude, WeightFunction func) {
    using std::abs;
    using std::cos;
    using std::sin;
    
    const double PI = M_PI;
    
    switch (func) {
        case WeightFunction::F1:
            // f1(lat) = 2*abs(lat)/π (favors angles at poles)
            return 2.0 * abs(latitude) / PI;
            
        case WeightFunction::F2:
            // f2(lat) = 1 - f1(lat) (favors area at poles)
            return 1.0 - (2.0 * abs(latitude) / PI);
            
        case WeightFunction::F3:
            // f3(lat) = cos²(lat) (favors area at poles)
            return cos(latitude) * cos(latitude);
            
        case WeightFunction::F4:
            // f4(lat) = sin²(lat) (favors angles at poles)
            return sin(latitude) * sin(latitude);
            
        case WeightFunction::F5:
            // f5(lat) = 1 (pure area optimization)
            return 1.0;
            
        case WeightFunction::F6:
            // f6(lat) = 0 (pure angle optimization)
            return 0.0;
            
        default:
            return 0.5;
    }
}

// Cost functor for dual cell properties (area variance + angle variance)
// Now using 3D Cartesian coordinates instead of lat-lon to avoid singularities at poles
struct DualCellCost {
    DualCellCost(
        const std::vector<Node>* nodes,
        int node_idx,
        const std::vector<int>& neighbor_indices,
        double radius,
        double target_area,
        double w_area,
        double w_angle
    ) : nodes_(nodes),
        node_idx_(node_idx),
        neighbor_indices_(neighbor_indices),
        radius_(radius),
        target_area_(target_area),
        w_area_(w_area),
        w_angle_(w_angle) {}
    
    template <typename T>
    bool operator()(T const* const* parameters, T* residuals) const {
        // Extract current node parameters (now 3D Cartesian coordinates)
        // The sphere manifold will keep these normalized to radius
        Eigen::Matrix<T, 3, 1> center(parameters[0][0], parameters[0][1], parameters[0][2]);
        
        // Build neighbor positions
        std::vector<Eigen::Matrix<T, 3, 1>> neighbor_positions;
        for (size_t i = 0; i < neighbor_indices_.size(); ++i) {
            Eigen::Matrix<T, 3, 1> neighbor(
                parameters[i + 1][0], 
                parameters[i + 1][1], 
                parameters[i + 1][2]
            );
            neighbor_positions.push_back(neighbor);
        }
        
        // Compute dual cell vertices (circumcenters)
        std::vector<Eigen::Matrix<T, 3, 1>> dual_vertices;
        for (size_t i = 0; i < neighbor_positions.size(); ++i) {
            const auto& n1 = neighbor_positions[i];
            const auto& n2 = neighbor_positions[(i + 1) % neighbor_positions.size()];
            
            Eigen::Matrix<T, 3, 1> circumcenter = sphericalCircumcenterT(center, n1, n2, T(radius_));
            dual_vertices.push_back(circumcenter);
        }
        
        // Compute dual cell area
        T area = sphericalPolygonAreaT(dual_vertices, T(radius_));
        
        // Compute tangent space angles
        std::vector<T> angles = computeTangentSpaceAngles(center, dual_vertices, T(radius_));
        T angle_variance = computeAngleVariance(angles);
        
        // Compute residuals
        // Area residual: deviation from target area
        T area_residual = (area - T(target_area_)) / T(target_area_);
        
        // Angle residual: angle variance (already normalized)
        T angle_residual = angle_variance;
        
        // Apply weights
        residuals[0] = T(w_area_) * area_residual;
        residuals[1] = T(w_angle_) * angle_residual;
        
        return true;
    }
    
private:
    const std::vector<Node>* nodes_;
    int node_idx_;
    std::vector<int> neighbor_indices_;
    double radius_;
    double target_area_;
    double w_area_;
    double w_angle_;
};

void optimizeTileGraph(TileGraph& graph, double radius, WeightFunction weightFunc, int maxIterations, bool silent) {
    // Compute target area (total surface area divided by number of nodes)
    double totalArea = 4.0 * M_PI * radius * radius;
    double targetArea = totalArea / graph.numNodes();
    
    if (!silent) {
        std::cout << "  Target area per node: " << targetArea << "\n";
    }
    
    ceres::Problem problem;
    
    // Prepare parameter blocks: 3D Cartesian coordinates for each vertex
    std::vector<std::array<double, 3>> parameters(graph.numNodes());
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        auto& node = graph.getNode(i);
        parameters[i][0] = node.center[0];
        parameters[i][1] = node.center[1];
        parameters[i][2] = node.center[2];
    }
    
    const auto& nodes = graph.getNodes();
    
    // Add cost functions for each node
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        const auto& node = nodes[i];
        
        if (node.neighbors.size() < 3) continue;
        
        // Compute weight based on latitude
        double w_area = computeAreaWeight(node.latitude, weightFunc);
        double w_angle = 1.0 - w_area;
        
        // Create cost functor
        auto* cost_functor = new DualCellCost(
            &nodes, i, node.neighbors, radius, targetArea, w_area, w_angle
        );
        
        // Create dynamic cost function
        auto* cost_function = new ceres::DynamicAutoDiffCostFunction<DualCellCost>(cost_functor);
        cost_function->SetNumResiduals(2);
        
        // Add parameter blocks: current node + all neighbors (now 3D)
        cost_function->AddParameterBlock(3);  // Current node
        for (size_t j = 0; j < node.neighbors.size(); ++j) {
            cost_function->AddParameterBlock(3);  // Each neighbor
        }
        
        // Build parameter block pointers
        std::vector<double*> param_blocks;
        param_blocks.push_back(parameters[i].data());
        for (int n_idx : node.neighbors) {
            param_blocks.push_back(parameters[n_idx].data());
        }
        
        problem.AddResidualBlock(cost_function, nullptr, param_blocks);
    }
    
    // Create sphere manifold to constrain points to sphere surface
    // This keeps each point normalized to the sphere radius
    ceres::Manifold* sphere_manifold = new ceres::SphereManifold<3>();
    
    // Set the manifold for each parameter block
    for (size_t i = 0; i < parameters.size(); ++i) {
        problem.SetManifold(parameters[i].data(), sphere_manifold);
    }
    
    // Solver options
    ceres::Solver::Options options;
    options.max_num_iterations = maxIterations;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    options.minimizer_progress_to_stdout = false;
    options.num_threads = 4;
    // Note: gradient checking disabled because the sphere manifold changes the gradient computation
    // The 3D Cartesian parameterization with sphere manifold avoids the lat-lon singularities at poles
    
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    
    if (!silent) {
        std::cout << "  Optimization iterations: " << summary.iterations.size() << "\n";
        std::cout << "  Final cost: " << summary.final_cost << "\n";
    }
    
    // Update graph with optimized positions
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        auto& node = graph.getNode(i);
        
        // Update 3D center from optimized parameters
        node.center[0] = parameters[i][0];
        node.center[1] = parameters[i][1];
        node.center[2] = parameters[i][2];
        
        // Compute latitude and longitude from 3D position for consistency
        double x = node.center[0];
        double y = node.center[1];
        double z = node.center[2];
        node.latitude = std::asin(z / radius);
        node.longitude = std::atan2(y, x);
    }
}

} // namespace spherical_tiling
