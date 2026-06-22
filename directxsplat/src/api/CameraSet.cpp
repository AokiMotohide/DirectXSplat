#include "api/CameraSetInternal.h"

#include <array>
#include <utility>

namespace dxsplat {

namespace {

constexpr std::array<float, 9> kIdentityIntrinsic{
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
};

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

}  // namespace

StatusOr<CameraSet> ConvertInputCamerasToCameraSet(const Scene& scene) {
  CameraSet out{};
  out.cameras.reserve(scene.inputCameras.size());

  // Convert DirectXSplat cameras into public OpenCV matrices
  for (const auto& input : scene.inputCameras) {
    CameraParams camera{};
    camera.name = input.name;
    camera.extrinsic = BuildOpenCvExtrinsic(input);
    camera.intrinsic = kIdentityIntrinsic;
    out.cameras.push_back(std::move(camera));
  }

  return StatusOr<CameraSet>::Ok(std::move(out));
}

}  // namespace dxsplat
