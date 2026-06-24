#include "dxsplat/directxsplat.h"

#include <array>
#include <cmath>
#include <mutex>

#include "dxsplat/math.h"
#include "api/OwnedD3D12Runtime.h"

namespace dxsplat {

namespace {

constexpr float kCameraBasisEpsilon = 1e-5f;

bool Finite(float v) {
  return std::isfinite(v);
}

template <size_t Count>
bool Finite(const std::array<float, Count>& values) {
  for (float value : values) {
    if (!Finite(value)) {
      return false;
    }
  }
  return true;
}

bool ValidCameraBasis(const std::array<float, 16>& e) {
  const Vec3 right{e[0], e[1], e[2]};
  const Vec3 down{e[4], e[5], e[6]};
  const Vec3 forward{e[8], e[9], e[10]};
  return Length(right) > kCameraBasisEpsilon &&
         Length(down) > kCameraBasisEpsilon &&
         Length(forward) > kCameraBasisEpsilon;
}

Status ValidateDrawInputs(const GaussianSplats& splats, const CameraParams& camera, const DrawOptions& options) {
  if (splats.Empty()) {
    return Status::Error("splats are empty");
  }
  if (options.width == 0) {
    return Status::Error("draw width must be greater than zero");
  }
  if (options.height == 0) {
    return Status::Error("draw height must be greater than zero");
  }
  if (camera.width == 0) {
    return Status::Error("camera width must be greater than zero");
  }
  if (camera.height == 0) {
    return Status::Error("camera height must be greater than zero");
  }
  if (!Finite(camera.extrinsic) || !Finite(camera.intrinsic)) {
    return Status::Error("camera matrices must be finite");
  }
  if (!ValidCameraBasis(camera.extrinsic)) {
    return Status::Error("camera extrinsic is invalid");
  }
  if (camera.intrinsic[0] <= 0.0f || camera.intrinsic[4] <= 0.0f ||
      std::abs(camera.intrinsic[8]) <= kCameraBasisEpsilon) {
    return Status::Error("camera intrinsic is invalid");
  }
  if (!Finite(options.nearPlane) || !Finite(options.farPlane) || options.farPlane <= options.nearPlane) {
    return Status::Error("far plane must be greater than near plane");
  }
  if (!Finite(options.background[0]) || !Finite(options.background[1]) || !Finite(options.background[2]) ||
      !Finite(options.antialiasingStrength)) {
    return Status::Error("draw options must be finite");
  }
  return Status::Ok();
}

}  // namespace

StatusOr<ImageRgba8> Draw(const GaussianSplats& splats, const CameraParams& camera, const DrawOptions& options) {
  Status validation = ValidateDrawInputs(splats, camera, options);
  if (!validation.ok) {
    return StatusOr<ImageRgba8>::Error(validation.message);
  }

  static std::mutex runtimeMutex;
  static OwnedD3D12Runtime runtime;
  std::lock_guard<std::mutex> lock(runtimeMutex);
  return runtime.Draw(splats, camera, options);
}

}  // namespace dxsplat
