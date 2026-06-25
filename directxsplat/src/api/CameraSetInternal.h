#pragma once

#include <array>
#include <cstddef>

#include "directxsplat/directxsplat.h"
#include "directxsplat/math.h"
#include "directxsplat/scene.h"
#include "directxsplat/status.h"

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
