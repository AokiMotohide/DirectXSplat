#pragma once

#include <d3d12.h>

#include <cstdint>
#include <functional>

#include "dxsplat/gpu_resources.h"
#include "dxsplat/settings.h"
#include "dxsplat/types.h"

namespace directxsplat {

enum class RenderHookStage {
  BeforePrepare,
  AfterPrepare,
  AfterSort,
  BeforeRaster,
  AfterRaster,
  BeforePostProcess,
  AfterPostProcess,
};

struct RenderHookContext {
  RenderHookStage stage = RenderHookStage::BeforePrepare;
  ID3D12GraphicsCommandList* commandList = nullptr;
  UploadedSceneHandle scene{};
  const RenderInput* input = nullptr;
  const RenderTargetBinding* targets = nullptr;
  const GpuFrameResources* resources = nullptr;
  FrameStats* stats = nullptr;
  uint64_t frameIndex = 0;
};

struct RenderHooks {
  std::function<void(const RenderHookContext&)> beforePrepare;
  std::function<void(const RenderHookContext&)> afterPrepare;
  std::function<void(const RenderHookContext&)> afterSort;
  std::function<void(const RenderHookContext&)> beforeRaster;
  std::function<void(const RenderHookContext&)> afterRaster;
  std::function<void(const RenderHookContext&)> beforePostProcess;
  std::function<void(const RenderHookContext&)> afterPostProcess;
};

}  // namespace directxsplat
