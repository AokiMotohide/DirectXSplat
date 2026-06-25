#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "directxsplat/status.h"

namespace directxsplat::internal {

struct ImageRgba8 {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> pixels;

  bool Empty() const { return width == 0 || height == 0 || pixels.empty(); }
};

directxsplat::Status SavePpm(const ImageRgba8& image, const std::string& path);
directxsplat::StatusOr<ImageRgba8> LoadPpm(const std::string& path);

}  // namespace directxsplat::internal
