#include "tools/ScreenshotWriter.h"

#include <d3d12.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

#include "platform/Image.h"

namespace dxsplat {

namespace {

struct MappedReadback {
  ID3D12Resource* resource = nullptr;
  ~MappedReadback() {
    if (resource != nullptr) {
      resource->Unmap(0, nullptr);
    }
  }
};

Status ValidateCaptureLayout(uint32_t width,
                             uint32_t height,
                             uint32_t rowPitch,
                             uint64_t readbackSizeBytes,
                             size_t* outRowBytes = nullptr,
                             size_t* outPixelBytes = nullptr) {
  if (width == 0 || height == 0 || rowPitch == 0 || readbackSizeBytes == 0) {
    return Status::Error("invalid pending screenshot capture");
  }
  constexpr uint64_t kBytesPerPixel = 4;
  const uint64_t rowBytes = static_cast<uint64_t>(width) * kBytesPerPixel;
  if (rowBytes > rowPitch) {
    return Status::Error("invalid screenshot readback layout");
  }
  if (height > std::numeric_limits<uint64_t>::max() / rowPitch ||
      height > std::numeric_limits<uint64_t>::max() / rowBytes) {
    return Status::Error("screenshot readback layout is too large");
  }
  const uint64_t requiredReadbackBytes = static_cast<uint64_t>(rowPitch) * height;
  const uint64_t pixelBytes = rowBytes * height;
  if (readbackSizeBytes < requiredReadbackBytes) {
    return Status::Error("invalid screenshot readback layout");
  }
  if (rowBytes > std::numeric_limits<size_t>::max() || pixelBytes > std::numeric_limits<size_t>::max() ||
      requiredReadbackBytes > std::numeric_limits<size_t>::max()) {
    return Status::Error("screenshot readback layout is too large");
  }
  if (outRowBytes != nullptr) {
    *outRowBytes = static_cast<size_t>(rowBytes);
  }
  if (outPixelBytes != nullptr) {
    *outPixelBytes = static_cast<size_t>(pixelBytes);
  }
  return Status::Ok();
}

}

Status ScreenshotWriter::QueueBackBufferPpm(internal::SwapchainContext& context, const std::string& outputPath) {
  if (outputPath.empty()) {
    return Status::Error("invalid screenshot path");
  }
  if (pending_.active) {
    return Status::Error("screenshot capture already pending");
  }

  ID3D12Device* device = context.Device();
  ID3D12GraphicsCommandList* cmd = context.CommandList();
  ID3D12Resource* backBuffer = context.CurrentBackBuffer();
  if (device == nullptr || cmd == nullptr || backBuffer == nullptr) {
    return Status::Error("invalid screenshot capture context");
  }
  ID3D12Fence* fence = context.Fence();
  const uint64_t fenceValue = context.PendingSubmissionFenceValue();
  if (fence == nullptr || fenceValue == 0) {
    return Status::Error("invalid screenshot capture fence");
  }

  const D3D12_RESOURCE_DESC srcDesc = backBuffer->GetDesc();
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT rowCount = 0;
  UINT64 rowSizeBytes = 0;
  UINT64 totalBytes = 0;
  device->GetCopyableFootprints(&srcDesc, 0, 1, 0, &footprint, &rowCount, &rowSizeBytes, &totalBytes);
  if (totalBytes == 0 || rowCount == 0) {
    return Status::Error("failed to compute screenshot copy layout");
  }
  if (srcDesc.Width > std::numeric_limits<uint32_t>::max() || srcDesc.Height > std::numeric_limits<uint32_t>::max()) {
    return Status::Error("screenshot dimensions are too large");
  }
  Status layoutStatus = ValidateCaptureLayout(static_cast<uint32_t>(srcDesc.Width),
                                              static_cast<uint32_t>(srcDesc.Height),
                                              footprint.Footprint.RowPitch,
                                              totalBytes);
  if (!layoutStatus.ok) {
    return layoutStatus;
  }

  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_READBACK;
  heap.CreationNodeMask = 1;
  heap.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC readbackDesc{};
  readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  readbackDesc.Width = std::max<UINT64>(totalBytes, 4ull);
  readbackDesc.Height = 1;
  readbackDesc.DepthOrArraySize = 1;
  readbackDesc.MipLevels = 1;
  readbackDesc.SampleDesc.Count = 1;
  readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  Microsoft::WRL::ComPtr<ID3D12Resource> readback;
  const HRESULT hr = device->CreateCommittedResource(
      &heap, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(readback.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("failed creating screenshot readback buffer");
  }

  PendingCapture pending{};
  pending.readback = readback;
  pending.fence = fence;
  pending.fenceValue = fenceValue;
  pending.width = static_cast<uint32_t>(srcDesc.Width);
  pending.height = static_cast<uint32_t>(srcDesc.Height);
  pending.rowPitch = footprint.Footprint.RowPitch;
  pending.readbackSizeBytes = totalBytes;
  try {
    pending.outputPath = outputPath;
  } catch (const std::bad_alloc&) {
    return Status::Error("screenshot path allocation failed");
  } catch (const std::length_error&) {
    return Status::Error("screenshot path allocation failed");
  }
  pending.active = true;

  D3D12_RESOURCE_BARRIER toCopy{};
  toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  toCopy.Transition.pResource = backBuffer;
  toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmd->ResourceBarrier(1, &toCopy);

  D3D12_TEXTURE_COPY_LOCATION dstLoc{};
  dstLoc.pResource = readback.Get();
  dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLoc.PlacedFootprint = footprint;

  D3D12_TEXTURE_COPY_LOCATION srcLoc{};
  srcLoc.pResource = backBuffer;
  srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = 0;

  cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

  D3D12_RESOURCE_BARRIER toRt{};
  toRt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  toRt.Transition.pResource = backBuffer;
  toRt.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  toRt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  toRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmd->ResourceBarrier(1, &toRt);

  pending_ = std::move(pending);
  return Status::Ok();
}

Status ScreenshotWriter::ResolvePendingCapture(std::string* completedPath) {
  if (!pending_.active) {
    return Status::Ok();
  }

  if (pending_.readback == nullptr || pending_.width == 0 || pending_.height == 0 || pending_.rowPitch == 0 ||
      pending_.readbackSizeBytes == 0 || pending_.outputPath.empty() || pending_.fence == nullptr || pending_.fenceValue == 0) {
    pending_ = {};
    return Status::Error("invalid pending screenshot capture");
  }
  size_t rowBytes = 0;
  size_t pixelBytes = 0;
  Status layoutStatus = ValidateCaptureLayout(pending_.width,
                                              pending_.height,
                                              pending_.rowPitch,
                                              pending_.readbackSizeBytes,
                                              &rowBytes,
                                              &pixelBytes);
  if (!layoutStatus.ok) {
    pending_ = {};
    return layoutStatus;
  }
  if (pending_.fence->GetCompletedValue() < pending_.fenceValue) {
    return Status::Ok();
  }

  void* mapped = nullptr;
  const HRESULT hr = pending_.readback->Map(0, nullptr, &mapped);
  if (FAILED(hr) || mapped == nullptr) {
    pending_ = {};
    return Status::Error("failed mapping screenshot readback");
  }

  internal::ImageRgba8 image{};
  image.width = pending_.width;
  image.height = pending_.height;
  Status copyStatus = Status::Ok();
  {
    MappedReadback mappedReadback{pending_.readback.Get()};
    try {
      image.pixels.resize(pixelBytes);
      const uint8_t* src = reinterpret_cast<const uint8_t*>(mapped);
      for (uint32_t y = 0; y < image.height; ++y) {
        const uint8_t* srcRow = src + static_cast<size_t>(y) * pending_.rowPitch;
        uint8_t* dstRow = image.pixels.data() + static_cast<size_t>(y) * rowBytes;
        std::memcpy(dstRow, srcRow, rowBytes);
      }
    } catch (const std::bad_alloc&) {
      copyStatus = Status::Error("screenshot allocation failed");
    } catch (const std::length_error&) {
      copyStatus = Status::Error("screenshot allocation failed");
    }
  }
  if (!copyStatus.ok) {
    pending_ = {};
    return copyStatus;
  }

  std::string outputPath;
  try {
    outputPath = pending_.outputPath;
  } catch (const std::bad_alloc&) {
    pending_ = {};
    return Status::Error("screenshot path allocation failed");
  } catch (const std::length_error&) {
    pending_ = {};
    return Status::Error("screenshot path allocation failed");
  }
  pending_ = {};

  Status save = internal::SavePpm(image, outputPath);
  if (!save.ok) {
    return save;
  }

  if (completedPath != nullptr) {
    *completedPath = outputPath;
  }
  return Status::Ok();
}

bool ScreenshotWriter::HasPendingCapture() const {
  return pending_.active;
}

}  // namespace dxsplat
