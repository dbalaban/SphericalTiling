#pragma once

#include <Eigen/Dense>

namespace spherical_tiling {

class Camera {
public:
    Camera();
    
    // Set up perspective projection
    void setPerspective(float fov, float aspect, float near, float far);
    
    // Get view matrix (looking at origin)
    Eigen::Matrix4f getViewMatrix() const;
    
    // Get projection matrix
    Eigen::Matrix4f getProjectionMatrix() const;
    
    // Camera controls
    void rotate(float deltaAzimuth, float deltaLatitude);
    void zoom(float deltaDistance);
    void reset();
    
    // Getters
    float getDistance() const { return distance_; }
    float getYaw() const { return azimuth_; }
    float getPitch() const { return latitude_; }
    float getAzimuth() const { return azimuth_; }
    float getLatitude() const { return latitude_; }
    Eigen::Vector3f getEyePosition() const;
    
private:
    void updateViewMatrix();
    
    // Camera position in spherical coordinates around the Z-up globe.
    float distance_;
    float azimuth_;
    float latitude_;

    // Projection parameters
    float fov_;
    float aspect_;
    float nearPlane_;
    float farPlane_;
    
    // Cached matrices
    Eigen::Matrix4f viewMatrix_;
    Eigen::Matrix4f projMatrix_;
};

} // namespace spherical_tiling
