#include "appcommon/swapchain_context.h"

#include <algorithm>
#include <sstream>

#include <d3d12sdklayers.h>

namespace appcommon {

namespace {

using Microsoft::WRL::ComPtr;

void SetName(ID3D12Object* obj, const wchar_t* name) {
  if (obj != nullptr) {
    obj->SetName(name);
  }
}

std::string HrString(HRESULT hr) {
  std::ostringstream ss;
  ss << "0x" << std::hex << static_cast<unsigned long>(hr);
  return ss.str();
}

std::string WideToUtf8(const wchar_t* text) {
  if (text == nullptr || text[0] == L'\0') {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) {
    return {};
  }
  std::string out(static_cast<size_t>(size - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
  return out;
}

bool IsDeviceRemovalFailure(HRESULT hr) {
  return hr == DXGI_ERROR_DEVICE_REMOVED ||
         hr == DXGI_ERROR_DEVICE_RESET ||
         hr == DXGI_ERROR_DEVICE_HUNG ||
         hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

}  

dxsplat::Status SwapchainContext::Initialize(HWND hwnd, uint32_t width, uint32_t height, bool enableDebugLayer) {
  hwnd_ = hwnd;
  width_ = std::max(width, 1u);
  height_ = std::max(height, 1u);
  queueLost_ = false;
  auto fail = [&](dxsplat::Status status) {
    (void)Shutdown();
    return status;
  };

  dxsplat::Status status = CreateDeviceAndSwapchain(enableDebugLayer);
  if (!status.ok) {
    return fail(status);
  }

  status = CreateFrameResources();
  if (!status.ok) {
    return fail(status);
  }

  status = CreateImGuiHeap();
  if (!status.ok) {
    return fail(status);
  }

  HRESULT hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
  if (FAILED(hr)) {
    return fail(dxsplat::Status::Error("CreateFence failed"));
  }
  fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (fenceEvent_ == nullptr) {
    return fail(dxsplat::Status::Error("CreateEvent failed"));
  }

  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::Shutdown() {
  dxsplat::Status idle = WaitForGpu();
  if (fenceEvent_ != nullptr) {
    CloseHandle(fenceEvent_);
    fenceEvent_ = nullptr;
  }
  frames_ = {};
  imguiSrvHeap_.Reset();
  rtvHeap_.Reset();
  commandList_.Reset();
  fence_.Reset();
  swapchain_.Reset();
  frameLatencyWaitableObject_ = nullptr;
  queue_.Reset();
  device_.Reset();
  adapter_.Reset();
  adapterName_.clear();
  factory_.Reset();
  rtvDescriptorSize_ = 0;
  fenceValue_ = 0;
  frameIndex_ = 0;
  queueLost_ = false;
  return idle;
}

dxsplat::Status SwapchainContext::CreateDeviceAndSwapchain(bool enableDebugLayer) {
  bool debugLayerEnabled = false;
  if (enableDebugLayer) {
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf())))) {
      debug->EnableDebugLayer();
      debugLayerEnabled = true;
    }
  }

  UINT factoryFlags = debugLayerEnabled ? DXGI_CREATE_FACTORY_DEBUG : 0u;
  HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory_.GetAddressOf()));
  if (FAILED(hr) && factoryFlags != 0u) {
    hr = CreateDXGIFactory2(0u, IID_PPV_ARGS(factory_.GetAddressOf()));
  }
  if (FAILED(hr)) {
    ComPtr<IDXGIFactory1> factory1;
    hr = CreateDXGIFactory1(IID_PPV_ARGS(factory1.GetAddressOf()));
    if (SUCCEEDED(hr)) {
      hr = factory1.As(&factory_);
    }
  }
  if (FAILED(hr)) {
    return dxsplat::Status::Error("CreateDXGIFactory2 failed");
  }

  for (UINT index = 0;; ++index) {
    ComPtr<IDXGIAdapter1> candidate;
    if (factory_->EnumAdapters1(index, candidate.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    DXGI_ADAPTER_DESC1 desc{};
    candidate->GetDesc1(&desc);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
      continue;
    }
    if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(device_.GetAddressOf())))) {
      adapter_ = candidate;
      adapterName_ = WideToUtf8(desc.Description);
      break;
    }
  }

  if (device_ == nullptr) {
    ComPtr<IDXGIAdapter1> warp;
    hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf()));
    if (FAILED(hr)) {
      return dxsplat::Status::Error("failed to get WARP adapter");
    }
    DXGI_ADAPTER_DESC1 desc{};
    warp->GetDesc1(&desc);
    hr = D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(device_.GetAddressOf()));
    if (FAILED(hr)) {
      return dxsplat::Status::Error("D3D12CreateDevice failed");
    }
    adapter_ = warp;
    adapterName_ = WideToUtf8(desc.Description);
  }

  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue_.GetAddressOf()));
  if (FAILED(hr)) {
    return dxsplat::Status::Error("CreateCommandQueue failed");
  }

  DXGI_SWAP_CHAIN_DESC1 swapDesc{};
  swapDesc.Width = width_;
  swapDesc.Height = height_;
  swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.BufferCount = kFrameCount;
  swapDesc.SampleDesc.Count = 1;
  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

  ComPtr<IDXGISwapChain1> swapchain1;
  hr = factory_->CreateSwapChainForHwnd(queue_.Get(), hwnd_, &swapDesc, nullptr, nullptr, swapchain1.GetAddressOf());
  if (FAILED(hr)) {
    return dxsplat::Status::Error("CreateSwapChainForHwnd failed");
  }

  hr = swapchain1.As(&swapchain_);
  if (FAILED(hr)) {
    return dxsplat::Status::Error("swapchain cast failed");
  }

  frameIndex_ = swapchain_->GetCurrentBackBufferIndex();
  hr = swapchain_->SetMaximumFrameLatency(1);
  if (FAILED(hr)) {
    return dxsplat::Status::Error("SetMaximumFrameLatency failed");
  }
  frameLatencyWaitableObject_ = swapchain_->GetFrameLatencyWaitableObject();
  if (frameLatencyWaitableObject_ == nullptr) {
    return dxsplat::Status::Error("GetFrameLatencyWaitableObject failed");
  }

  D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
  rtvDesc.NumDescriptors = kFrameCount;
  rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  hr = device_->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(rtvHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    return dxsplat::Status::Error("CreateDescriptorHeap RTV failed");
  }

  rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  ComPtr<ID3D12CommandAllocator> bootstrapAllocator;
  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(bootstrapAllocator.GetAddressOf()));
  if (FAILED(hr)) {
    return dxsplat::Status::Error("CreateCommandAllocator bootstrap failed");
  }

  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, bootstrapAllocator.Get(), nullptr,
                                  IID_PPV_ARGS(commandList_.GetAddressOf()));
  if (FAILED(hr)) {
    return dxsplat::Status::Error("CreateCommandList failed");
  }
  hr = commandList_->Close();
  if (FAILED(hr)) {
    return dxsplat::Status::Error("command list close failed");
  }

  factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);

  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::CreateFrameResources() {
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

  for (uint32_t i = 0; i < kFrameCount; ++i) {
    auto& frame = frames_[i];
    HRESULT hr = swapchain_->GetBuffer(i, IID_PPV_ARGS(frame.backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
      return dxsplat::Status::Error("GetBuffer failed");
    }
    device_->CreateRenderTargetView(frame.backBuffer.Get(), nullptr, rtvHandle);
    frame.rtv = rtvHandle;
    frame.fenceValue = 0;
    SetName(frame.backBuffer.Get(), L"BackBuffer");

    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frame.allocator.GetAddressOf()));
    if (FAILED(hr)) {
      return dxsplat::Status::Error("CreateCommandAllocator failed");
    }

    rtvHandle.ptr += rtvDescriptorSize_;
  }

  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::CreateImGuiHeap() {
  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.NumDescriptors = 2048;
  desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(imguiSrvHeap_.GetAddressOf()));
  if (FAILED(hr)) {
    return dxsplat::Status::Error("CreateDescriptorHeap imgui failed");
  }
  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::Resize(uint32_t width, uint32_t height) {
  if (device_ == nullptr || swapchain_ == nullptr) {
    return dxsplat::Status::Error("swapchain is not initialized");
  }
  width_ = std::max(width, 1u);
  height_ = std::max(height, 1u);
  dxsplat::Status idle = WaitForGpu();
  if (!idle.ok) {
    return idle;
  }

  for (auto& frame : frames_) {
    frame.backBuffer.Reset();
    frame.allocator.Reset();
  }

  DXGI_SWAP_CHAIN_DESC desc{};
  swapchain_->GetDesc(&desc);
  HRESULT hr = swapchain_->ResizeBuffers(kFrameCount, width_, height_, desc.BufferDesc.Format, desc.Flags);
  if (FAILED(hr)) {
    queueLost_ = true;
    return dxsplat::Status::Error("ResizeBuffers failed");
  }

  frameIndex_ = swapchain_->GetCurrentBackBufferIndex();
  dxsplat::Status created = CreateFrameResources();
  if (!created.ok) {
    queueLost_ = true;
  }
  return created;
}

dxsplat::Status SwapchainContext::BeginFrame(bool waitForFrameLatency) {
  if (queueLost_) {
    return dxsplat::Status::Error("direct queue is lost");
  }
  if (swapchain_ == nullptr || commandList_ == nullptr || fence_ == nullptr || fenceEvent_ == nullptr) {
    return dxsplat::Status::Error("swapchain is not initialized");
  }
  if (waitForFrameLatency) {
    dxsplat::Status latency = WaitForFrameLatency();
    if (!latency.ok) {
      return latency;
    }
  }
  frameIndex_ = swapchain_->GetCurrentBackBufferIndex();
  Frame& frame = frames_[frameIndex_];
  if (frame.allocator == nullptr || frame.backBuffer == nullptr) {
    queueLost_ = true;
    return dxsplat::Status::Error("swapchain frame resources are invalid");
  }

  if (frame.fenceValue != 0 && fence_->GetCompletedValue() < frame.fenceValue) {
    HRESULT hr = fence_->SetEventOnCompletion(frame.fenceValue, fenceEvent_);
    if (FAILED(hr)) {
      queueLost_ = true;
      return dxsplat::Status::Error("SetEventOnCompletion failed " + HrString(hr) + DebugMessages());
    }
    if (WaitForSingleObject(fenceEvent_, INFINITE) != WAIT_OBJECT_0) {
      queueLost_ = true;
      return dxsplat::Status::Error("WaitForSingleObject failed");
    }
  }
  frame.fenceValue = 0;

  HRESULT hr = frame.allocator->Reset();
  if (FAILED(hr)) {
    return dxsplat::Status::Error("allocator reset failed");
  }

  hr = commandList_->Reset(frame.allocator.Get(), nullptr);
  if (FAILED(hr)) {
    return dxsplat::Status::Error("command list reset failed");
  }

  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = frame.backBuffer.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  commandList_->ResourceBarrier(1, &barrier);

  D3D12_VIEWPORT vp{};
  vp.Width = static_cast<float>(width_);
  vp.Height = static_cast<float>(height_);
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  D3D12_RECT sc{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
  commandList_->RSSetViewports(1, &vp);
  commandList_->RSSetScissorRects(1, &sc);

  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::EndFrame(bool vsync) {
  if (queueLost_) {
    return dxsplat::Status::Error("direct queue is lost");
  }
  if (swapchain_ == nullptr || queue_ == nullptr || commandList_ == nullptr || fence_ == nullptr) {
    return dxsplat::Status::Error("swapchain is not initialized");
  }
  Frame& frame = frames_[frameIndex_];

  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = frame.backBuffer.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  commandList_->ResourceBarrier(1, &barrier);

  HRESULT hr = commandList_->Close();
  if (FAILED(hr)) {
    const std::string closeError = "command list close failed " + HrString(hr) + DebugMessages();
    dxsplat::Status signaled = SignalFrame(frame);
    if (!signaled.ok) {
      return signaled;
    }
    return dxsplat::Status::Error(closeError);
  }

  ID3D12CommandList* lists[] = {commandList_.Get()};
  queue_->ExecuteCommandLists(1, lists);

  dxsplat::Status signaled = SignalFrame(frame);
  if (!signaled.ok) {
    return signaled;
  }

  hr = swapchain_->Present(vsync ? 1 : 0, 0);
  if (FAILED(hr)) {
    HRESULT removed = device_ != nullptr ? device_->GetDeviceRemovedReason() : S_OK;
    if (IsDeviceRemovalFailure(hr) || IsDeviceRemovalFailure(removed)) {
      queueLost_ = true;
    }
    return dxsplat::Status::Error("Present failed " + HrString(hr) + " removed=" + HrString(removed) + DebugMessages());
  }

  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::SignalFrame(Frame& frame) {
  const uint64_t signal = fenceValue_ + 1;
  HRESULT hr = queue_->Signal(fence_.Get(), signal);
  if (FAILED(hr)) {
    queueLost_ = true;
    return dxsplat::Status::Error("queue signal failed " + HrString(hr) + DebugMessages());
  }
  fenceValue_ = signal;
  frame.fenceValue = signal;
  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::WaitForFrameLatency() {
  if (frameLatencyWaitableObject_ == nullptr) {
    return dxsplat::Status::Error("frame latency waitable object is not initialized");
  }
  const DWORD wait = WaitForSingleObject(frameLatencyWaitableObject_, INFINITE);
  if (wait != WAIT_OBJECT_0) {
    queueLost_ = true;
    return dxsplat::Status::Error("frame latency wait failed");
  }
  return dxsplat::Status::Ok();
}

dxsplat::Status SwapchainContext::WaitForGpu() {
  if (queueLost_) {
    return dxsplat::Status::Error("direct queue is lost");
  }
  if (queue_ == nullptr || fence_ == nullptr || fenceEvent_ == nullptr) {
    return dxsplat::Status::Ok();
  }
  const uint64_t signal = fenceValue_ + 1;
  HRESULT hr = queue_->Signal(fence_.Get(), signal);
  if (FAILED(hr)) {
    queueLost_ = true;
    return dxsplat::Status::Error("queue signal failed " + HrString(hr) + DebugMessages());
  }
  fenceValue_ = signal;
  if (fence_->GetCompletedValue() < signal) {
    hr = fence_->SetEventOnCompletion(signal, fenceEvent_);
    if (FAILED(hr)) {
      queueLost_ = true;
      return dxsplat::Status::Error("SetEventOnCompletion failed " + HrString(hr) + DebugMessages());
    }
    if (WaitForSingleObject(fenceEvent_, INFINITE) != WAIT_OBJECT_0) {
      queueLost_ = true;
      return dxsplat::Status::Error("WaitForSingleObject failed");
    }
  }
  for (Frame& frame : frames_) {
    frame.fenceValue = 0;
  }
  return dxsplat::Status::Ok();
}

void SwapchainContext::NotifyQueueLost() {
  queueLost_ = true;
}

ID3D12Device* SwapchainContext::Device() const { return device_.Get(); }
ID3D12GraphicsCommandList* SwapchainContext::CommandList() const { return commandList_.Get(); }
ID3D12CommandQueue* SwapchainContext::CommandQueue() const { return queue_.Get(); }
ID3D12Fence* SwapchainContext::Fence() const { return fence_.Get(); }
IDXGISwapChain3* SwapchainContext::Swapchain() const { return swapchain_.Get(); }
const std::string& SwapchainContext::AdapterName() const { return adapterName_; }
uint32_t SwapchainContext::Width() const { return width_; }
uint32_t SwapchainContext::Height() const { return height_; }
uint32_t SwapchainContext::FrameIndex() const { return frameIndex_; }
uint64_t SwapchainContext::CompletedFenceValue() const { return fence_ != nullptr ? fence_->GetCompletedValue() : 0; }
uint64_t SwapchainContext::PendingSubmissionFenceValue() const {
  return queueLost_ || queue_ == nullptr || fence_ == nullptr ? 0 : fenceValue_ + 1;
}

D3D12_VIEWPORT SwapchainContext::Viewport() const {
  D3D12_VIEWPORT vp{};
  vp.Width = static_cast<float>(width_);
  vp.Height = static_cast<float>(height_);
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  return vp;
}

D3D12_RECT SwapchainContext::ScissorRect() const {
  return {0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapchainContext::CurrentRtv() const { return frames_[frameIndex_].rtv; }
ID3D12Resource* SwapchainContext::CurrentBackBuffer() const { return frames_[frameIndex_].backBuffer.Get(); }
ID3D12DescriptorHeap* SwapchainContext::ImGuiSrvHeap() const { return imguiSrvHeap_.Get(); }

std::string SwapchainContext::DebugMessages() const {
  if (device_ == nullptr) {
    return {};
  }
  ComPtr<ID3D12InfoQueue> infoQueue;
  if (FAILED(device_.As(&infoQueue)) || infoQueue == nullptr) {
    return {};
  }
  const UINT64 count = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
  if (count == 0) {
    return {};
  }
  const UINT64 begin = count > 8 ? count - 8 : 0;
  std::string out;
  for (UINT64 i = begin; i < count; ++i) {
    SIZE_T bytes = 0;
    if (FAILED(infoQueue->GetMessage(i, nullptr, &bytes)) || bytes == 0) {
      continue;
    }
    std::string storage(bytes, '\0');
    auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
    if (FAILED(infoQueue->GetMessage(i, message, &bytes)) || message->pDescription == nullptr) {
      continue;
    }
    out += "\n";
    out += message->pDescription;
  }
  return out;
}

}  // namespace appcommon
