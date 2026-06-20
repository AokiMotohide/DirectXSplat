#pragma once

#include <cstdint>
#include <string>

#include <wrl/client.h>

#include "platform/SwapchainContext.h"
#include "dxsplat/status.h"

namespace dxsplat {

class ScreenshotWriter {
 public:
  Status QueueBackBufferPpm(internal::SwapchainContext& context, const std::string& outputPath);
  Status ResolvePendingCapture(std::string* completedPath = nullptr);
  bool HasPendingCapture() const;

 private:
  struct PendingCapture {
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    uint64_t fenceValue = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t rowPitch = 0;
    uint64_t readbackSizeBytes = 0;
    std::string outputPath;
    bool active = false;
  };

  PendingCapture pending_{};
};

}  // namespace dxsplat
