#include "dxsplat_examples/ExampleArgs.h"

#include <charconv>
#include <sstream>

namespace dxsplat::examples {
namespace {

bool ParseU32(std::string_view text, uint32_t& out) {
  if (text.empty()) {
    return false;
  }
  uint32_t value = 0;
  const char* first = text.data();
  const char* last = text.data() + text.size();
  const std::from_chars_result result = std::from_chars(first, last, value);
  if (result.ec != std::errc{} || result.ptr != last || value == 0) {
    return false;
  }
  out = value;
  return true;
}

bool ParseSize(std::string_view text, uint32_t& width, uint32_t& height) {
  const size_t split = text.find_first_of("xX");
  if (split == std::string_view::npos) {
    return false;
  }
  uint32_t parsedWidth = 0;
  uint32_t parsedHeight = 0;
  if (!ParseU32(text.substr(0, split), parsedWidth) || !ParseU32(text.substr(split + 1), parsedHeight)) {
    return false;
  }
  if (parsedWidth > 16384 || parsedHeight > 16384) {
    return false;
  }
  width = parsedWidth;
  height = parsedHeight;
  return true;
}

bool NeedsOutput(ExampleKind kind) {
  return kind == ExampleKind::OffscreenCapture || kind == ExampleKind::SceneUpdates;
}

std::string DefaultOutput(ExampleKind kind) {
  switch (kind) {
    case ExampleKind::OffscreenCapture:
      return "directxsplat_capture.ppm";
    case ExampleKind::SceneUpdates:
      return "directxsplat_scene_updates.ppm";
    default:
      return {};
  }
}

uint32_t DefaultFrames(ExampleKind kind) {
  return kind == ExampleKind::MinimalViewer ? 0u : 1u;
}

}  // namespace

std::string ExampleName(ExampleKind kind) {
  switch (kind) {
    case ExampleKind::MinimalViewer:
      return "minimal_viewer";
    case ExampleKind::OffscreenCapture:
      return "offscreen_capture";
    case ExampleKind::ExternalD3D12Integration:
      return "external_d3d12_integration";
    case ExampleKind::SceneUpdates:
      return "scene_updates";
    case ExampleKind::GpuResourceInterop:
      return "gpu_resource_interop";
    default:
      return "example";
  }
}

std::string ExampleUsage(ExampleKind kind) {
  std::ostringstream usage;
  usage << ExampleName(kind) << " <scene_path>";
  if (NeedsOutput(kind)) {
    usage << " [--output capture.ppm]";
  }
  usage << " [--render-size 1280x720]";
  if (kind == ExampleKind::MinimalViewer) {
    usage << " [--frames N] [--no-vsync]";
  }
  usage << " [--warp]";
  return usage.str();
}

StatusOr<ExampleOptions> ParseExampleOptions(ExampleKind kind, const std::vector<std::string>& args) {
  ExampleOptions options{};
  options.outputPath = DefaultOutput(kind);
  options.frameCount = DefaultFrames(kind);

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg == "--help" || arg == "-h") {
      options.showHelp = true;
      return StatusOr<ExampleOptions>::Ok(std::move(options));
    }
    if (arg == "--render-size") {
      if (i + 1 >= args.size() || !ParseSize(args[i + 1], options.width, options.height)) {
        return StatusOr<ExampleOptions>::Error("invalid --render-size, expected WxH");
      }
      ++i;
      continue;
    }
    if (arg == "--output") {
      if (i + 1 >= args.size() || args[i + 1].empty()) {
        return StatusOr<ExampleOptions>::Error("missing --output path");
      }
      options.outputPath = args[++i];
      continue;
    }
    if (arg == "--frames") {
      if (i + 1 >= args.size() || !ParseU32(args[i + 1], options.frameCount)) {
        return StatusOr<ExampleOptions>::Error("invalid --frames value");
      }
      ++i;
      continue;
    }
    if (arg == "--no-vsync") {
      options.vsync = false;
      continue;
    }
    if (arg == "--warp") {
      options.forceWarp = true;
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      return StatusOr<ExampleOptions>::Error("unknown option: " + arg);
    }
    if (!options.scenePath.empty()) {
      return StatusOr<ExampleOptions>::Error("multiple scene paths provided");
    }
    options.scenePath = arg;
  }

  if (options.scenePath.empty()) {
    return StatusOr<ExampleOptions>::Error(ExampleUsage(kind));
  }
  if (NeedsOutput(kind) && options.outputPath.empty()) {
    return StatusOr<ExampleOptions>::Error("missing output path");
  }
  return StatusOr<ExampleOptions>::Ok(std::move(options));
}

std::vector<std::string> ArgsFromMain(int argc, char** argv) {
  std::vector<std::string> args;
  if (argc <= 1) {
    return args;
  }
  args.reserve(static_cast<size_t>(argc - 1));
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i] != nullptr ? argv[i] : "");
  }
  return args;
}

}  // namespace dxsplat::examples
