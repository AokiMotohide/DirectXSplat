#pragma once

#include "dxsplat/directxsplat.h"
#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace dxsplat {

CameraParams CameraParamsFromInputCamera(const InputCamera& input, uint32_t width, uint32_t height);
StatusOr<CameraSet> ConvertInputCamerasToCameraSet(const Scene& scene);

}  // namespace dxsplat
