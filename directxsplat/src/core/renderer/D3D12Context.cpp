#include "renderer/D3D12Context.h"

namespace directxsplat {

Status D3D12Context::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Fence* submissionFence) {
  return Initialize(device, queue, submissionFence, nullptr, nullptr);
}

Status D3D12Context::Initialize(ID3D12Device* device,
                                ID3D12CommandQueue* queue,
                                ID3D12Fence* submissionFence,
                                ID3D12CommandQueue* copyQueue,
                                ID3D12Fence* uploadFence) {
  if (device == nullptr || queue == nullptr || submissionFence == nullptr) {
    return Status::Error("invalid D3D12 device/queue/fence");
  }
  if ((copyQueue == nullptr) != (uploadFence == nullptr)) {
    return Status::Error("copy queue and upload fence must both be provided");
  }
  device_ = device;
  queue_ = queue;
  submissionFence_ = submissionFence;
  copyQueue_ = copyQueue;
  uploadFence_ = uploadFence;
  return Status::Ok();
}

void D3D12Context::Reset() {
  device_ = nullptr;
  queue_ = nullptr;
  submissionFence_ = nullptr;
  copyQueue_ = nullptr;
  uploadFence_ = nullptr;
}

void D3D12Context::Shutdown() {
  Reset();
}

ID3D12Device* D3D12Context::Device() const { return device_; }
ID3D12CommandQueue* D3D12Context::CommandQueue() const { return queue_; }
ID3D12Fence* D3D12Context::SubmissionFence() const { return submissionFence_; }
ID3D12CommandQueue* D3D12Context::CopyQueue() const { return copyQueue_; }
ID3D12Fence* D3D12Context::UploadFence() const { return uploadFence_; }

}  // namespace directxsplat
