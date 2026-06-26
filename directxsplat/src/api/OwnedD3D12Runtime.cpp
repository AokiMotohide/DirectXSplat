#include "api/OwnedD3D12Runtime.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>

#include "api/CameraSetInternal.h"
#include "api/GaussianSplatsInternal.h"

namespace directxsplat {

namespace {

using Microsoft::WRL::ComPtr;

constexpr uint64_t kBytesPerPixel = 4;
constexpr DWORD kFenceWaitPollMs = 50;

std::string HrString(HRESULT hr) {
  std::ostringstream ss;
  ss << "0x" << std::hex << static_cast<unsigned long>(hr);
  return ss.str();
}

Status HrStatus(HRESULT hr, const char* message) {
  return Status::Error(std::string(message) + " " + HrString(hr));
}

Status CheckDeviceRemoved(ID3D12Device* device) {
  if (device == nullptr) {
    return Status::Ok();
  }
  const HRESULT removed = device->GetDeviceRemovedReason();
  if (FAILED(removed)) {
    return HrStatus(removed, "D3D12 device removed");
  }
  return Status::Ok();
}

Status ValidateReadbackLayout(uint32_t width, uint32_t height, uint32_t rowPitch, uint64_t readbackBytes) {
  if (width == 0 || height == 0 || rowPitch == 0 || readbackBytes == 0) {
    return Status::Error("invalid draw readback layout");
  }
  const uint64_t rowBytes = static_cast<uint64_t>(width) * kBytesPerPixel;
  if (rowBytes > rowPitch) {
    return Status::Error("invalid draw readback layout");
  }
  if (height > std::numeric_limits<uint64_t>::max() / rowPitch || height > std::numeric_limits<uint64_t>::max() / rowBytes) {
    return Status::Error("draw image is too large");
  }
  const uint64_t requiredReadbackBytes = static_cast<uint64_t>(height) * rowPitch;
  const uint64_t pixelBytes = static_cast<uint64_t>(height) * rowBytes;
  if (readbackBytes < requiredReadbackBytes) {
    return Status::Error("invalid draw readback layout");
  }
  if (pixelBytes > std::numeric_limits<size_t>::max() || rowBytes > std::numeric_limits<size_t>::max()) {
    return Status::Error("draw image is too large");
  }
  return Status::Ok();
}

}  // namespace

OwnedD3D12Runtime::~OwnedD3D12Runtime() {
  renderer_.Shutdown();
  context_.Shutdown();
  if (fenceEvent_ != nullptr) {
    CloseHandle(fenceEvent_);
    fenceEvent_ = nullptr;
  }
}

Status OwnedD3D12Runtime::EnsureInitialized() {
  if (initialized_) {
    return Status::Ok();
  }

  HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory_.GetAddressOf()));
  if (FAILED(hr)) {
    ComPtr<IDXGIFactory1> factory1;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(factory1.GetAddressOf()));
    if (SUCCEEDED(hr)) {
      hr = factory1.As(&factory_);
    }
  }
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating DXGI factory");
  }

  for (UINT index = 0;; ++index) {
    ComPtr<IDXGIAdapter1> candidate;
    hr = factory_->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(candidate.GetAddressOf()));
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
    if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.ReleaseAndGetAddressOf())))) {
      adapter_ = candidate;
      break;
    }
  }

  if (device_ == nullptr) {
    hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(adapter_.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed acquiring WARP adapter");
    }
    hr = D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) {
      return HrStatus(hr, "failed creating D3D12 device");
    }
  }

  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue_.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating direct queue");
  }

  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator_.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating command allocator");
  }

  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr,
                                  IID_PPV_ARGS(commandList_.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating command list");
  }
  hr = commandList_->Close();
  if (FAILED(hr)) {
    return HrStatus(hr, "failed closing command list");
  }

  hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating fence");
  }
  fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (fenceEvent_ == nullptr) {
    return Status::Error("failed creating fence event");
  }

  Status contextStatus = context_.Initialize(device_.Get(), queue_.Get(), fence_.Get());
  if (!contextStatus.ok) {
    return contextStatus;
  }
  Status rendererStatus = renderer_.Initialize(context_);
  if (!rendererStatus.ok) {
    return rendererStatus;
  }

  initialized_ = true;
  return Status::Ok();
}

Status OwnedD3D12Runtime::EnsureOffscreenTarget(uint32_t width, uint32_t height) {
  if (colorTarget_ != nullptr && colorReadback_ != nullptr && width_ == width && height_ == height) {
    return Status::Ok();
  }

  colorTarget_.Reset();
  colorReadback_.Reset();
  rtvHeap_.Reset();
  rtv_ = {};
  footprint_ = {};
  readbackBytes_ = 0;
  width_ = width;
  height_ = height;
  colorState_ = D3D12_RESOURCE_STATE_COMMON;

  D3D12_HEAP_PROPERTIES heapProps{};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  heapProps.CreationNodeMask = 1;
  heapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width = width;
  desc.Height = height;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_CLEAR_VALUE clearValue{};
  clearValue.Format = desc.Format;
  clearValue.Color[3] = 1.0f;

  HRESULT hr = device_->CreateCommittedResource(&heapProps,
                                                D3D12_HEAP_FLAG_NONE,
                                                &desc,
                                                D3D12_RESOURCE_STATE_COMMON,
                                                &clearValue,
                                                IID_PPV_ARGS(colorTarget_.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating draw color target");
  }

  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 1;
  hr = device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating draw RTV heap");
  }
  rtv_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
  device_->CreateRenderTargetView(colorTarget_.Get(), nullptr, rtv_);

  UINT rowCount = 0;
  UINT64 rowBytes = 0;
  UINT64 totalBytes = 0;
  device_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint_, &rowCount, &rowBytes, &totalBytes);
  readbackBytes_ = totalBytes;
  Status layoutStatus = ValidateReadbackLayout(width_, height_, footprint_.Footprint.RowPitch, readbackBytes_);
  if (!layoutStatus.ok) {
    return layoutStatus;
  }

  D3D12_HEAP_PROPERTIES readbackHeapProps{};
  readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK;
  readbackHeapProps.CreationNodeMask = 1;
  readbackHeapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC readbackDesc{};
  readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  readbackDesc.Width = std::max<UINT64>(totalBytes, 4ull);
  readbackDesc.Height = 1;
  readbackDesc.DepthOrArraySize = 1;
  readbackDesc.MipLevels = 1;
  readbackDesc.SampleDesc.Count = 1;
  readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  hr = device_->CreateCommittedResource(&readbackHeapProps,
                                        D3D12_HEAP_FLAG_NONE,
                                        &readbackDesc,
                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                        nullptr,
                                        IID_PPV_ARGS(colorReadback_.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating draw readback buffer");
  }

  return Status::Ok();
}

Status OwnedD3D12Runtime::ResetCommandList() {
  HRESULT hr = allocator_->Reset();
  if (FAILED(hr)) {
    return HrStatus(hr, "failed resetting command allocator");
  }
  hr = commandList_->Reset(allocator_.Get(), nullptr);
  if (FAILED(hr)) {
    return HrStatus(hr, "failed resetting command list");
  }
  return Status::Ok();
}

Status OwnedD3D12Runtime::ExecuteAndWait(UploadSyncPoint sync) {
  if (sync.IsValid()) {
    HRESULT hr = queue_->Wait(sync.fence, sync.value);
    if (FAILED(hr)) {
      return HrStatus(hr, "failed waiting for upload sync point");
    }
  }

  HRESULT hr = commandList_->Close();
  if (FAILED(hr)) {
    return HrStatus(hr, "failed closing command list");
  }

  ID3D12CommandList* lists[] = {commandList_.Get()};
  queue_->ExecuteCommandLists(1, lists);
  const uint64_t targetFence = fenceValue_ + 1;
  hr = queue_->Signal(fence_.Get(), targetFence);
  if (FAILED(hr)) {
    return HrStatus(hr, "failed signaling fence");
  }
  fenceValue_ = targetFence;

  if (fence_->GetCompletedValue() < targetFence) {
    Status deviceStatus = CheckDeviceRemoved(device_.Get());
    if (!deviceStatus.ok) {
      return deviceStatus;
    }
    hr = fence_->SetEventOnCompletion(targetFence, fenceEvent_);
    if (FAILED(hr)) {
      deviceStatus = CheckDeviceRemoved(device_.Get());
      return deviceStatus.ok ? HrStatus(hr, "failed waiting for fence") : deviceStatus;
    }
    while (fence_->GetCompletedValue() < targetFence) {
      const DWORD wait = WaitForSingleObject(fenceEvent_, kFenceWaitPollMs);
      if (wait == WAIT_OBJECT_0) {
        break;
      }
      if (wait != WAIT_TIMEOUT) {
        return Status::Error("failed waiting for fence");
      }
      deviceStatus = CheckDeviceRemoved(device_.Get());
      if (!deviceStatus.ok) {
        return deviceStatus;
      }
    }
  }
  return Status::Ok();
}

RenderFrameContext OwnedD3D12Runtime::FrameContext() const {
  RenderFrameContext out{};
  out.fence = fence_.Get();
  out.completedFenceValue = fence_ != nullptr ? fence_->GetCompletedValue() : 0;
  out.submissionFenceValue = fenceValue_ + 1;
  out.frameIndex = fenceValue_;
  return out;
}

RenderTargetBinding OwnedD3D12Runtime::TargetBinding(const DrawOptions& options) const {
  RenderTargetBinding target{};
  target.colorTarget = colorTarget_.Get();
  target.colorRtv = rtv_;
  target.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
  target.colorStateBefore = colorState_;
  target.colorStateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  target.transitionMode = ResourceTransitionMode::LibraryManaged;
  target.clearColor = true;
  const bool accumulationView = options.renderType == RenderType::Alpha || options.renderType == RenderType::Depth;
  target.clearColorValue[0] = accumulationView ? 0.0f : options.background[0];
  target.clearColorValue[1] = accumulationView ? 0.0f : options.background[1];
  target.clearColorValue[2] = accumulationView ? 0.0f : options.background[2];
  target.clearColorValue[3] = accumulationView ? 0.0f : 1.0f;
  target.viewport.TopLeftX = 0.0f;
  target.viewport.TopLeftY = 0.0f;
  target.viewport.Width = static_cast<float>(options.width);
  target.viewport.Height = static_cast<float>(options.height);
  target.viewport.MinDepth = 0.0f;
  target.viewport.MaxDepth = 1.0f;
  target.scissor.left = 0;
  target.scissor.top = 0;
  target.scissor.right = static_cast<LONG>(options.width);
  target.scissor.bottom = static_cast<LONG>(options.height);
  return target;
}

void OwnedD3D12Runtime::QueueColorReadback() {
  D3D12_TEXTURE_COPY_LOCATION src{};
  src.pResource = colorTarget_.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src.SubresourceIndex = 0;

  D3D12_TEXTURE_COPY_LOCATION dst{};
  dst.pResource = colorReadback_.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.PlacedFootprint = footprint_;

  commandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

StatusOr<ImageRgba8> OwnedD3D12Runtime::ReadbackImage() const {
  Status layoutStatus = ValidateReadbackLayout(width_, height_, footprint_.Footprint.RowPitch, readbackBytes_);
  if (!layoutStatus.ok) {
    return StatusOr<ImageRgba8>::Error(layoutStatus.message);
  }

  ImageRgba8 image{};
  image.width = width_;
  image.height = height_;
  const uint64_t rowBytes64 = static_cast<uint64_t>(width_) * kBytesPerPixel;
  const uint64_t pixelBytes64 = static_cast<uint64_t>(height_) * rowBytes64;
  try {
    image.pixels.resize(static_cast<size_t>(pixelBytes64));
  } catch (const std::bad_alloc&) {
    return StatusOr<ImageRgba8>::Error("draw image allocation failed");
  } catch (const std::length_error&) {
    return StatusOr<ImageRgba8>::Error("draw image allocation failed");
  }

  void* mapped = nullptr;
  const HRESULT hr = colorReadback_->Map(0, nullptr, &mapped);
  if (FAILED(hr) || mapped == nullptr) {
    return StatusOr<ImageRgba8>::Error("failed mapping draw readback");
  }

  const uint8_t* src = reinterpret_cast<const uint8_t*>(mapped);
  const size_t rowBytes = static_cast<size_t>(rowBytes64);
  for (uint32_t y = 0; y < height_; ++y) {
    const uint8_t* srcRow = src + static_cast<size_t>(y) * footprint_.Footprint.RowPitch;
    uint8_t* dstRow = image.pixels.data() + static_cast<size_t>(y) * rowBytes;
    std::memcpy(dstRow, srcRow, rowBytes);
  }
  colorReadback_->Unmap(0, nullptr);

  return StatusOr<ImageRgba8>::Ok(std::move(image));
}

StatusOr<ImageRgba8> OwnedD3D12Runtime::Draw(const GaussianSplats& splats, const CameraParams& camera, const DrawOptions& options) {
  Status initStatus = EnsureInitialized();
  if (!initStatus.ok) {
    return StatusOr<ImageRgba8>::Error(initStatus.message);
  }
  Status targetStatus = EnsureOffscreenTarget(options.width, options.height);
  if (!targetStatus.ok) {
    return StatusOr<ImageRgba8>::Error(targetStatus.message);
  }

  UploadedSceneHandle sceneHandle{};
  Status uploadStatus = renderer_.CreateUploadedScene(SceneFromSplats(splats), sceneHandle);
  if (!uploadStatus.ok) {
    return StatusOr<ImageRgba8>::Error(uploadStatus.message);
  }

  const CameraRenderState cameraState = CameraRenderStateFromCameraParams(camera, options.nearPlane, options.farPlane);

  RenderInput input{};
  input.view = cameraState.view;
  input.proj = cameraState.proj;
  input.cameraPosition = cameraState.position;
  input.viewportWidth = options.width;
  input.viewportHeight = options.height;
  input.nearPlane = options.nearPlane;
  input.farPlane = options.farPlane;
  input.settings.antialiasing = options.antialiasing;
  input.settings.antialiasingStrength = options.antialiasingStrength;
  input.settings.backgroundColor = {options.background[0], options.background[1], options.background[2]};
  input.settings.gammaCorrection = options.gammaCorrection;
  input.settings.renderType = options.renderType;
  input.settings.shadingDegree = options.shadingDegree;
  input.settings.outputDepth = options.renderType == RenderType::Depth;
  input.frameIndex = FrameContext().frameIndex;

  const RenderFrameContext frameContext = FrameContext();
  RenderPreparationResult preparation{};
  Status prepareStatus = renderer_.PrepareSceneForRender(sceneHandle, input, frameContext, &preparation);
  if (!prepareStatus.ok) {
    (void)renderer_.DestroyUploadedScene(sceneHandle);
    return StatusOr<ImageRgba8>::Error(prepareStatus.message);
  }

  Status resetStatus = ResetCommandList();
  if (!resetStatus.ok) {
    (void)renderer_.DestroyUploadedScene(sceneHandle);
    return StatusOr<ImageRgba8>::Error(resetStatus.message);
  }

  RenderResult renderResult{};
  RenderTargetBinding target = TargetBinding(options);
  Status renderStatus = renderer_.Render(commandList_.Get(), target, sceneHandle, input, frameContext, renderResult);
  if (renderStatus.ok || renderResult.submission.submissionRequired) {
    colorState_ = target.colorStateAfter;
  }
  if (renderStatus.ok) {
    QueueColorReadback();
  }

  Status executeStatus = ExecuteAndWait(renderResult.submission.uploadSyncPoint);
  Status destroyStatus = renderer_.DestroyUploadedScene(sceneHandle);
  if (!renderStatus.ok) {
    return StatusOr<ImageRgba8>::Error(renderStatus.message);
  }
  if (!executeStatus.ok) {
    return StatusOr<ImageRgba8>::Error(executeStatus.message);
  }
  if (!destroyStatus.ok) {
    return StatusOr<ImageRgba8>::Error(destroyStatus.message);
  }

  return ReadbackImage();
}

}  // namespace directxsplat
