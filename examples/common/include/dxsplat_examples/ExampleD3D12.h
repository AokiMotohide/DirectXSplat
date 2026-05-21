#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>

#include "appcommon/image.h"
#include "dxsplat/context.h"
#include "dxsplat/gpu_resources.h"
#include "dxsplat/status.h"

namespace dxsplat::examples {

struct OffscreenTarget {
  Microsoft::WRL::ComPtr<ID3D12Resource> color;
  Microsoft::WRL::ComPtr<ID3D12Resource> readback;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  uint64_t readbackSizeBytes = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
};

class D3D12ExampleDevice {
 public:
  D3D12ExampleDevice() = default;
  D3D12ExampleDevice(const D3D12ExampleDevice&) = delete;
  D3D12ExampleDevice& operator=(const D3D12ExampleDevice&) = delete;
  ~D3D12ExampleDevice();

  Status Initialize(bool forceWarp);
  void Shutdown();

  Status BeginCommands();
  Status FinishCommands(UploadSyncPoint uploadSyncPoint, bool executeCommandList);
  Status SignalFrame(UploadSyncPoint uploadSyncPoint);
  RenderFrameContext FrameContext() const;

  Status CreateOffscreenTarget(uint32_t width, uint32_t height, OffscreenTarget& outTarget);
  Status RecordReadback(const OffscreenTarget& target);
  Status ReadbackImage(const OffscreenTarget& target, appcommon::ImageRgba8& outImage);

  ID3D12Device* Device() const { return device_.Get(); }
  ID3D12CommandQueue* Queue() const { return queue_.Get(); }
  ID3D12GraphicsCommandList* CommandList() const { return commandList_.Get(); }
  ID3D12Fence* Fence() const { return fence_.Get(); }
  uint64_t CompletedFenceValue() const { return fence_ != nullptr ? fence_->GetCompletedValue() : 0; }
  uint64_t NextFenceValue() const { return fenceValue_ + 1; }

 private:
  Status WaitForFence(uint64_t value);

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

}  // namespace dxsplat::examples
