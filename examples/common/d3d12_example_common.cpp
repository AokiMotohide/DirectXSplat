#include "d3d12_example_common.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include <directxsplat/bounding.h>

namespace directxsplat_examples {
namespace {

std::string HrString(HRESULT hr) {
  std::ostringstream stream;
  stream << "0x" << std::hex << static_cast<unsigned long>(hr);
  return stream.str();
}

directxsplat::Status HrStatus(HRESULT hr, const char* message) {
  return directxsplat::Status::Error(std::string(message) + " " + HrString(hr));
}

directxsplat::Aabb MergeBounds(directxsplat::Aabb a, const directxsplat::Aabb& b) {
  if (!b.valid) {
    return a;
  }
  if (!a.valid) {
    return b;
  }
  a.min = directxsplat::Min(a.min, b.min);
  a.max = directxsplat::Max(a.max, b.max);
  return a;
}

}  // namespace

D3D12Host::~D3D12Host() { Shutdown(); }

directxsplat::Status D3D12Host::Initialize() {
  HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating DXGI factory");
  }

  for (UINT adapterIndex = 0; ; ++adapterIndex) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> candidate;
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
  hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating direct queue");
  }

  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating command allocator");
  }

  hr = device_->CreateCommandList(0,
                                  D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  allocator_.Get(),
                                  nullptr,
                                  IID_PPV_ARGS(commandList_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating command list");
  }
  hr = commandList_->Close();
  if (FAILED(hr)) {
    return HrStatus(hr, "failed closing initial command list");
  }

  hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating fence");
  }

  fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (fenceEvent_ == nullptr) {
    return directxsplat::Status::Error("failed creating fence event");
  }

  return directxsplat::Status::Ok();
}

void D3D12Host::Shutdown() {
  if (fence_ != nullptr) {
    (void)WaitForFenceValue(fenceValue_);
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

directxsplat::RenderFrameContext D3D12Host::FrameContext() const {
  directxsplat::RenderFrameContext frame{};
  frame.fence = fence_.Get();
  frame.completedFenceValue = fence_ != nullptr ? fence_->GetCompletedValue() : 0;
  frame.submissionFenceValue = fenceValue_ + 1;
  frame.frameIndex = fenceValue_;
  return frame;
}

directxsplat::Status D3D12Host::ResetCommandList() {
  HRESULT hr = allocator_->Reset();
  if (FAILED(hr)) {
    return HrStatus(hr, "failed resetting command allocator");
  }
  hr = commandList_->Reset(allocator_.Get(), nullptr);
  if (FAILED(hr)) {
    return HrStatus(hr, "failed resetting command list");
  }
  return directxsplat::Status::Ok();
}

directxsplat::Status D3D12Host::QueueWait(directxsplat::UploadSyncPoint sync) {
  if (!sync.IsValid()) {
    return directxsplat::Status::Ok();
  }
  const HRESULT hr = queue_->Wait(sync.fence, sync.value);
  return FAILED(hr) ? HrStatus(hr, "failed queue wait") : directxsplat::Status::Ok();
}

directxsplat::Status D3D12Host::ExecuteCommandList(directxsplat::UploadSyncPoint sync, uint64_t signalValue) {
  directxsplat::Status waitStatus = QueueWait(sync);
  if (!waitStatus.ok) {
    return waitStatus;
  }
  HRESULT hr = commandList_->Close();
  if (FAILED(hr)) {
    return HrStatus(hr, "failed closing command list");
  }
  ID3D12CommandList* lists[] = {commandList_.Get()};
  queue_->ExecuteCommandLists(1, lists);
  hr = queue_->Signal(fence_.Get(), signalValue);
  if (FAILED(hr)) {
    return HrStatus(hr, "failed signaling fence");
  }
  fenceValue_ = std::max(fenceValue_, signalValue);
  return directxsplat::Status::Ok();
}

directxsplat::Status D3D12Host::SignalFence(ID3D12Fence* fence, uint64_t value) {
  if (fence == nullptr || value == 0) {
    return directxsplat::Status::Error("fence signal requires a fence and value");
  }
  const HRESULT hr = queue_->Signal(fence, value);
  if (FAILED(hr)) {
    return HrStatus(hr, "failed signaling fence");
  }
  if (fence == fence_.Get()) {
    fenceValue_ = std::max(fenceValue_, value);
  }
  return directxsplat::Status::Ok();
}

directxsplat::Status D3D12Host::SignalFenceValue(uint64_t value) {
  return SignalFence(fence_.Get(), value);
}

directxsplat::Status D3D12Host::WaitForFenceValue(uint64_t value) {
  if (fence_ == nullptr || value == 0 || fence_->GetCompletedValue() >= value) {
    return directxsplat::Status::Ok();
  }
  HRESULT hr = fence_->SetEventOnCompletion(value, fenceEvent_);
  if (FAILED(hr)) {
    return HrStatus(hr, "failed waiting for fence");
  }
  WaitForSingleObject(fenceEvent_, INFINITE);
  return directxsplat::Status::Ok();
}

directxsplat::Status D3D12Host::CreateOffscreenTarget(uint32_t width,
                                                      uint32_t height,
                                                      D3D12_RESOURCE_STATES stateAfterRender,
                                                      OffscreenTarget& out) {
  if (width == 0 || height == 0) {
    return directxsplat::Status::Error("offscreen target dimensions must be positive");
  }

  out = {};
  out.width = width;
  out.height = height;

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
  clearValue.Color[0] = 0.0f;
  clearValue.Color[1] = 0.0f;
  clearValue.Color[2] = 0.0f;
  clearValue.Color[3] = 1.0f;

  HRESULT hr = device_->CreateCommittedResource(&heapProps,
                                                D3D12_HEAP_FLAG_NONE,
                                                &desc,
                                                D3D12_RESOURCE_STATE_COMMON,
                                                &clearValue,
                                                IID_PPV_ARGS(out.color.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating color target");
  }

  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 1;
  hr = device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(out.rtvHeap.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating RTV heap");
  }
  out.rtv = out.rtvHeap->GetCPUDescriptorHandleForHeapStart();
  device_->CreateRenderTargetView(out.color.Get(), nullptr, out.rtv);

  UINT64 totalBytes = 0;
  device_->GetCopyableFootprints(&desc, 0, 1, 0, &out.footprint, nullptr, nullptr, &totalBytes);
  out.readbackBytes = totalBytes;

  D3D12_HEAP_PROPERTIES readbackHeapProps{};
  readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK;
  readbackHeapProps.CreationNodeMask = 1;
  readbackHeapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC readbackDesc{};
  readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  readbackDesc.Width = totalBytes;
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
                                        IID_PPV_ARGS(out.readback.GetAddressOf()));
  if (FAILED(hr)) {
    return HrStatus(hr, "failed creating readback buffer");
  }

  out.binding.colorTarget = out.color.Get();
  out.binding.colorRtv = out.rtv;
  out.binding.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
  out.binding.colorStateBefore = D3D12_RESOURCE_STATE_COMMON;
  out.binding.colorStateAfter = stateAfterRender;
  out.binding.transitionMode = directxsplat::ResourceTransitionMode::LibraryManaged;
  out.binding.clearColor = true;
  out.binding.clearColorValue[0] = 0.0f;
  out.binding.clearColorValue[1] = 0.0f;
  out.binding.clearColorValue[2] = 0.0f;
  out.binding.clearColorValue[3] = 1.0f;
  out.binding.viewport.TopLeftX = 0.0f;
  out.binding.viewport.TopLeftY = 0.0f;
  out.binding.viewport.Width = static_cast<float>(width);
  out.binding.viewport.Height = static_cast<float>(height);
  out.binding.viewport.MinDepth = 0.0f;
  out.binding.viewport.MaxDepth = 1.0f;
  out.binding.scissor.left = 0;
  out.binding.scissor.top = 0;
  out.binding.scissor.right = static_cast<LONG>(width);
  out.binding.scissor.bottom = static_cast<LONG>(height);
  return directxsplat::Status::Ok();
}

void D3D12Host::QueueReadback(const OffscreenTarget& target) {
  D3D12_TEXTURE_COPY_LOCATION src{};
  src.pResource = target.color.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  src.SubresourceIndex = 0;

  D3D12_TEXTURE_COPY_LOCATION dst{};
  dst.pResource = target.readback.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dst.PlacedFootprint = target.footprint;

  commandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
}

directxsplat::StatusOr<std::vector<uint8_t>> D3D12Host::ReadbackRgba8(const OffscreenTarget& target) const {
  const uint64_t rowBytes = static_cast<uint64_t>(target.width) * 4ull;
  const uint64_t requiredBytes = target.height == 0 ? 0 : (static_cast<uint64_t>(target.height - 1) *
      target.footprint.Footprint.RowPitch + rowBytes);
  if (target.readbackBytes < requiredBytes) {
    return directxsplat::StatusOr<std::vector<uint8_t>>::Error("readback buffer is too small");
  }

  std::vector<uint8_t> pixels(static_cast<size_t>(rowBytes) * target.height, 0u);
  void* mapped = nullptr;
  const HRESULT hr = target.readback->Map(0, nullptr, &mapped);
  if (FAILED(hr) || mapped == nullptr) {
    return directxsplat::StatusOr<std::vector<uint8_t>>::Error("failed mapping readback");
  }

  const uint8_t* src = reinterpret_cast<const uint8_t*>(mapped);
  for (uint32_t y = 0; y < target.height; ++y) {
    const uint8_t* srcRow = src + static_cast<size_t>(y) * target.footprint.Footprint.RowPitch;
    uint8_t* dstRow = pixels.data() + static_cast<size_t>(y) * rowBytes;
    std::copy(srcRow, srcRow + rowBytes, dstRow);
  }
  target.readback->Unmap(0, nullptr);
  return directxsplat::StatusOr<std::vector<uint8_t>>::Ok(std::move(pixels));
}

SceneView EstimateSceneView(const directxsplat::Scene& scene) {
  directxsplat::Aabb bounds = scene.sceneBounds;
  for (const directxsplat::GaussianSet& set : scene.splatSets) {
    bounds = MergeBounds(bounds, set.bounds);
  }
  if (!bounds.valid) {
    return {{0.0f, 0.0f, 2.5f}, 1.0f};
  }
  return {directxsplat::ComputeAabbCenter(bounds), std::max(directxsplat::ComputeAabbRadius(bounds), 0.5f)};
}

directxsplat::RenderInput MakeRenderInput(const directxsplat::Scene& scene,
                                          uint32_t width,
                                          uint32_t height,
                                          uint64_t frameIndex) {
  const SceneView view = EstimateSceneView(scene);
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const float distance = std::max(view.radius * 2.5f, 1.5f);
  const directxsplat::Vec3 eye{view.center.x, view.center.y, view.center.z - distance};

  directxsplat::RenderInput input{};
  input.view = directxsplat::LookAt(eye, view.center, {0.0f, 1.0f, 0.0f});
  input.proj = directxsplat::Perspective(1.0f, aspect, 0.01f, std::max(100.0f, distance + view.radius * 4.0f));
  input.cameraPosition = eye;
  input.viewportWidth = width;
  input.viewportHeight = height;
  input.nearPlane = 0.01f;
  input.farPlane = std::max(100.0f, distance + view.radius * 4.0f);
  input.frameIndex = frameIndex;
  input.settings.antialiasing = true;
  input.settings.fastCulling = true;
  return input;
}

directxsplat::Status WritePpm(const std::filesystem::path& path,
                              const std::vector<uint8_t>& rgba,
                              uint32_t width,
                              uint32_t height) {
  if (rgba.size() < static_cast<size_t>(width) * height * 4u) {
    return directxsplat::Status::Error("image buffer is too small");
  }

  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return directxsplat::Status::Error("failed opening output image");
  }
  file << "P6\n" << width << " " << height << "\n255\n";
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const size_t offset = (static_cast<size_t>(y) * width + x) * 4u;
      file.put(static_cast<char>(rgba[offset + 0u]));
      file.put(static_cast<char>(rgba[offset + 1u]));
      file.put(static_cast<char>(rgba[offset + 2u]));
    }
  }
  return directxsplat::Status::Ok();
}

int PrintError(const char* operation, const directxsplat::Status& status) {
  std::cerr << operation << ": " << status.message << "\n";
  return 1;
}

}  // namespace directxsplat_examples
