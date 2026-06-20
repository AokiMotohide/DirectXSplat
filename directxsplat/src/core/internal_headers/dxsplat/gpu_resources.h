#pragma once

#include <d3d12.h>
#include <dxgiformat.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dxsplat/sort.h"
#include "dxsplat/types.h"
#include "dxsplat/vram_format.h"

namespace dxsplat {

enum class GpuResourceAccess {
  ReadOnly,
  ReadWrite,
};

enum class GpuViewLifetime {
  CurrentRenderCall,
  UploadedSceneLifetime,
};

enum class ResourceTransitionMode {
  LibraryManaged,
  CallerManaged,
};

enum class DepthOutputKind {
  None,
  ApproximateSplatDepth,
};

enum class UploadedSceneBufferFormat {
  Unknown,
  CompactPackedVariable,
};

struct GpuBufferView {
  ID3D12Resource* resource = nullptr;
  uint64_t gpuVirtualAddress = 0;
  uint64_t sizeBytes = 0;
  uint32_t strideBytes = 0;
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  GpuResourceAccess access = GpuResourceAccess::ReadOnly;
  GpuViewLifetime lifetime = GpuViewLifetime::CurrentRenderCall;
  bool callerMayTransition = true;
  bool callerMayWrite = false;
  constexpr bool IsValid() const { return resource != nullptr && sizeBytes > 0; }
};

struct GpuTextureView {
  ID3D12Resource* resource = nullptr;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t mipLevels = 1;
  D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
  D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
  D3D12_GPU_DESCRIPTOR_HANDLE srv{};
  D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
  GpuResourceAccess access = GpuResourceAccess::ReadOnly;
  GpuViewLifetime lifetime = GpuViewLifetime::CurrentRenderCall;
  bool callerMayTransition = true;
  bool callerMayWrite = false;
  constexpr bool IsValid() const { return resource != nullptr; }
};

struct RenderTargetBinding {
  ID3D12Resource* colorTarget = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE colorRtv{};
  D3D12_GPU_DESCRIPTOR_HANDLE colorSrv{};
  DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
  D3D12_RESOURCE_STATES colorStateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  D3D12_RESOURCE_STATES colorStateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

  ID3D12Resource* depthTarget = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE depthDsv{};
  D3D12_GPU_DESCRIPTOR_HANDLE depthSrv{};
  DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
  D3D12_RESOURCE_STATES depthStateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
  D3D12_RESOURCE_STATES depthStateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

  ID3D12Resource* motionVectorsTarget = nullptr;
  D3D12_CPU_DESCRIPTOR_HANDLE motionVectorsRtv{};
  D3D12_GPU_DESCRIPTOR_HANDLE motionVectorsSrv{};
  DXGI_FORMAT motionVectorsFormat = DXGI_FORMAT_UNKNOWN;
  D3D12_RESOURCE_STATES motionVectorsStateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  D3D12_RESOURCE_STATES motionVectorsStateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

  ResourceTransitionMode transitionMode = ResourceTransitionMode::LibraryManaged;
  D3D12_VIEWPORT viewport{};
  D3D12_RECT scissor{};
  bool clearColor = false;
  float clearColorValue[4]{0.0f, 0.0f, 0.0f, 1.0f};
  bool clearDepth = false;
  float clearDepthValue = 1.0f;
  uint8_t clearStencilValue = 0;
  bool clearMotionVectors = false;
  float clearMotionVectorsValue[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

struct RenderFrameContext {
  ID3D12Fence* fence = nullptr;
  uint64_t completedFenceValue = 0;
  uint64_t submissionFenceValue = 0;
  uint64_t frameIndex = 0;
  constexpr bool HasCompletedFence() const { return completedFenceValue != 0; }
  constexpr bool HasSubmissionFence() const { return submissionFenceValue != 0; }
  constexpr bool HasFence() const { return fence != nullptr; }
};

struct RenderSubmissionInfo {
  bool submissionRequired = false;
  UploadSyncPoint uploadSyncPoint{};
};

struct GpuFrameResources {
  GpuTextureView colorTarget{};
  GpuTextureView depthTarget{};
  GpuTextureView motionVectorsTarget{};
  GpuBufferView sceneGaussians{};
  VramFormatSettings vramFormat{};
  uint32_t packedStrideBytes = 0;
  GpuBufferView sceneIndexToChunk{};
  GpuBufferView sortedSceneIndices{};
  GpuBufferView secondarySortedSceneIndices{};
  GpuBufferView visibleCounter{};
  GpuBufferView drawArgs{};
  GpuBufferView sortMeta{};
  uint32_t visibleSplatCount = 0;
  uint32_t emittedSortEntryCount = 0;
  uint32_t drawInstanceCount = 0;
  SortBackend sortBackend = SortBackend::OneSweep;
  DepthOutputKind depthOutput = DepthOutputKind::None;
  bool colorValid = false;
  bool depthValid = false;
  bool motionVectorsValid = false;
};

struct UploadedChunkInfo {
  UploadedChunkHandle handle{};
  uint32_t gaussianCount = 0;
  Aabb bounds{};
  bool visible = false;
  bool resident = false;
  int residentLod = -1;
  float scalingModifier = 1.0f;
};

struct UploadedSceneInfo {
  UploadedSceneHandle handle{};
  uint32_t chunkCount = 0;
  uint64_t gaussianCount = 0;
  uint64_t residentGaussians = 0;
  uint64_t residentChunks = 0;
  Aabb bounds{};
  bool readyToRender = false;
};

struct UploadedChunkGpuResources {
  UploadedChunkHandle handle{};
  GpuBufferView gaussianData{};
  uint32_t gaussianCount = 0;
  uint32_t packedStrideBytes = 0;
  UploadedSceneBufferFormat format = UploadedSceneBufferFormat::Unknown;
  VramFormatSettings vramFormat{};
  float decodeMin[3]{};
  float decodeExtent[3]{};
  bool visible = false;
  int residentLod = -1;
};

struct UploadedSceneGpuResources {
  UploadedSceneHandle scene{};
  ID3D12Fence* leaseFence = nullptr;
  uint64_t leaseFenceValue = 0;
  RenderSubmissionInfo submission{};
  std::vector<UploadedChunkGpuResources> chunks;
  GpuBufferView sceneGaussians{};
  VramFormatSettings vramFormat{};
  uint32_t packedStrideBytes = 0;
  GpuBufferView sceneIndexToChunk{};
  GpuBufferView sortedSceneIndices{};
  GpuBufferView secondarySortedSceneIndices{};
  GpuBufferView visibleCounter{};
  GpuBufferView drawArgs{};
  GpuBufferView sortMeta{};
};

}  // namespace dxsplat
