#include "d3d12_example_common.h"

#include <cstdint>
#include <iostream>

#include <directxsplat/context.h>
#include <directxsplat/gpu_resources.h>
#include <directxsplat/io.h>
#include <directxsplat/renderer.h>
#include <directxsplat/scene.h>
#include <directxsplat/settings.h>
#include <directxsplat/status.h>
#include <directxsplat/types.h>

namespace {

uint64_t CountResidentBytes(const directxsplat::UploadedSceneGpuResources& resources) {
  uint64_t bytes = resources.sceneGaussians.sizeBytes;
  bytes += resources.sceneIndexToChunk.sizeBytes;
  bytes += resources.sortedSceneIndices.sizeBytes;
  bytes += resources.secondarySortedSceneIndices.sizeBytes;
  bytes += resources.visibleCounter.sizeBytes;
  bytes += resources.drawArgs.sizeBytes;
  bytes += resources.sortMeta.sizeBytes;
  for (const directxsplat::UploadedChunkGpuResources& chunk : resources.chunks) {
    bytes += chunk.gaussianData.sizeBytes;
  }
  return bytes;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: DirectXSplatGpuResourceInteropExample <scene>\n";
    return 1;
  }

  auto loaded = directxsplat::LoadSceneFromFile(argv[1]);
  if (!loaded.ok()) {
    return directxsplat_examples::PrintError("load scene", loaded.status);
  }

  directxsplat_examples::D3D12Host host;
  directxsplat::D3D12Context context;
  directxsplat::Renderer renderer;
  directxsplat::UploadedSceneHandle uploadedScene{};

  auto cleanup = [&]() {
    if (uploadedScene.IsValid()) {
      (void)renderer.DestroyUploadedScene(uploadedScene);
    }
    (void)renderer.Shutdown();
    context.Shutdown();
    host.Shutdown();
  };

  directxsplat::Status status = host.Initialize();
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("initialize host D3D12", status);
  }

  status = context.Initialize(host.Device(), host.Queue(), host.Fence());
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("initialize D3D12 context", status);
  }

  status = renderer.Initialize(context);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("initialize renderer", status);
  }

  status = renderer.CreateUploadedScene(loaded.value, uploadedScene);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("create uploaded scene", status);
  }

  constexpr uint32_t kWidth = 1280;
  constexpr uint32_t kHeight = 720;
  directxsplat_examples::OffscreenTarget target;
  status = host.CreateOffscreenTarget(kWidth, kHeight, D3D12_RESOURCE_STATE_RENDER_TARGET, target);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("create render target", status);
  }

  directxsplat::RenderFrameContext interopFrame = host.FrameContext();
  directxsplat::RenderInput input =
      directxsplat_examples::MakeRenderInput(loaded.value, target.width, target.height, interopFrame.frameIndex);

  directxsplat::RenderPreparationResult preparation{};
  status = renderer.PrepareSceneForRender(uploadedScene, input, interopFrame, &preparation);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("prepare scene for interop", status);
  }

  directxsplat::UploadedSceneGpuResources resources{};
  status = renderer.AcquireUploadedSceneGpuResources(uploadedScene, interopFrame, resources);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("acquire GPU resources", status);
  }

  status = host.QueueWait(resources.submission.uploadSyncPoint);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("wait for resource uploads", status);
  }

  // External GPU work that references leased resources must be submitted before signaling the lease fence.
  if (resources.leaseFence != nullptr && resources.leaseFenceValue != 0) {
    status = host.SignalFence(resources.leaseFence, resources.leaseFenceValue);
    if (!status.ok) {
      renderer.NotifyDeviceLost();
      cleanup();
      return directxsplat_examples::PrintError("signal resource lease", status);
    }
    if (resources.leaseFence == host.Fence()) {
      status = host.WaitForFenceValue(resources.leaseFenceValue);
      if (!status.ok) {
        renderer.NotifyDeviceLost();
        cleanup();
        return directxsplat_examples::PrintError("wait for resource lease", status);
      }
    }
  }

  directxsplat::RenderFrameContext frameContext = host.FrameContext();
  input.frameIndex = frameContext.frameIndex;

  status = renderer.PrepareSceneForRender(uploadedScene, input, frameContext, &preparation);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("prepare scene", status);
  }

  status = host.ResetCommandList();
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("reset command list", status);
  }

  directxsplat::RenderTargetBinding targetBinding = target.binding;
  directxsplat::RenderResult renderResult{};
  directxsplat::Status renderStatus =
      renderer.Render(host.CommandList(), targetBinding, uploadedScene, input, frameContext, renderResult);
  const bool commandListWorkRecorded = renderStatus.ok || renderResult.submission.submissionRequired;
  if (commandListWorkRecorded) {
    status = host.ExecuteCommandList(renderResult.submission.uploadSyncPoint, frameContext.submissionFenceValue);
    if (!status.ok) {
      renderer.NotifyDeviceLost();
      cleanup();
      return directxsplat_examples::PrintError("execute frame", status);
    }

    status = host.WaitForFenceValue(frameContext.submissionFenceValue);
    if (!status.ok) {
      renderer.NotifyDeviceLost();
      cleanup();
      return directxsplat_examples::PrintError("wait for frame", status);
    }
  }

  if (!renderStatus.ok) {
    cleanup();
    return directxsplat_examples::PrintError("render frame", renderStatus);
  }

  std::cout << "Acquired " << resources.chunks.size() << " chunk resource views, "
            << CountResidentBytes(resources) << " resident bytes; visible splats "
            << renderResult.stats.gaussiansVisible << " / " << preparation.stats.gaussiansTotal << "\n";
  cleanup();
  return 0;
}
