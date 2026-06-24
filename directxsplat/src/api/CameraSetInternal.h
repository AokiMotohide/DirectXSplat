#pragma once

#include <cstddef>

#include "dxsplat/directxsplat.h"
#include "dxsplat/math.h"
#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace dxsplat {

struct CameraRenderState {
  Mat4 view{};
  Mat4 proj{};
  Vec3 position{};
  float fovYRadians = 1.0471975512f;
};

Status ValidateCameraParamsForRendering(const CameraParams& camera);
CameraRenderState CameraRenderStateFromCameraParams(const CameraParams& camera, float nearPlane, float farPlane);
CameraParams CameraParamsFromInputCamera(const InputCamera& input, uint32_t width, uint32_t height);
InputCamera InputCameraFromCameraParams(const CameraParams& camera, size_t index);
StatusOr<CameraSet> ConvertInputCamerasToCameraSet(const Scene& scene);

}  // namespace dxsplat
