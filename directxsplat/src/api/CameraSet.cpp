#include "api/CameraSetInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace dxsplat {

namespace {

constexpr uint32_t kDefaultCameraWidth = 1600;
constexpr uint32_t kDefaultCameraHeight = 900;

Vec3 RotateVector(const Quat& q, const Vec3& v) {
  const Vec3 u{q.x, q.y, q.z};
  const float s = q.w;
  return u * (2.0f * Dot(u, v)) + v * (s * s - Dot(u, u)) + Cross(u, v) * (2.0f * s);
}

std::array<float, 16> BuildOpenCvExtrinsic(const InputCamera& camera) {
  const Quat q = Normalize(camera.rotation);
  const Vec3 right = RotateVector(q, {1.0f, 0.0f, 0.0f});
  const Vec3 down = RotateVector(q, {0.0f, -1.0f, 0.0f});
  const Vec3 forward = RotateVector(q, {0.0f, 0.0f, 1.0f});
  const Vec3 origin = camera.position;

  return {
      right.x, right.y, right.z, -Dot(right, origin),
      down.x, down.y, down.z, -Dot(down, origin),
      forward.x, forward.y, forward.z, -Dot(forward, origin),
      0.0f, 0.0f, 0.0f, 1.0f,
  };
}

std::array<float, 9> BuildIntrinsic(uint32_t width, uint32_t height, float fovYRadians) {
  const float safeFovY = std::clamp(fovYRadians, 0.001f, 3.13f);
  const float fy = static_cast<float>(height) * 0.5f / std::tan(safeFovY * 0.5f);
  const float fx = fy;
  const float cx = static_cast<float>(width) * 0.5f;
  const float cy = static_cast<float>(height) * 0.5f;
  return {
      fx, 0.0f, cx,
      0.0f, fy, cy,
      0.0f, 0.0f, 1.0f,
  };
}

}  // namespace

CameraParams CameraParamsFromInputCamera(const InputCamera& input, uint32_t width, uint32_t height) {
  CameraParams camera{};
  camera.name = input.name;
  camera.width = width;
  camera.height = height;
  camera.extrinsic = BuildOpenCvExtrinsic(input);
  camera.intrinsic = BuildIntrinsic(width, height, input.fovYRadians);
  return camera;
}

StatusOr<CameraSet> ConvertInputCamerasToCameraSet(const Scene& scene) {
  CameraSet out{};
  out.cameras.reserve(scene.inputCameras.size());

  // Convert DirectXSplat cameras into public OpenCV matrices
  for (const auto& input : scene.inputCameras) {
    out.cameras.push_back(CameraParamsFromInputCamera(input, kDefaultCameraWidth, kDefaultCameraHeight));
  }

  return StatusOr<CameraSet>::Ok(std::move(out));
}

}  // namespace dxsplat
