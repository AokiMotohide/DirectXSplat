#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <array>
#include <cstdint>
#include <string>

#include <wrl/client.h>

#include "dxsplat/status.h"

namespace appcommon {

class SwapchainContext {
 public:
  static constexpr uint32_t kFrameCount = 2;

  struct Frame {
    Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    uint64_t fenceValue = 0;
  };

  dxsplat::Status Initialize(HWND hwnd, uint32_t width, uint32_t height, bool enableDebugLayer);
  dxsplat::Status Shutdown();

  dxsplat::Status Resize(uint32_t width, uint32_t height);

  dxsplat::Status BeginFrame(bool waitForFrameLatency = false);
  dxsplat::Status EndFrame(bool vsync);
  dxsplat::Status WaitForGpu();
  void NotifyQueueLost();

  ID3D12Device* Device() const;
  ID3D12GraphicsCommandList* CommandList() const;
  ID3D12CommandQueue* CommandQueue() const;
  ID3D12Fence* Fence() const;
  IDXGISwapChain3* Swapchain() const;
  const std::string& AdapterName() const;

  uint32_t Width() const;
  uint32_t Height() const;
  uint32_t FrameIndex() const;
  uint64_t CompletedFenceValue() const;
  uint64_t PendingSubmissionFenceValue() const;

  D3D12_VIEWPORT Viewport() const;
  D3D12_RECT ScissorRect() const;
  D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;
  ID3D12Resource* CurrentBackBuffer() const;

  ID3D12DescriptorHeap* ImGuiSrvHeap() const;

 private:
  dxsplat::Status CreateDeviceAndSwapchain(bool enableDebugLayer);
  dxsplat::Status CreateFrameResources();
  dxsplat::Status CreateImGuiHeap();
  dxsplat::Status SignalFrame(Frame& frame);
  dxsplat::Status WaitForFrameLatency();
  dxsplat::Status WaitForFenceValue(uint64_t value);
  dxsplat::Status CheckDeviceRemoved();
  std::string DebugMessages() const;

  HWND hwnd_ = nullptr;
  uint32_t width_ = 1;
  uint32_t height_ = 1;

  Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
  std::string adapterName_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain_;
  HANDLE frameLatencyWaitableObject_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  UINT rtvDescriptorSize_ = 0;

  std::array<Frame, kFrameCount> frames_{};

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  HANDLE fenceEvent_ = nullptr;
  uint64_t fenceValue_ = 0;
  bool queueLost_ = false;
  bool allowTearing_ = false;

  uint32_t frameIndex_ = 0;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> imguiSrvHeap_;
};

}  // namespace appcommon
