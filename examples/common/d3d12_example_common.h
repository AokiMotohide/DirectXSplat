#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <vector>

#include <directxsplat/gpu_resources.h>
#include <directxsplat/math.h>
#include <directxsplat/scene.h>
#include <directxsplat/settings.h>
#include <directxsplat/status.h>
#include <directxsplat/types.h>

namespace directxsplat_examples {

struct OffscreenTarget {
  Microsoft::WRL::ComPtr<ID3D12Resource> color;
  Microsoft::WRL::ComPtr<ID3D12Resource> readback;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  uint64_t readbackBytes = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  directxsplat::RenderTargetBinding binding{};
};

struct SceneView {
  directxsplat::Vec3 center{};
  float radius = 1.0f;
};

class D3D12Host {
 public:
  D3D12Host() = default;
  ~D3D12Host();
  D3D12Host(const D3D12Host&) = delete;
  D3D12Host& operator=(const D3D12Host&) = delete;

  directxsplat::Status Initialize();
  void Shutdown();

  ID3D12Device* Device() const { return device_.Get(); }
  ID3D12CommandQueue* Queue() const { return queue_.Get(); }
  ID3D12Fence* Fence() const { return fence_.Get(); }
  ID3D12GraphicsCommandList* CommandList() const { return commandList_.Get(); }

  directxsplat::RenderFrameContext FrameContext() const;
  directxsplat::Status ResetCommandList();
  directxsplat::Status QueueWait(directxsplat::UploadSyncPoint sync);
  directxsplat::Status ExecuteCommandList(directxsplat::UploadSyncPoint sync, uint64_t signalValue);
  directxsplat::Status SignalFence(ID3D12Fence* fence, uint64_t value);
  directxsplat::Status SignalFenceValue(uint64_t value);
  directxsplat::Status WaitForFenceValue(uint64_t value);

  directxsplat::Status CreateOffscreenTarget(uint32_t width,
                                             uint32_t height,
                                             D3D12_RESOURCE_STATES stateAfterRender,
                                             OffscreenTarget& out);
  void QueueReadback(const OffscreenTarget& target);
  directxsplat::StatusOr<std::vector<uint8_t>> ReadbackRgba8(const OffscreenTarget& target) const;

 private:
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  HANDLE fenceEvent_ = nullptr;
  uint64_t fenceValue_ = 0;
};

SceneView EstimateSceneView(const directxsplat::Scene& scene);
directxsplat::RenderInput MakeRenderInput(const directxsplat::Scene& scene,
                                          uint32_t width,
                                          uint32_t height,
                                          uint64_t frameIndex);
directxsplat::Status WritePpm(const std::filesystem::path& path,
                              const std::vector<uint8_t>& rgba,
                              uint32_t width,
                              uint32_t height);
int PrintError(const char* operation, const directxsplat::Status& status);

}  // namespace directxsplat_examples
