#pragma once

#include <Eigen/Dense>
#include <ceres/jet.h>
#include <cmath>
#include <vector>

namespace spherical_tiling {

// Convert latitude-longitude to 3D position on sphere (templated for Ceres Jet types)
template <typename T>
Eigen::Matrix<T, 3, 1> latLonToCartesian(const T& latitude, const T& longitude, const T& radius) {
    using std::cos;
    using std::sin;
    
    T cos_lat = cos(latitude);
    T sin_lat = sin(latitude);
    T cos_lon = cos(longitude);
    T sin_lon = sin(longitude);
    
    return Eigen::Matrix<T, 3, 1>(
        radius * cos_lat * cos_lon,
        radius * cos_lat * sin_lon,
        radius * sin_lat
    );
}

// Compute spherical circumcenter (templated)
template <typename T>
Eigen::Matrix<T, 3, 1> sphericalCircumcenterT(
    const Eigen::Matrix<T, 3, 1>& p1,
    const Eigen::Matrix<T, 3, 1>& p2,
    const Eigen::Matrix<T, 3, 1>& p3,
    const T& radius
) {
    // Get normals to the great circle arcs
    Eigen::Matrix<T, 3, 1> n1 = p1.cross(p2).normalized();
    Eigen::Matrix<T, 3, 1> n2 = p2.cross(p3).normalized();
    
    // The circumcenter is perpendicular to both normals
    Eigen::Matrix<T, 3, 1> center = n1.cross(n2).normalized();
    
    // Check which direction to use
    if (center.dot(p1) < T(0.0)) {
        center = -center;
    }
    
    return center * radius;
}

// Compute spherical polygon area using L'Huilier's formula (templated)
template <typename T>
T sphericalPolygonAreaT(const std::vector<Eigen::Matrix<T, 3, 1>>& vertices, const T& radius) {
    if (vertices.size() < 3) return T(0.0);
    
    T area = T(0.0);
    int n = vertices.size();
    
    // Normalize all vertices to unit sphere
    std::vector<Eigen::Matrix<T, 3, 1>> v;
    for (const auto& vert : vertices) {
        v.push_back(vert.normalized());
    }
    
    // Triangulate from first vertex and sum solid angles
    for (int i = 1; i < n - 1; ++i) {
        const Eigen::Matrix<T, 3, 1>& a = v[0];
        const Eigen::Matrix<T, 3, 1>& b = v[i];
        const Eigen::Matrix<T, 3, 1>& c = v[i + 1];
        
        // Use L'Huilier's formula for spherical triangle
        T num = (b.cross(c)).dot(a);
        T denom = T(1.0) + a.dot(b) + b.dot(c) + c.dot(a);
        if (abs(denom) < T(1e-10)) {
            continue; // Degenerate triangle
        }
        T excess = T(2) * atan2(num, denom);
        area += excess;
    }
    
    return area * radius * radius;
}

// Compute angles in tangent space at a primal vertex
template <typename T>
std::vector<T> computeTangentSpaceAngles(
    const Eigen::Matrix<T, 3, 1>& center,
    const std::vector<Eigen::Matrix<T, 3, 1>>& dual_vertices,
    const T& radius
) {
    using std::acos;
    
    std::vector<T> angles;
    if (dual_vertices.size() < 2) return angles;
    
    // Normal at the center (for tangent plane)
    Eigen::Matrix<T, 3, 1> normal = center.normalized();
    
    int n = dual_vertices.size();
    for (int i = 0; i < n; ++i) {
        const Eigen::Matrix<T, 3, 1>& prev = dual_vertices[(i - 1 + n) % n];
        const Eigen::Matrix<T, 3, 1>& curr = dual_vertices[i];
        const Eigen::Matrix<T, 3, 1>& next = dual_vertices[(i + 1) % n];
        
        // Project to tangent plane
        Eigen::Matrix<T, 3, 1> v1 = prev - curr;
        Eigen::Matrix<T, 3, 1> v2 = next - curr;
        
        // Project onto tangent plane at center
        v1 = v1 - (v1.dot(normal)) * normal;
        v2 = v2 - (v2.dot(normal)) * normal;
        
        v1 = v1.normalized();
        v2 = v2.normalized();
        
        T cos_angle = v1.dot(v2);
        cos_angle = (cos_angle < T(-1.0)) ? T(-1.0) : ((cos_angle > T(1.0)) ? T(1.0) : cos_angle);
        T angle = acos(cos_angle);
        
        angles.push_back(angle);
    }
    
    return angles;
}

// Compute angle variance (templated)
template <typename T>
T computeAngleVariance(const std::vector<T>& angles) {
    if (angles.empty()) return T(0.0);
    
    // Compute mean
    T mean = T(0.0);
    for (const T& angle : angles) {
        mean += angle;
    }
    mean /= T(angles.size());
    
    // Compute variance
    T variance = T(0.0);
    for (const T& angle : angles) {
        T diff = angle - mean;
        variance += diff * diff;
    }
    variance /= T(angles.size());
    
    return variance;
}

} // namespace spherical_tiling
