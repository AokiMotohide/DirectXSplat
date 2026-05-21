#include <iostream>

#include "appcommon/image.h"
#include "dxsplat/context.h"
#include "dxsplat/renderer.h"
#include "dxsplat_examples/ExampleArgs.h"
#include "dxsplat_examples/ExampleD3D12.h"
#include "dxsplat_examples/ExampleRender.h"

int main(int argc, char** argv) {
  using namespace dxsplat;
  using namespace dxsplat::examples;

  StatusOr<ExampleOptions> parsed = ParseExampleOptions(ExampleKind::SceneUpdates, ArgsFromMain(argc, argv));
  if (!parsed.ok()) {
    std::cerr << parsed.status.message << "\n";
    return 1;
  }
  if (parsed.value.showHelp) {
    std::cout << ExampleUsage(ExampleKind::SceneUpdates) << "\n";
    return 0;
  }

  StatusOr<Scene> loaded = LoadExampleScene(parsed.value.scenePath);
  if (!loaded.ok()) {
    std::cerr << loaded.status.message << "\n";
    return 1;
  }

  D3D12ExampleDevice device;
  Status status = device.Initialize(parsed.value.forceWarp);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  D3D12Context context;
  status = context.Initialize(device.Device(), device.Queue(), device.Fence());
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  Renderer renderer;
  status = renderer.Initialize(context);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  Scene emptyScene = MakeSceneWithNoChunks(loaded.value);
  UploadedSceneHandle sceneHandle{};
  status = renderer.CreateUploadedScene(emptyScene, sceneHandle);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  SceneMutationToken token{};
  status = renderer.BeginSceneMutation(sceneHandle, token);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  UploadedChunkHandle firstChunk{};
  status = renderer.AddUploadedChunk(token, loaded.value.splatSets.front(), firstChunk);
  if (status.ok) {
    status = renderer.SetUploadedChunkScalingModifier(token, firstChunk, 1.0f);
  }
  if (status.ok) {
    status = renderer.SetUploadedChunkEnabled(token, firstChunk, true);
  }
  Status endStatus = renderer.EndSceneMutation(token);
  if (!status.ok || !endStatus.ok) {
    std::cerr << (!status.ok ? status.message : endStatus.message) << "\n";
    return 1;
  }

  OffscreenTarget target;
  status = device.CreateOffscreenTarget(parsed.value.width, parsed.value.height, target);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  RenderInput input = MakeExampleRenderInput(loaded.value, target.width, target.height, device.NextFenceValue());
  RenderResult result{};
  status = RenderSceneToOffscreen(device, renderer, sceneHandle, input, target, result);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  appcommon::ImageRgba8 image;
  status = device.ReadbackImage(target, image);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  status = appcommon::SavePpm(image, parsed.value.outputPath);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  std::cout << "updated scene and wrote " << parsed.value.outputPath << "\n";
  std::cout << "visible splats: " << result.stats.gaussiansVisible << "\n";
  return 0;
}
