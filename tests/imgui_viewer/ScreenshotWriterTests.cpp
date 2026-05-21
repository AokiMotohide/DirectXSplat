#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>

#include "appcommon/swapchain_context.h"
#include "tools/ScreenshotWriter.h"

namespace dxsplat {
namespace {

std::filesystem::path TempPath(const char* name) {
  return std::filesystem::temp_directory_path() / name;
}

}

TEST_CASE("Screenshot writer rejects invalid capture contexts without pending work") {
  ScreenshotWriter writer;
  appcommon::SwapchainContext context;
  CHECK_FALSE(writer.QueueBackBufferPpm(context, "").ok);
  CHECK_FALSE(writer.QueueBackBufferPpm(context, TempPath("directxsplat_capture.ppm").string()).ok);
  CHECK_FALSE(writer.HasPendingCapture());
  CHECK(writer.ResolvePendingCapture().ok);
}

}  // namespace dxsplat
