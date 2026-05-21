#include "dxsplat_examples/ExampleRender.h"

#include <algorithm>

#include "appcommon/image.h"
#include "dxsplat/bounding.h"

namespace dxsplat::examples {
namespace {

constexpr float kPi = 3.14159265358979323846f;

Aabb SceneBoundsOrFallback(const Scene& scene) {
  if (scene.sceneBounds.valid) {
    return scene.sceneBounds;
  }

  Aabb bounds{};
  for (const GaussianSet& set : scene.splatSets) {
    if (!set.bounds.valid) {
      continue;
    }
    if (!bounds.valid) {
      bounds = set.bounds;
    } else {
      bounds.min = Min(bounds.min, set.bounds.min);
      bounds.max = Max(bounds.max, set.bounds.max);
      bounds.valid = true;
    }
  }
  if (!bounds.valid) {
    bounds.min = {-1.0f, -1.0f, -1.0f};
    bounds.max = {1.0f, 1.0f, 1.0f};
    bounds.valid = true;
  }
  return bounds;
}

}  // namespace

StatusOr<Scene> LoadExampleScene(const std::string& path) {
  StatusOr<Scene> loaded = LoadSceneFromFile(path);
  if (!loaded.ok()) {
    return loaded;
  }
  if (loaded.value.splatSets.empty()) {
    return StatusOr<Scene>::Error("scene contains no splat sets");
  }
  return loaded;
}

RenderInput MakeExampleRenderInput(const Scene& scene, uint32_t width, uint32_t height, uint64_t frameIndex) {
  const Aabb bounds = SceneBoundsOrFallback(scene);
  const Vec3 center = ComputeAabbCenter(bounds);
  const float radius = std::max(ComputeAabbRadius(bounds), 1.0f);
  const Vec3 eye{center.x, center.y, center.z - radius * 2.5f};
  RenderInput input{};
  input.viewportWidth = std::max(width, 1u);
  input.viewportHeight = std::max(height, 1u);
  input.cameraPosition = eye;
  input.view = LookAt(eye, center, {0.0f, 1.0f, 0.0f});
  input.proj = Perspective(kPi / 3.0f, static_cast<float>(input.viewportWidth) / static_cast<float>(input.viewportHeight), 0.1f, 5000.0f);
  input.nearPlane = 0.1f;
  input.farPlane = 5000.0f;
  input.frameIndex = frameIndex;
  input.settings.antialiasing = true;
  input.settings.fastCulling = true;
  return input;
}

RenderTargetBinding MakeOffscreenRenderTarget(const OffscreenTarget& target) {
  RenderTargetBinding binding{};
  binding.colorTarget = target.color.Get();
  binding.colorRtv = target.rtv;
  binding.colorFormat = target.format;
  binding.colorStateBefore = D3D12_RESOURCE_STATE_COMMON;
  binding.colorStateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  binding.transitionMode = ResourceTransitionMode::LibraryManaged;
  binding.viewport.TopLeftX = 0.0f;
  binding.viewport.TopLeftY = 0.0f;
  binding.viewport.Width = static_cast<float>(target.width);
  binding.viewport.Height = static_cast<float>(target.height);
  binding.viewport.MinDepth = 0.0f;
  binding.viewport.MaxDepth = 1.0f;
  binding.scissor.left = 0;
  binding.scissor.top = 0;
  binding.scissor.right = static_cast<LONG>(target.width);
  binding.scissor.bottom = static_cast<LONG>(target.height);
  binding.clearColor = true;
  binding.clearColorValue[0] = 0.02f;
  binding.clearColorValue[1] = 0.02f;
  binding.clearColorValue[2] = 0.025f;
  binding.clearColorValue[3] = 1.0f;
  return binding;
}

Status RenderSceneToOffscreen(D3D12ExampleDevice& device,
                              Renderer& renderer,
                              UploadedSceneHandle sceneHandle,
                              const RenderInput& input,
                              OffscreenTarget& target,
                              RenderResult& outResult) {
  Status status = device.BeginCommands();
  if (!status.ok) {
    return status;
  }

  RenderFrameContext frameContext = device.FrameContext();
  RenderPreparationResult preparation{};
  status = renderer.PrepareSceneForRender(sceneHandle, input, frameContext, &preparation);
  if (!status.ok) {
    Status finish = device.FinishCommands(preparation.submission.uploadSyncPoint, preparation.submission.submissionRequired);
    return finish.ok ? status : finish;
  }

  RenderResult result{};
  RenderTargetBinding binding = MakeOffscreenRenderTarget(target);
  Status renderStatus = renderer.Render(device.CommandList(), binding, sceneHandle, input, frameContext, result);
  bool copyRecorded = false;
  if (renderStatus.ok) {
    status = device.RecordReadback(target);
    if (!status.ok) {
      Status finish = device.FinishCommands(result.submission.uploadSyncPoint, result.submission.submissionRequired);
      return finish.ok ? status : finish;
    }
    copyRecorded = true;
  }

  Status finish = device.FinishCommands(result.submission.uploadSyncPoint, result.submission.submissionRequired || copyRecorded);
  if (!finish.ok) {
    return finish;
  }
  if (!renderStatus.ok) {
    return renderStatus;
  }

  outResult = std::move(result);
  return Status::Ok();
}

Status UploadScene(Renderer& renderer,
                   const Scene& scene,
                   UploadedSceneHandle& outSceneHandle,
                   std::vector<UploadedChunkHandle>* outChunks) {
  return renderer.CreateUploadedScene(scene, outSceneHandle, outChunks);
}

Scene MakeSceneWithNoChunks(const Scene& source) {
  Scene scene{};
  scene.sourcePath = source.sourcePath;
  scene.sceneBounds = source.sceneBounds;
  scene.vramFormat = source.vramFormat;
  scene.inputCameras = source.inputCameras;
  return scene;
}

}  // namespace dxsplat::examples
