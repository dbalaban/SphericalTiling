#include "optimization.h"
#include "templated_geometry.h"
#include <ceres/ceres.h>
#include <cmath>
#include <iostream>

namespace spherical_tiling {

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
        // Extract current node parameters
        T lat_center = parameters[0][0];
        T lon_center = parameters[0][1];
        
        // Convert to 3D position
        Eigen::Matrix<T, 3, 1> center = latLonToCartesian(lat_center, lon_center, T(radius_));
        
        // Build neighbor positions
        std::vector<Eigen::Matrix<T, 3, 1>> neighbor_positions;
        for (size_t i = 0; i < neighbor_indices_.size(); ++i) {
            T lat_n = parameters[i + 1][0];
            T lon_n = parameters[i + 1][1];
            neighbor_positions.push_back(latLonToCartesian(lat_n, lon_n, T(radius_)));
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
    
    // Prepare parameter blocks: lat-lon for each vertex
    std::vector<std::array<double, 2>> parameters(graph.numNodes());
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        auto& node = graph.getNode(i);
        parameters[i][0] = node.latitude;
        parameters[i][1] = node.longitude;
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
        
        // Add parameter blocks: current node + all neighbors
        cost_function->AddParameterBlock(2);  // Current node
        for (size_t j = 0; j < node.neighbors.size(); ++j) {
            cost_function->AddParameterBlock(2);  // Each neighbor
        }
        
        // Build parameter block pointers
        std::vector<double*> param_blocks;
        param_blocks.push_back(parameters[i].data());
        for (int n_idx : node.neighbors) {
            param_blocks.push_back(parameters[n_idx].data());
        }
        
        problem.AddResidualBlock(cost_function, nullptr, param_blocks);
    }
    
    // Set parameter bounds (latitude: -π/2 to π/2, longitude: -π to π)
    for (size_t i = 0; i < parameters.size(); ++i) {
        problem.SetParameterLowerBound(parameters[i].data(), 0, -M_PI / 2.0);
        problem.SetParameterUpperBound(parameters[i].data(), 0, M_PI / 2.0);
        problem.SetParameterLowerBound(parameters[i].data(), 1, -M_PI);
        problem.SetParameterUpperBound(parameters[i].data(), 1, M_PI);
    }
    
    // Solver options
    ceres::Solver::Options options;
    options.max_num_iterations = maxIterations;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    options.minimizer_progress_to_stdout = true;
    options.num_threads = 4;
    options.check_gradients = true;
    
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    
    if (!silent) {
        std::cout << "  Optimization iterations: " << summary.iterations.size() << "\n";
        std::cout << "  Final cost: " << summary.final_cost << "\n";
    }
    
    // Update graph with optimized positions
    for (size_t i = 0; i < graph.numNodes(); ++i) {
        auto& node = graph.getNode(i);
        node.latitude = parameters[i][0];
        node.longitude = parameters[i][1];
        
        // Update 3D center from lat-lon
        node.center = latLonToCartesian(node.latitude, node.longitude, radius);
    }
}

} // namespace spherical_tiling
