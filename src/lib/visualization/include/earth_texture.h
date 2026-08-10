#pragma once

#include <Eigen/Dense>

#include <string>
#include <vector>

namespace spherical_tiling {

class EarthTexture {
public:
  bool load(const std::string& path, std::string& error);

  bool isLoaded() const { return loaded_; }
  int width() const { return width_; }
  int height() const { return height_; }
  const std::string& path() const { return path_; }
  const unsigned char* data() const { return pixels_.empty() ? nullptr : pixels_.data(); }

  Eigen::Vector3f sampleBilinear(float u, float v) const;

private:
  int width_ = 0;
  int height_ = 0;
  std::string path_;
  bool loaded_ = false;
  std::vector<unsigned char> pixels_;
};

} // namespace spherical_tiling
