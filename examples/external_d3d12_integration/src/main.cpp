#include <iostream>

#include "dxsplat/context.h"
#include "dxsplat/renderer.h"
#include "dxsplat_examples/ExampleArgs.h"
#include "dxsplat_examples/ExampleD3D12.h"
#include "dxsplat_examples/ExampleRender.h"

int main(int argc, char** argv) {
  using namespace dxsplat;
  using namespace dxsplat::examples;

  StatusOr<ExampleOptions> parsed = ParseExampleOptions(ExampleKind::ExternalD3D12Integration, ArgsFromMain(argc, argv));
  if (!parsed.ok()) {
    std::cerr << parsed.status.message << "\n";
    return 1;
  }
  if (parsed.value.showHelp) {
    std::cout << ExampleUsage(ExampleKind::ExternalD3D12Integration) << "\n";
    return 0;
  }

  StatusOr<Scene> loaded = LoadExampleScene(parsed.value.scenePath);
  if (!loaded.ok()) {
    std::cerr << loaded.status.message << "\n";
    return 1;
  }

  D3D12ExampleDevice host;
  Status status = host.Initialize(parsed.value.forceWarp);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  D3D12Context libraryContext;
  status = libraryContext.Initialize(host.Device(), host.Queue(), host.Fence());
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  Renderer renderer;
  RendererConfig config{};
  config.enableGpuTiming = true;
  status = renderer.Initialize(libraryContext, config);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  UploadedSceneHandle sceneHandle{};
  status = renderer.CreateUploadedScene(loaded.value, sceneHandle);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  OffscreenTarget target;
  status = host.CreateOffscreenTarget(parsed.value.width, parsed.value.height, target);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  RenderInput input = MakeExampleRenderInput(loaded.value, target.width, target.height, host.NextFenceValue());
  RenderResult result{};
  status = RenderSceneToOffscreen(host, renderer, sceneHandle, input, target, result);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  std::cout << "rendered with host-owned D3D12 device, queue, command list, and fence\n";
  std::cout << "visible splats: " << result.stats.gaussiansVisible << "\n";
  return 0;
}
