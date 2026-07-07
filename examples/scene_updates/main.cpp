#include "d3d12_example_common.h"

#include <algorithm>
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

constexpr float kShC0 = 0.28209479177387814f;

float ToShDc(float channel) {
  return (channel - 0.5f) / kShC0;
}

directxsplat::GaussianSet MakeRuntimeChunk(const directxsplat::Scene& scene) {
  const directxsplat_examples::SceneView view = directxsplat_examples::EstimateSceneView(scene);
  const float radius = std::max(view.radius * 0.06f, 0.02f);
  const directxsplat::Vec3 position{view.center.x, view.center.y + view.radius * 0.15f, view.center.z};

  directxsplat::Gaussian gaussian{};
  gaussian.position = position;
  gaussian.scale = {radius, radius, radius};
  gaussian.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
  gaussian.opacity = 2.0f;
  gaussian.sh[0] = ToShDc(1.0f);
  gaussian.sh[16] = ToShDc(0.35f);
  gaussian.sh[32] = ToShDc(0.1f);

  directxsplat::GaussianSet set{};
  set.name = "runtime_chunk";
  set.gaussians.push_back(gaussian);
  set.bounds.valid = true;
  set.bounds.min = {position.x - radius, position.y - radius, position.z - radius};
  set.bounds.max = {position.x + radius, position.y + radius, position.z + radius};
  set.visible = true;
  set.scalingModifier = 1.0f;
  return set;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: DirectXSplatSceneUpdatesExample <scene>\n";
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

  directxsplat::SceneMutationToken token{};
  status = renderer.BeginSceneMutation(uploadedScene, token);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("begin scene mutation", status);
  }

  directxsplat::UploadedChunkHandle addedChunk{};
  directxsplat::GaussianSet runtimeChunk = MakeRuntimeChunk(loaded.value);
  status = renderer.AddUploadedChunk(token, runtimeChunk, addedChunk);
  if (status.ok) {
    status = renderer.SetUploadedChunkScalingModifier(token, addedChunk, 1.2f);
  }

  directxsplat::Status endStatus = renderer.EndSceneMutation(token);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("mutate scene", status);
  }
  if (!endStatus.ok) {
    cleanup();
    return directxsplat_examples::PrintError("end scene mutation", endStatus);
  }

  directxsplat::UploadedChunkInfo chunkInfo{};
  status = renderer.GetUploadedChunkInfo(uploadedScene, addedChunk, chunkInfo);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("inspect added chunk", status);
  }

  constexpr uint32_t kWidth = 1280;
  constexpr uint32_t kHeight = 720;
  directxsplat_examples::OffscreenTarget target;
  status = host.CreateOffscreenTarget(kWidth, kHeight, D3D12_RESOURCE_STATE_RENDER_TARGET, target);
  if (!status.ok) {
    cleanup();
    return directxsplat_examples::PrintError("create render target", status);
  }

  directxsplat::RenderFrameContext frameContext = host.FrameContext();
  directxsplat::RenderInput input =
      directxsplat_examples::MakeRenderInput(loaded.value, target.width, target.height, frameContext.frameIndex);

  directxsplat::RenderPreparationResult preparation{};
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

  std::cout << "Added chunk " << chunkInfo.handle.value << " with " << chunkInfo.gaussianCount
            << " Gaussian; visible splats " << renderResult.stats.gaussiansVisible << " / "
            << preparation.stats.gaussiansTotal << "\n";
  cleanup();
  return 0;
}
