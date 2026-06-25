#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dxsplat/status.h"

namespace directxsplat::io {

struct DecodedImage {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> rgba;
};

StatusOr<DecodedImage> DecodeImageFromFileWic(const std::string& path);
StatusOr<DecodedImage> DecodeImageFromMemoryWic(const std::vector<uint8_t>& bytes);

}

