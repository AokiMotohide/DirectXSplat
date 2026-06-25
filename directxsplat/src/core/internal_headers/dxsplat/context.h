#pragma once

#include <d3d12.h>

#include "dxsplat/status.h"

namespace directxsplat {

class D3D12Context {
 public:
  Status Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Fence* submissionFence);
  Status Initialize(ID3D12Device* device,
                    ID3D12CommandQueue* queue,
                    ID3D12Fence* submissionFence,
                    ID3D12CommandQueue* copyQueue,
                    ID3D12Fence* uploadFence);
  void Shutdown();
  void Reset();

  ID3D12Device* Device() const;
  ID3D12CommandQueue* CommandQueue() const;
  ID3D12Fence* SubmissionFence() const;
  ID3D12CommandQueue* CopyQueue() const;
  ID3D12Fence* UploadFence() const;

 private:
  ID3D12Device* device_ = nullptr;
  ID3D12CommandQueue* queue_ = nullptr;
  ID3D12Fence* submissionFence_ = nullptr;
  ID3D12CommandQueue* copyQueue_ = nullptr;
  ID3D12Fence* uploadFence_ = nullptr;
};

}  // namespace directxsplat
