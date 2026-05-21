#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "appcommon/image.h"
#include "appcommon/swapchain_context.h"

namespace dxsplat {
namespace {

std::filesystem::path TempPath(const char* name) {
  return std::filesystem::temp_directory_path() / name;
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
}

}

TEST_CASE("PPM loader and writer reject malformed images") {
  const std::filesystem::path truncated = TempPath("directxsplat_truncated.ppm");
  WriteFile(truncated, "P6\n2 2\n255\nabc");
  const auto loaded = appcommon::LoadPpm(truncated.string());
  CHECK_FALSE(loaded.ok());

  appcommon::ImageRgba8 invalid{};
  invalid.width = 70000u;
  invalid.height = 1u;
  invalid.pixels.resize(4u);
  CHECK_FALSE(appcommon::SavePpm(invalid, TempPath("directxsplat_invalid.ppm").string()).ok);

  std::error_code ec;
  std::filesystem::remove(truncated, ec);
  std::filesystem::remove(TempPath("directxsplat_invalid.ppm"), ec);
}

TEST_CASE("PPM loader and writer roundtrip deterministic RGBA images") {
  const std::filesystem::path path = TempPath("directxsplat_roundtrip.ppm");

  appcommon::ImageRgba8 image{};
  image.width = 2u;
  image.height = 2u;
  image.pixels = {
      255u, 0u, 0u, 255u,
      0u, 255u, 0u, 128u,
      0u, 0u, 255u, 64u,
      10u, 20u, 30u, 0u,
  };

  REQUIRE(appcommon::SavePpm(image, path.string()).ok);
  const auto loaded = appcommon::LoadPpm(path.string());
  REQUIRE(loaded.ok());
  CHECK(loaded.value.width == 2u);
  CHECK(loaded.value.height == 2u);
  REQUIRE(loaded.value.pixels.size() == 16u);
  CHECK(loaded.value.pixels[0] == 255u);
  CHECK(loaded.value.pixels[1] == 0u);
  CHECK(loaded.value.pixels[2] == 0u);
  CHECK(loaded.value.pixels[3] == 255u);
  CHECK(loaded.value.pixels[7] == 255u);
  CHECK(loaded.value.pixels[11] == 255u);
  CHECK(loaded.value.pixels[12] == 10u);
  CHECK(loaded.value.pixels[13] == 20u);
  CHECK(loaded.value.pixels[14] == 30u);
  CHECK(loaded.value.pixels[15] == 255u);

  std::error_code ec;
  std::filesystem::remove(path, ec);
}

TEST_CASE("PPM loader rejects invalid header matrix") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "directxsplat_ppm_matrix";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);

  const std::vector<std::string> cases{
      "P3\n1 1\n255\nabc",
      "P6\n0 1\n255\n",
      "P6\n1 0\n255\n",
      "P6\n1 1\n1\nabc",
      "P6\n70000 1\n255\nabc",
      "P6\n1 1\n255",
  };

  for (size_t i = 0; i < cases.size(); ++i) {
    const std::filesystem::path path = dir / ("case_" + std::to_string(i) + ".ppm");
    WriteFile(path, cases[i]);
    CHECK_FALSE(appcommon::LoadPpm(path.string()).ok());
  }

  std::filesystem::remove_all(dir, ec);
}

TEST_CASE("SwapchainContext uninitialized public calls fail without crashing") {
  appcommon::SwapchainContext context;
  CHECK_FALSE(context.BeginFrame().ok);
  CHECK_FALSE(context.EndFrame(false).ok);
  CHECK_FALSE(context.Resize(16, 16).ok);
  CHECK(context.WaitForGpu().ok);
  CHECK(context.PendingSubmissionFenceValue() == 0u);
  CHECK(context.CompletedFenceValue() == 0u);
  CHECK(context.Device() == nullptr);
  CHECK(context.CommandQueue() == nullptr);
  CHECK(context.CommandList() == nullptr);
  CHECK(context.CurrentBackBuffer() == nullptr);
}

}  // namespace dxsplat
