#include "directxsplat/directxsplat.h"

#include <cmath>
#include <mutex>

#include "api/CameraSetInternal.h"
#include "api/OwnedD3D12Runtime.h"

namespace directxsplat {

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
  Status cameraStatus = ValidateCameraParamsForRendering(camera);
  if (!cameraStatus.ok) {
    return cameraStatus;
  }
  if (!std::isfinite(options.nearPlane) || !std::isfinite(options.farPlane) || options.farPlane <= options.nearPlane) {
    return Status::Error("far plane must be greater than near plane");
  }
  if (!std::isfinite(options.background[0]) || !std::isfinite(options.background[1]) ||
      !std::isfinite(options.background[2]) || !std::isfinite(options.antialiasingStrength)) {
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

}  // namespace directxsplat
