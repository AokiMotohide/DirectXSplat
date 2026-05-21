#include "dxsplat_examples/ExampleD3D12.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace dxsplat::examples {
namespace {

using Microsoft::WRL::ComPtr;

std::string HrString(HRESULT hr) {
  std::ostringstream stream;
  stream << "0x" << std::hex << static_cast<unsigned long>(hr);
  return stream.str();
}

D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource,
                                         D3D12_RESOURCE_STATES before,
                                         D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = resource;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore = before;
  barrier.Transition.StateAfter = after;
  return barrier;
}

}  // namespace

D3D12ExampleDevice::~D3D12ExampleDevice() {
  Shutdown();
}

Status D3D12ExampleDevice::Initialize(bool forceWarp) {
  Shutdown();

  HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory_.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateDXGIFactory2 failed " + HrString(hr));
  }

  if (!forceWarp) {
    for (UINT adapterIndex = 0;; ++adapterIndex) {
      ComPtr<IDXGIAdapter1> candidate;
      hr = factory_->EnumAdapterByGpuPreference(adapterIndex,
                                                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                IID_PPV_ARGS(candidate.GetAddressOf()));
      if (hr == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (FAILED(hr)) {
        break;
      }
      DXGI_ADAPTER_DESC1 desc{};
      candidate->GetDesc1(&desc);
      if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
        continue;
      }
      hr = D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(device_.GetAddressOf()));
      if (SUCCEEDED(hr)) {
        adapter_ = candidate;
        break;
      }
    }
  }

  if (device_ == nullptr) {
    ComPtr<IDXGIAdapter> warp;
    hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf()));
    if (FAILED(hr)) {
      return Status::Error("EnumWarpAdapter failed " + HrString(hr));
    }
    hr = D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(device_.GetAddressOf()));
    if (FAILED(hr)) {
      return Status::Error("D3D12CreateDevice failed " + HrString(hr));
    }
    warp.As(&adapter_);
  }

  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue_.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateCommandQueue failed " + HrString(hr));
  }

  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator_.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateCommandAllocator failed " + HrString(hr));
  }

  hr = device_->CreateCommandList(0,
                                  D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  allocator_.Get(),
                                  nullptr,
                                  IID_PPV_ARGS(commandList_.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateCommandList failed " + HrString(hr));
  }

  hr = commandList_->Close();
  if (FAILED(hr)) {
    return Status::Error("initial command list close failed " + HrString(hr));
  }

  hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateFence failed " + HrString(hr));
  }

  fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (fenceEvent_ == nullptr) {
    return Status::Error("CreateEvent failed");
  }

  return Status::Ok();
}

void D3D12ExampleDevice::Shutdown() {
  if (queue_ != nullptr && fence_ != nullptr) {
    const uint64_t value = fenceValue_ + 1;
    if (SUCCEEDED(queue_->Signal(fence_.Get(), value))) {
      fenceValue_ = value;
      (void)WaitForFence(value);
    }
  }
  if (fenceEvent_ != nullptr) {
    CloseHandle(fenceEvent_);
    fenceEvent_ = nullptr;
  }
  commandList_.Reset();
  allocator_.Reset();
  fence_.Reset();
  queue_.Reset();
  device_.Reset();
  adapter_.Reset();
  factory_.Reset();
  fenceValue_ = 0;
}

Status D3D12ExampleDevice::BeginCommands() {
  HRESULT hr = allocator_->Reset();
  if (FAILED(hr)) {
    return Status::Error("command allocator reset failed " + HrString(hr));
  }
  hr = commandList_->Reset(allocator_.Get(), nullptr);
  if (FAILED(hr)) {
    return Status::Error("command list reset failed " + HrString(hr));
  }
  return Status::Ok();
}

Status D3D12ExampleDevice::FinishCommands(UploadSyncPoint uploadSyncPoint, bool executeCommandList) {
  if (uploadSyncPoint.IsValid()) {
    HRESULT waitHr = queue_->Wait(uploadSyncPoint.fence, uploadSyncPoint.value);
    if (FAILED(waitHr)) {
      return Status::Error("direct queue upload wait failed " + HrString(waitHr));
    }
  }

  HRESULT hr = commandList_->Close();
  if (FAILED(hr)) {
    return Status::Error("command list close failed " + HrString(hr));
  }

  if (!executeCommandList) {
    return Status::Ok();
  }

  ID3D12CommandList* lists[] = {commandList_.Get()};
  queue_->ExecuteCommandLists(1, lists);
  return SignalFrame({});
}

Status D3D12ExampleDevice::SignalFrame(UploadSyncPoint uploadSyncPoint) {
  if (uploadSyncPoint.IsValid()) {
    HRESULT waitHr = queue_->Wait(uploadSyncPoint.fence, uploadSyncPoint.value);
    if (FAILED(waitHr)) {
      return Status::Error("direct queue upload wait failed " + HrString(waitHr));
    }
  }

  const uint64_t value = fenceValue_ + 1;
  HRESULT hr = queue_->Signal(fence_.Get(), value);
  if (FAILED(hr)) {
    return Status::Error("queue signal failed " + HrString(hr));
  }
  fenceValue_ = value;
  return WaitForFence(value);
}

RenderFrameContext D3D12ExampleDevice::FrameContext() const {
  RenderFrameContext context{};
  context.fence = fence_.Get();
  context.completedFenceValue = CompletedFenceValue();
  context.submissionFenceValue = NextFenceValue();
  context.frameIndex = fenceValue_;
  return context;
}

Status D3D12ExampleDevice::CreateOffscreenTarget(uint32_t width, uint32_t height, OffscreenTarget& outTarget) {
  outTarget = {};
  outTarget.width = std::max(width, 1u);
  outTarget.height = std::max(height, 1u);

  D3D12_HEAP_PROPERTIES defaultHeap{};
  defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
  defaultHeap.CreationNodeMask = 1;
  defaultHeap.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC colorDesc{};
  colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  colorDesc.Width = outTarget.width;
  colorDesc.Height = outTarget.height;
  colorDesc.DepthOrArraySize = 1;
  colorDesc.MipLevels = 1;
  colorDesc.Format = outTarget.format;
  colorDesc.SampleDesc.Count = 1;
  colorDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_CLEAR_VALUE clear{};
  clear.Format = outTarget.format;
  clear.Color[3] = 1.0f;

  HRESULT hr = device_->CreateCommittedResource(&defaultHeap,
                                                D3D12_HEAP_FLAG_NONE,
                                                &colorDesc,
                                                D3D12_RESOURCE_STATE_COMMON,
                                                &clear,
                                                IID_PPV_ARGS(outTarget.color.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateCommittedResource color failed " + HrString(hr));
  }

  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
  rtvHeapDesc.NumDescriptors = 1;
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  hr = device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(outTarget.rtvHeap.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateDescriptorHeap RTV failed " + HrString(hr));
  }

  outTarget.rtv = outTarget.rtvHeap->GetCPUDescriptorHandleForHeapStart();
  device_->CreateRenderTargetView(outTarget.color.Get(), nullptr, outTarget.rtv);

  UINT64 totalBytes = 0;
  device_->GetCopyableFootprints(&colorDesc, 0, 1, 0, &outTarget.footprint, nullptr, nullptr, &totalBytes);
  outTarget.readbackSizeBytes = totalBytes;

  D3D12_HEAP_PROPERTIES readbackHeap{};
  readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
  readbackHeap.CreationNodeMask = 1;
  readbackHeap.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC readbackDesc{};
  readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  readbackDesc.Width = totalBytes;
  readbackDesc.Height = 1;
  readbackDesc.DepthOrArraySize = 1;
  readbackDesc.MipLevels = 1;
  readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
  readbackDesc.SampleDesc.Count = 1;
  readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  hr = device_->CreateCommittedResource(&readbackHeap,
                                        D3D12_HEAP_FLAG_NONE,
                                        &readbackDesc,
                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                        nullptr,
                                        IID_PPV_ARGS(outTarget.readback.GetAddressOf()));
  if (FAILED(hr)) {
    return Status::Error("CreateCommittedResource readback failed " + HrString(hr));
  }

  return Status::Ok();
}

Status D3D12ExampleDevice::RecordReadback(const OffscreenTarget& target) {
  if (target.color == nullptr || target.readback == nullptr) {
    return Status::Error("invalid offscreen target");
  }

  D3D12_TEXTURE_COPY_LOCATION src{};
  src.pResource = target.color.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src.SubresourceIndex = 0;

  D3D12_TEXTURE_COPY_LOCATION dst{};
  dst.pResource = target.readback.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.PlacedFootprint = target.footprint;

  commandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
  D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(target.color.Get(),
                                                     D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                     D3D12_RESOURCE_STATE_RENDER_TARGET);
  commandList_->ResourceBarrier(1, &barrier);
  return Status::Ok();
}

Status D3D12ExampleDevice::ReadbackImage(const OffscreenTarget& target, appcommon::ImageRgba8& outImage) try {
  outImage = {};
  if (target.readback == nullptr || target.width == 0 || target.height == 0) {
    return Status::Error("invalid readback target");
  }

  const size_t pixelBytes = static_cast<size_t>(target.width) * target.height * 4u;
  appcommon::ImageRgba8 image{};
  image.width = target.width;
  image.height = target.height;
  image.pixels.resize(pixelBytes);

  D3D12_RANGE readRange{0, static_cast<SIZE_T>(target.readbackSizeBytes)};
  void* mapped = nullptr;
  HRESULT hr = target.readback->Map(0, &readRange, &mapped);
  if (FAILED(hr) || mapped == nullptr) {
    return Status::Error("readback map failed " + HrString(hr));
  }

  const auto* src = static_cast<const uint8_t*>(mapped);
  for (uint32_t y = 0; y < target.height; ++y) {
    const uint8_t* row = src + static_cast<size_t>(target.footprint.Footprint.RowPitch) * y;
    std::memcpy(image.pixels.data() + static_cast<size_t>(target.width) * y * 4u, row, static_cast<size_t>(target.width) * 4u);
  }

  D3D12_RANGE writeRange{0, 0};
  target.readback->Unmap(0, &writeRange);
  outImage = std::move(image);
  return Status::Ok();
} catch (const std::bad_alloc&) {
  return Status::Error("readback image allocation failed");
} catch (const std::length_error&) {
  return Status::Error("readback image allocation failed");
}

Status D3D12ExampleDevice::WaitForFence(uint64_t value) {
  if (fence_ == nullptr || fenceEvent_ == nullptr) {
    return Status::Error("fence is not initialized");
  }
  if (fence_->GetCompletedValue() >= value) {
    return Status::Ok();
  }
  HRESULT hr = fence_->SetEventOnCompletion(value, fenceEvent_);
  if (FAILED(hr)) {
    return Status::Error("SetEventOnCompletion failed " + HrString(hr));
  }
  const DWORD wait = WaitForSingleObject(fenceEvent_, INFINITE);
  if (wait != WAIT_OBJECT_0) {
    return Status::Error("WaitForSingleObject failed");
  }
  return Status::Ok();
}

}  // namespace dxsplat::examples
