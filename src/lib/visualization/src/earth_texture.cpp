#include "earth_texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>

namespace spherical_tiling {

namespace {

float wrapUnit(float value) {
  float wrapped = std::fmod(value, 1.0f);
  if (wrapped < 0.0f) {
    wrapped += 1.0f;
  }
  return wrapped;
}

float clampUnit(float value) {
  return std::max(0.0f, std::min(1.0f, value));
}

}

bool EarthTexture::load(const std::string& path, std::string& error) {
  int channels = 0;
  unsigned char* data = stbi_load(path.c_str(), &width_, &height_, &channels, 3);
  if (!data) {
    error = stbi_failure_reason() ? stbi_failure_reason() : "unknown image loading error";
    loaded_ = false;
    width_ = 0;
    height_ = 0;
    pixels_.clear();
    return false;
  }

  pixels_.assign(data, data + width_ * height_ * 3);
  stbi_image_free(data);
  path_ = path;
  loaded_ = true;
  error.clear();
  return true;
}

Eigen::Vector3f EarthTexture::sampleBilinear(float u, float v) const {
  if (!loaded_ || width_ <= 0 || height_ <= 0) {
    return Eigen::Vector3f(0.2f, 0.25f, 0.3f);
  }

  const float wrappedU = wrapUnit(u);
  const float clampedV = clampUnit(v);

  const float x = wrappedU * static_cast<float>(width_ - 1);
  const float y = clampedV * static_cast<float>(height_ - 1);
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = (x0 + 1) % width_;
  const int y1 = std::min(y0 + 1, height_ - 1);
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);

  auto sample = [&](int px, int py) {
    const std::size_t offset = static_cast<std::size_t>((py * width_ + px) * 3);
    return Eigen::Vector3f(
      pixels_[offset] / 255.0f,
      pixels_[offset + 1] / 255.0f,
      pixels_[offset + 2] / 255.0f
    );
  };

  const Eigen::Vector3f c00 = sample(x0, y0);
  const Eigen::Vector3f c10 = sample(x1, y0);
  const Eigen::Vector3f c01 = sample(x0, y1);
  const Eigen::Vector3f c11 = sample(x1, y1);

  const Eigen::Vector3f c0 = c00 * (1.0f - tx) + c10 * tx;
  const Eigen::Vector3f c1 = c01 * (1.0f - tx) + c11 * tx;
  return c0 * (1.0f - ty) + c1 * ty;
}

} // namespace spherical_tiling
