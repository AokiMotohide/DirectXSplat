#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dxsplat/status.h"

namespace dxsplat::examples {

enum class ExampleKind {
  MinimalViewer,
  OffscreenCapture,
  ExternalD3D12Integration,
  SceneUpdates,
  GpuResourceInterop,
};

struct ExampleOptions {
  std::string scenePath;
  std::string outputPath;
  uint32_t width = 1280;
  uint32_t height = 720;
  uint32_t frameCount = 1;
  bool vsync = true;
  bool forceWarp = false;
  bool showHelp = false;
};

std::string ExampleName(ExampleKind kind);
std::string ExampleUsage(ExampleKind kind);
StatusOr<ExampleOptions> ParseExampleOptions(ExampleKind kind, const std::vector<std::string>& args);
std::vector<std::string> ArgsFromMain(int argc, char** argv);

}  // namespace dxsplat::examples
