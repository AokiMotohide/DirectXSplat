#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>

#include "directxsplat/context.h"
#include "directxsplat/directxsplat.h"
#include "directxsplat/gpu_resources.h"
#include "directxsplat/renderer.h"
#include "directxsplat/types.h"

namespace directxsplat {

class OwnedD3D12Runtime {
 public:
  OwnedD3D12Runtime() = default;
  ~OwnedD3D12Runtime();

  OwnedD3D12Runtime(const OwnedD3D12Runtime&) = delete;
  OwnedD3D12Runtime& operator=(const OwnedD3D12Runtime&) = delete;

  StatusOr<ImageRgba8> Draw(const GaussianSplats& splats, const CameraParams& camera, const DrawOptions& options);

 private:
  Status EnsureInitialized();
  Status EnsureOffscreenTarget(uint32_t width, uint32_t height);
  Status ResetCommandList();
  Status ExecuteAndWait(UploadSyncPoint sync = {});
  RenderFrameContext FrameContext() const;
  RenderTargetBinding TargetBinding(const DrawOptions& options) const;
  void QueueColorReadback();
  StatusOr<ImageRgba8> ReadbackImage() const;

  Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  HANDLE fenceEvent_ = nullptr;
  uint64_t fenceValue_ = 0;

  D3D12Context context_;
  Renderer renderer_;
  bool initialized_ = false;

  Microsoft::WRL::ComPtr<ID3D12Resource> colorTarget_;
  Microsoft::WRL::ComPtr<ID3D12Resource> colorReadback_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv_{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
  uint64_t readbackBytes_ = 0;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  D3D12_RESOURCE_STATES colorState_ = D3D12_RESOURCE_STATE_COMMON;
};

}  // namespace directxsplat
