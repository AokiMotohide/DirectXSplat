#include "dxsplat/directxsplat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

#include "api/GaussianSplatsInternal.h"
#include "dxsplat/bounding.h"
#include "dxsplat/math.h"

namespace dxsplat {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kOrbitFovYRadians = 55.0f * kPi / 180.0f;

std::array<float, 9> BuildIntrinsic(uint32_t width, uint32_t height) {
  const float fy = static_cast<float>(height) * 0.5f / std::tan(kOrbitFovYRadians * 0.5f);
  const float fx = fy;
  const float cx = static_cast<float>(width) * 0.5f;
  const float cy = static_cast<float>(height) * 0.5f;
  return {
      fx, 0.0f, cx,
      0.0f, fy, cy,
      0.0f, 0.0f, 1.0f,
  };
}

std::array<float, 16> BuildLookAtOpenCvExtrinsic(const Vec3& eye, const Vec3& target) {
  const Vec3 worldUp{0.0f, 1.0f, 0.0f};
  const Vec3 forward = Normalize(target - eye);
  Vec3 right = Normalize(Cross(worldUp, forward));
  if (Length(right) <= 0.0f) {
    right = {1.0f, 0.0f, 0.0f};
  }
  const Vec3 down = Normalize(Cross(right, forward));

  return {
      right.x, right.y, right.z, -Dot(right, eye),
      down.x, down.y, down.z, -Dot(down, eye),
      forward.x, forward.y, forward.z, -Dot(forward, eye),
      0.0f, 0.0f, 0.0f, 1.0f,
  };
}

}  // namespace

CameraSet MakeOrbitCameraSet(const GaussianSplats& splats, uint32_t count, uint32_t width, uint32_t height) {
  CameraSet out{};
  const Aabb& bounds = BoundsFromSplats(splats);
  if (splats.Empty() || !bounds.valid || count == 0 || width == 0 || height == 0) {
    return out;
  }

  const Vec3 center = ComputeAabbCenter(bounds);
  const float radius = std::max(ComputeAabbRadius(bounds), 0.01f);
  const float distance = radius * 2.5f;
  const float eyeHeight = radius * 0.35f;
  const auto intrinsic = BuildIntrinsic(width, height);

  out.cameras.reserve(count);
  for (uint32_t index = 0; index < count; ++index) {
    const float t = static_cast<float>(index) / static_cast<float>(count);
    const float angle = t * kPi * 2.0f;
    const Vec3 eye{
        center.x + std::sin(angle) * distance,
        center.y + eyeHeight,
        center.z + std::cos(angle) * distance,
    };

    CameraParams camera{};
    camera.name = "orbit " + std::to_string(index);
    camera.width = width;
    camera.height = height;
    camera.intrinsic = intrinsic;
    camera.extrinsic = BuildLookAtOpenCvExtrinsic(eye, center);
    out.cameras.push_back(std::move(camera));
  }

  return out;
}

}  // namespace dxsplat
