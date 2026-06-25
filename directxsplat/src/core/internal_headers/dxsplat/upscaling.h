#pragma once

#include <cstdint>

#include "dxsplat/gpu_resources.h"
#include "dxsplat/math.h"

namespace directxsplat {

struct UpscalerInput {
  GpuTextureView color{};
  GpuTextureView depth{};
  GpuTextureView motionVectors{};
  uint32_t renderWidth = 0;
  uint32_t renderHeight = 0;
  uint32_t outputWidth = 0;
  uint32_t outputHeight = 0;
  Mat4 view{};
  Mat4 proj{};
  Vec2 jitter{};
  bool cameraCut = false;
};

}  // namespace directxsplat
