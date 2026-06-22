#pragma once

#include "dxsplat/directxsplat.h"
#include "dxsplat/scene.h"
#include "dxsplat/status.h"

namespace dxsplat {

StatusOr<CameraSet> ConvertInputCamerasToCameraSet(const Scene& scene);

}  // namespace dxsplat
