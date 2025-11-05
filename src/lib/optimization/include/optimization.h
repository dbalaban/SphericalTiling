#pragma once

namespace spherical_tiling {

// Weight function types for balancing area vs angle optimization
enum class WeightFunction {
    F1,  // f1(lat) = 2*abs(lat)/π  (linear, favors angles at poles)
    F2,  // f2(lat) = 1 - f1(lat)  (linear, favors area at poles)
    F3,  // f3(lat) = cos²(lat)     (favors area at poles)
    F4,  // f4(lat) = sin²(lat)     (favors angles at poles)
    F5,  // f5(lat) = 1             (pure area optimization)
    F6   // f6(lat) = 0             (pure angle optimization)
};

// Compute weight for area term given latitude and weight function
// Returns w_area, where w_angle = 1 - w_area
double computeAreaWeight(double latitude, WeightFunction func);

} // namespace spherical_tiling
