#pragma once

#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "directxsplat/context.h"
#include "directxsplat/extensions.h"
#include "directxsplat/gpu_resources.h"
#include "directxsplat/scene.h"
#include "directxsplat/settings.h"
#include "directxsplat/status.h"
#include "directxsplat/types.h"

namespace directxsplat {

struct RendererConfig {
  uint64_t defaultSplatBudget = 0;
  uint64_t residencyBudgetGaussians = 16ull * 1024ull * 1024ull;
  uint64_t uploadBudgetGaussians = 650000;
  uint64_t warmUploadBudgetGaussians = 1200000;
  uint32_t maxUploadsPerFrame = 16;
  uint32_t chunkTargetSize = 65536;
  float lod0ScreenRadius = 72.0f;
  float lod1ScreenRadius = 24.0f;
  float cullScreenRadius = 0.35f;
  uint64_t residencyCacheFrames = 120;
  bool enableGpuTiming = true;
};

class Renderer {
 public:
  Renderer();
  ~Renderer();

  Status Initialize(D3D12Context& context);
  Status Initialize(D3D12Context& context, const RendererConfig& config);
  Status Shutdown();
  Status Reset();
  void NotifyDeviceLost();
  bool IsDeviceLost() const;

  Status CreateUploadedScene(UploadedSceneHandle& outHandle);
  Status CreateUploadedScene(const Scene& scene,
                             UploadedSceneHandle& outHandle,
                             std::vector<UploadedChunkHandle>* outChunkHandles = nullptr);
  Status UpdateUploadedScene(UploadedSceneHandle handle, const Scene& scene);
  Status UpdateUploadedScene(SceneMutationToken token, const Scene& scene);
  Status DestroyUploadedScene(UploadedSceneHandle handle);
  Status DestroyUploadedScene(SceneMutationToken token);
  bool IsUploadedSceneValid(UploadedSceneHandle handle) const;
  bool IsSceneReadyToRender(UploadedSceneHandle sceneHandle) const;
  Status GetSceneAccessInfo(UploadedSceneHandle sceneHandle, SceneAccessInfo& outInfo) const;
  Status GetUploadedSceneInfo(UploadedSceneHandle sceneHandle, UploadedSceneInfo& outInfo) const;
  Status GetUploadedChunkInfo(UploadedSceneHandle sceneHandle, UploadedChunkHandle chunkHandle, UploadedChunkInfo& outInfo) const;
  Status GetUploadedSceneGpuResources(UploadedSceneHandle sceneHandle,
                                      const RenderFrameContext& frameContext,
                                      UploadedSceneGpuResources& outResources) const;
  Status AcquireUploadedSceneGpuResources(UploadedSceneHandle sceneHandle,
                                          const RenderFrameContext& frameContext,
                                          UploadedSceneGpuResources& outResources);
  Status PrepareSceneForRender(UploadedSceneHandle sceneHandle,
                                const RenderInput& input,
                                const RenderFrameContext& frameContext,
                                RenderPreparationResult* outResult = nullptr);
  Status BeginSceneMutation(UploadedSceneHandle sceneHandle, SceneMutationToken& outToken);
  Status EndSceneMutation(SceneMutationToken token);
  Status GetUploadedSceneChunks(UploadedSceneHandle sceneHandle, std::vector<UploadedChunkHandle>& outChunkHandles) const;
  Status AddUploadedChunk(UploadedSceneHandle sceneHandle, const GaussianSet& chunk, UploadedChunkHandle& outChunkHandle);
  Status AddUploadedChunk(SceneMutationToken token, const GaussianSet& chunk, UploadedChunkHandle& outChunkHandle);
  Status UpdateUploadedChunk(UploadedSceneHandle sceneHandle, UploadedChunkHandle chunkHandle, const GaussianSet& chunk);
  Status UpdateUploadedChunk(SceneMutationToken token, UploadedChunkHandle chunkHandle, const GaussianSet& chunk);
  Status RemoveUploadedChunk(UploadedSceneHandle sceneHandle, UploadedChunkHandle chunkHandle);
  Status RemoveUploadedChunk(SceneMutationToken token, UploadedChunkHandle chunkHandle);
  Status SetUploadedChunkEnabled(UploadedSceneHandle sceneHandle, UploadedChunkHandle chunkHandle, bool enabled);
  Status SetUploadedChunkEnabled(SceneMutationToken token, UploadedChunkHandle chunkHandle, bool enabled);
  Status SetUploadedChunkScalingModifier(UploadedSceneHandle sceneHandle, UploadedChunkHandle chunkHandle, float scalingModifier);
  Status SetUploadedChunkScalingModifier(SceneMutationToken token, UploadedChunkHandle chunkHandle, float scalingModifier);
  bool IsUploadedChunkValid(UploadedSceneHandle sceneHandle, UploadedChunkHandle chunkHandle) const;

  Status Render(ID3D12GraphicsCommandList* commandList,
                const RenderTargetBinding& target,
                UploadedSceneHandle sceneHandle,
                const RenderInput& input,
                const RenderFrameContext& frameContext,
                RenderResult& outResult);
  Status Render(ID3D12GraphicsCommandList* commandList,
                const RenderTargetBinding& target,
                UploadedSceneHandle sceneHandle,
                const RenderInput& input,
                const AdvancedRenderOptions& options,
                const RenderFrameContext& frameContext,
                RenderResult& outResult);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace directxsplat
