#include "dxsplat/directxsplat.h"

#include <mutex>

#include "api/OwnedD3D12Runtime.h"

namespace dxsplat {

namespace {

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
  if (options.farPlane <= options.nearPlane) {
    return Status::Error("far plane must be greater than near plane");
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
