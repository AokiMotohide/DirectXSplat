#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "dxsplat/bounding.h"
#include "dxsplat/math.h"
#include "dxsplat/vram_format.h"

namespace dxsplat {

constexpr uint32_t kShOrder3CoeffCountPerChannel = 16;
constexpr uint32_t kShOrder3CoeffCountTotal = kShOrder3CoeffCountPerChannel * 3;

struct Gaussian {
  Vec3 position{};
  Vec3 scale{1.0f, 1.0f, 1.0f};
  Quat rotation{};
  float opacity = 0.0f;
  std::array<float, kShOrder3CoeffCountTotal> sh{};
  uint32_t splatId = 0;
  uint32_t instanceId = 0;
};

struct GaussianSet {
  std::string name;
  std::vector<Gaussian> gaussians;
  Aabb bounds;
  bool visible = true;
  float scalingModifier = 1.0f;
};

struct InputCamera {
  std::string name;
  Vec3 position{};
  Quat rotation{};
  float fovYRadians = 1.0471975512f;
  std::string sourceImage;
};

struct Scene {
  std::string sourcePath;
  std::vector<GaussianSet> splatSets;
  std::vector<InputCamera> inputCameras;
  Aabb sceneBounds;
  VramFormatSettings vramFormat{};
};

}  // namespace dxsplat
