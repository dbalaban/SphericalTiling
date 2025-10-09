#include "camera.h"
#include <cmath>

namespace spherical_tiling {

Camera::Camera() 
    : distance_(3.0f), yaw_(0.0f), pitch_(0.0f),
      fov_(45.0f), aspect_(1.0f), nearPlane_(0.1f), farPlane_(100.0f) {
    updateViewMatrix();
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
    fov_ = fov;
    aspect_ = aspect;
    nearPlane_ = near;
    farPlane_ = far;
    
    // Build perspective projection matrix
    float tanHalfFov = std::tan(fov * 0.5f * M_PI / 180.0f);
    projMatrix_ = Eigen::Matrix4f::Zero();
    projMatrix_(0, 0) = 1.0f / (aspect * tanHalfFov);
    projMatrix_(1, 1) = 1.0f / tanHalfFov;
    projMatrix_(2, 2) = -(far + near) / (far - near);
    projMatrix_(2, 3) = -(2.0f * far * near) / (far - near);
    projMatrix_(3, 2) = -1.0f;
}

Eigen::Matrix4f Camera::getViewMatrix() const {
    return viewMatrix_;
}

Eigen::Matrix4f Camera::getProjectionMatrix() const {
    return projMatrix_;
}

void Camera::rotate(float deltaYaw, float deltaPitch) {
    yaw_ += deltaYaw;
    pitch_ += deltaPitch;
    
    // Clamp pitch to avoid gimbal lock
    const float maxPitch = 89.0f * M_PI / 180.0f;
    if (pitch_ > maxPitch) pitch_ = maxPitch;
    if (pitch_ < -maxPitch) pitch_ = -maxPitch;
    
    updateViewMatrix();
}

void Camera::zoom(float deltaDistance) {
    distance_ += deltaDistance;
    
    // Clamp distance to reasonable values
    if (distance_ < 0.5f) distance_ = 0.5f;
    if (distance_ > 20.0f) distance_ = 20.0f;
    
    updateViewMatrix();
}

void Camera::reset() {
    distance_ = 3.0f;
    yaw_ = 0.0f;
    pitch_ = 0.0f;
    updateViewMatrix();
}

void Camera::updateViewMatrix() {
    // Calculate camera position in Cartesian coordinates
    float x = distance_ * std::cos(pitch_) * std::sin(yaw_);
    float y = distance_ * std::sin(pitch_);
    float z = distance_ * std::cos(pitch_) * std::cos(yaw_);
    
    Eigen::Vector3f eye(x, y, z);
    Eigen::Vector3f center(0.0f, 0.0f, 0.0f);  // Looking at origin
    Eigen::Vector3f up(0.0f, 1.0f, 0.0f);
    
    // Build view matrix (look-at)
    Eigen::Vector3f f = (center - eye).normalized();
    Eigen::Vector3f s = f.cross(up).normalized();
    Eigen::Vector3f u = s.cross(f);
    
    viewMatrix_ = Eigen::Matrix4f::Identity();
    viewMatrix_(0, 0) = s.x();
    viewMatrix_(0, 1) = s.y();
    viewMatrix_(0, 2) = s.z();
    viewMatrix_(1, 0) = u.x();
    viewMatrix_(1, 1) = u.y();
    viewMatrix_(1, 2) = u.z();
    viewMatrix_(2, 0) = -f.x();
    viewMatrix_(2, 1) = -f.y();
    viewMatrix_(2, 2) = -f.z();
    viewMatrix_(0, 3) = -s.dot(eye);
    viewMatrix_(1, 3) = -u.dot(eye);
    viewMatrix_(2, 3) = f.dot(eye);
}

} // namespace spherical_tiling
