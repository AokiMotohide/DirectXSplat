#pragma once

#include <array>
#include <cstddef>

#include "dxsplat/directxsplat.h"
#include "dxsplat/math.h"
#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace directxsplat {

struct CameraRenderState {
  Mat4 view{};
  Mat4 proj{};
  Vec3 position{};
};

Status ValidateCameraParamsForRendering(const CameraParams& camera);
CameraRenderState CameraRenderStateFromCameraParams(const CameraParams& camera, float nearPlane, float farPlane);
Vec3 CameraPoseUpFromExtrinsic(const std::array<float, 16>& extrinsic);
CameraParams CameraParamsFromInputCamera(const InputCamera& input, uint32_t width, uint32_t height);
InputCamera InputCameraFromCameraParams(const CameraParams& camera, size_t index);
StatusOr<CameraSet> ConvertInputCamerasToCameraSet(const Scene& scene);

}  // namespace directxsplat
