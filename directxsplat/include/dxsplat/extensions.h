#pragma once

#include <d3d12.h>

#include <cstdint>

#include "dxsplat/gpu_resources.h"
#include "dxsplat/render_hooks.h"
#include "dxsplat/settings.h"
#include "dxsplat/sort.h"
#include "dxsplat/status.h"
#include "dxsplat/upscaling.h"

namespace dxsplat {

enum class SortBufferSelection {
  Primary,
  Secondary,
};

enum class SortOutputFormat {
  SortedValuesU32,
};

struct SortDispatchContext {
  ID3D12GraphicsCommandList* commandList = nullptr;
  UploadedSceneHandle scene{};
  const RenderInput* input = nullptr;
  GpuBufferView primaryKeys{};
  GpuBufferView secondaryKeys{};
  GpuBufferView primaryValues{};
  GpuBufferView secondaryValues{};
  GpuBufferView visibleCounter{};
  GpuBufferView sortMeta{};
  GpuBufferView drawArgs{};
  uint32_t sortEntryCapacity = 0;
  uint32_t maxVisibleBlocks = 0;
  uint32_t radixBits = 0;
};

struct SortDispatchResult {
  SortBufferSelection sortedBuffer = SortBufferSelection::Primary;
  SortBackend backend = SortBackend::OneSweep;
  uint32_t passCount = 0;
  uint32_t emittedSortEntryCount = 0;
  SortOutputFormat outputFormat = SortOutputFormat::SortedValuesU32;
  D3D12_RESOURCE_STATES primaryKeysState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  D3D12_RESOURCE_STATES secondaryKeysState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  D3D12_RESOURCE_STATES primaryValuesState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  D3D12_RESOURCE_STATES secondaryValuesState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  D3D12_RESOURCE_STATES visibleCounterState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  D3D12_RESOURCE_STATES sortMetaState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  D3D12_RESOURCE_STATES drawArgsState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  bool sortMetaValid = true;
  bool drawArgsFinalized = false;
};

class ISortBackend {
 public:
  virtual ~ISortBackend() = default;
  virtual Status Sort(const SortDispatchContext& context, SortDispatchResult& outResult) = 0;
};

struct AdvancedRenderOptions {
  const RenderHooks* hooks = nullptr;
  ISortBackend* sortBackend = nullptr;
};

struct RenderPreparationResult {
  FrameStats stats{};
  RenderSubmissionInfo submission{};
};

struct RenderResult {
  FrameStats stats{};
  RenderSubmissionInfo submission{};
  GpuFrameResources resources{};
  UpscalerInput upscalerInput{};
  bool hasUpscalerInput = false;
};

}  // namespace dxsplat
