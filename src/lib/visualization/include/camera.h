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
    void rotate(float deltaYaw, float deltaPitch);
    void zoom(float deltaDistance);
    void reset();
    
    // Getters
    float getDistance() const { return distance_; }
    float getYaw() const { return yaw_; }
    float getPitch() const { return pitch_; }
    
private:
    void updateViewMatrix();
    
    // Camera position in spherical coordinates (looking at origin)
    float distance_;
    float yaw_;    // horizontal rotation
    float pitch_;  // vertical rotation
    
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
