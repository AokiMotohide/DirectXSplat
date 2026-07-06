#include <doctest/doctest.h>

#include <directxsplat/bounding.h>
#include <directxsplat/context.h>
#include <directxsplat/directxsplat.h>
#include <directxsplat/extensions.h>
#include <directxsplat/gpu_resources.h>
#include <directxsplat/io.h>
#include <directxsplat/math.h>
#include <directxsplat/render_hooks.h>
#include <directxsplat/renderer.h>
#include <directxsplat/scene.h>
#include <directxsplat/settings.h>
#include <directxsplat/sort.h>
#include <directxsplat/status.h>
#include <directxsplat/types.h>
#include <directxsplat/upscaling.h>
#include <directxsplat/vram_format.h>

TEST_CASE("Advanced public headers expose host renderer types") {
  directxsplat::D3D12Context context;
  CHECK(context.Device() == nullptr);
  CHECK(context.CommandQueue() == nullptr);

  directxsplat::Renderer renderer;
  CHECK_FALSE(renderer.IsDeviceLost());

  directxsplat::RendererConfig config{};
  CHECK(config.maxUploadsPerFrame > 0);

  directxsplat::Scene scene{};
  directxsplat::GaussianSet set{};
  directxsplat::Gaussian gaussian{};
  set.gaussians.push_back(gaussian);
  scene.splatSets.push_back(set);
  CHECK(scene.splatSets.size() == 1u);

  directxsplat::RenderInput input{};
  directxsplat::RenderSettings settings{};
  directxsplat::RenderPreparationResult preparation{};
  directxsplat::RenderResult renderResult{};
  directxsplat::RenderTargetBinding target{};
  directxsplat::RenderFrameContext frameContext{};
  directxsplat::RenderSubmissionInfo submission{};
  directxsplat::FrameStats stats{};

  CHECK(input.viewportWidth == 1u);
  CHECK(settings.renderType == directxsplat::RenderType::Color);
  CHECK_FALSE(frameContext.HasFence());
  CHECK(target.transitionMode == directxsplat::ResourceTransitionMode::LibraryManaged);
  CHECK_FALSE(submission.submissionRequired);
  CHECK(preparation.stats.gaussiansTotal == 0u);
  CHECK(renderResult.stats.gaussiansTotal == 0u);
  CHECK(stats.sortBackend == directxsplat::SortBackend::OneSweep);

  directxsplat::GpuBufferView buffer{};
  directxsplat::GpuTextureView texture{};
  directxsplat::GpuFrameResources frameResources{};
  directxsplat::UploadedSceneGpuResources sceneResources{};
  directxsplat::UploadedChunkGpuResources chunkResources{};
  directxsplat::UploadedSceneInfo sceneInfo{};
  directxsplat::UploadedChunkInfo chunkInfo{};
  directxsplat::SceneAccessInfo accessInfo{};

  CHECK_FALSE(buffer.IsValid());
  CHECK_FALSE(texture.IsValid());
  CHECK_FALSE(frameResources.colorValid);
  CHECK(sceneResources.chunks.empty());
  CHECK(chunkResources.format == directxsplat::UploadedSceneBufferFormat::Unknown);
  CHECK_FALSE(sceneInfo.readyToRender);
  CHECK_FALSE(chunkInfo.resident);
  CHECK_FALSE(accessInfo.mutationActive);

  directxsplat::UploadedSceneHandle uploadedScene{};
  directxsplat::UploadedChunkHandle uploadedChunk{};
  directxsplat::SceneMutationToken mutation{};
  directxsplat::UploadSyncPoint upload{};

  CHECK_FALSE(uploadedScene.IsValid());
  CHECK_FALSE(uploadedChunk.IsValid());
  CHECK_FALSE(mutation.IsValid());
  CHECK_FALSE(upload.IsValid());

  directxsplat::VramFormatSettings vram{};
  directxsplat::UpscalerInput upscaler{};
  directxsplat::RenderHooks hooks{};
  directxsplat::AdvancedRenderOptions options{};

  CHECK(directxsplat::AttributeFormatSizeBytes(vram.rgbaFormat) > 0u);
  CHECK(upscaler.outputWidth == 0u);
  CHECK(options.hooks == nullptr);
  CHECK_FALSE(static_cast<bool>(hooks.afterRaster));
  CHECK(directxsplat::DetectSceneFormat("scene.ply") == directxsplat::SceneFormat::Ply);
}
