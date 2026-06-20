#include <iostream>

#include "platform/Image.h"
#include "dxsplat/context.h"
#include "dxsplat/renderer.h"
#include "dxsplat_examples/ExampleArgs.h"
#include "dxsplat_examples/ExampleD3D12.h"
#include "dxsplat_examples/ExampleRender.h"

int main(int argc, char** argv) {
  using namespace dxsplat;
  using namespace dxsplat::examples;

  StatusOr<ExampleOptions> parsed = ParseExampleOptions(ExampleKind::OffscreenCapture, ArgsFromMain(argc, argv));
  if (!parsed.ok()) {
    std::cerr << parsed.status.message << "\n";
    return 1;
  }
  if (parsed.value.showHelp) {
    std::cout << ExampleUsage(ExampleKind::OffscreenCapture) << "\n";
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

  UploadedSceneHandle sceneHandle{};
  status = UploadScene(renderer, loaded.value, sceneHandle, nullptr);
  if (!status.ok) {
    std::cerr << status.message << "\n";
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

  internal::ImageRgba8 image;
  status = device.ReadbackImage(target, image);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  status = internal::SavePpm(image, parsed.value.outputPath);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  std::cout << "wrote " << parsed.value.outputPath << " with " << result.stats.gaussiansVisible << " visible splats\n";
  return 0;
}
