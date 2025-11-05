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

} // namespace spherical_tiling