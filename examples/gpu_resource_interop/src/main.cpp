#include <iostream>

#include "dxsplat/context.h"
#include "dxsplat/gpu_resources.h"
#include "dxsplat/renderer.h"
#include "dxsplat_examples/ExampleArgs.h"
#include "dxsplat_examples/ExampleD3D12.h"
#include "dxsplat_examples/ExampleRender.h"

int main(int argc, char** argv) {
  using namespace dxsplat;
  using namespace dxsplat::examples;

  StatusOr<ExampleOptions> parsed = ParseExampleOptions(ExampleKind::GpuResourceInterop, ArgsFromMain(argc, argv));
  if (!parsed.ok()) {
    std::cerr << parsed.status.message << "\n";
    return 1;
  }
  if (parsed.value.showHelp) {
    std::cout << ExampleUsage(ExampleKind::GpuResourceInterop) << "\n";
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
  status = renderer.CreateUploadedScene(loaded.value, sceneHandle);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  RenderFrameContext frameContext = device.FrameContext();
  RenderInput input = MakeExampleRenderInput(loaded.value, parsed.value.width, parsed.value.height, frameContext.frameIndex);
  RenderPreparationResult preparation{};
  status = renderer.PrepareSceneForRender(sceneHandle, input, frameContext, &preparation);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  UploadedSceneGpuResources resources{};
  status = renderer.GetUploadedSceneGpuResources(sceneHandle, frameContext, resources);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  status = device.SignalFrame(resources.submission.uploadSyncPoint);
  if (!status.ok) {
    renderer.NotifyDeviceLost();
    std::cerr << status.message << "\n";
    return 1;
  }

  std::cout << "scene buffer bytes: " << resources.sceneGaussians.sizeBytes << "\n";
  std::cout << "chunk resources: " << resources.chunks.size() << "\n";
  std::cout << "lease fence value: " << resources.leaseFenceValue << "\n";
  std::cout << "caller may transition scene buffer: " << (resources.sceneGaussians.callerMayTransition ? "true" : "false") << "\n";
  return 0;
}
