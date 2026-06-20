#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dxsplat/status.h"

namespace dxsplat::internal {

struct ImageRgba8 {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> pixels;

  bool Empty() const { return width == 0 || height == 0 || pixels.empty(); }
};

dxsplat::Status SavePpm(const ImageRgba8& image, const std::string& path);
dxsplat::StatusOr<ImageRgba8> LoadPpm(const std::string& path);

}  // namespace dxsplat::internal
