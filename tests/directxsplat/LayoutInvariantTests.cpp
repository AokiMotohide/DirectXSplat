#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#include "dxsplat/settings.h"
#include "dxsplat/vram_format.h"
#include "renderer/diagnostics.h"

namespace dxsplat {
namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

uint32_t ExpectedStride(VramAttributeFormat rgbaFormat, VramAttributeFormat shFormat) {
  const uint32_t rgbaBytes = 4u * AttributeFormatSizeBytes(rgbaFormat);
  const uint32_t shOffset = AlignPackedOffset4(24u + rgbaBytes);
  const uint32_t shBytes = 45u * AttributeFormatSizeBytes(shFormat);
  const uint32_t idOffset = AlignPackedOffset4(shOffset + shBytes);
  return AlignPackedOffset4(idOffset + 8u);
}

}

TEST_CASE("VRAM format stride and shader byte-address limits stay consistent") {
  const std::array<VramAttributeFormat, 3> formats{
      VramAttributeFormat::Float32,
      VramAttributeFormat::Float16,
      VramAttributeFormat::Uint8,
  };

  for (VramAttributeFormat rgbaFormat : formats) {
    for (VramAttributeFormat shFormat : formats) {
      const VramFormatSettings settings{rgbaFormat, shFormat};
      const uint32_t stride = EstimatePackedGaussianStrideBytes(settings);
      CHECK(stride == ExpectedStride(rgbaFormat, shFormat));
      CHECK((stride % 4u) == 0u);
      const uint64_t maxGaussians = std::numeric_limits<uint32_t>::max() / stride;
      CHECK(maxGaussians > 0u);
      CHECK(maxGaussians * stride <= std::numeric_limits<uint32_t>::max());
      CHECK((maxGaussians + 1u) * stride > std::numeric_limits<uint32_t>::max());
    }
  }
}

TEST_CASE("CPU diagnostic parameter block layout remains stable") {
  CHECK(ValidatePipelineParameterBlockLayout(32u, 4u).ok);
  CHECK_FALSE(ValidatePipelineParameterBlockLayout(28u, 4u).ok);
}

TEST_CASE("HLSL keeps the renderer-visible packed Gaussian layout fields") {
  const std::filesystem::path root = std::filesystem::path(DIRECTXSPLAT_TEST_ASSET_DIR).parent_path().parent_path();
  const std::string compute = ReadTextFile(root / "shaders" / "gaussian_compute.hlsl");
  const std::string raster = ReadTextFile(root / "shaders" / "gaussian_raster.hlsl");

  REQUIRE_FALSE(compute.empty());
  REQUIRE_FALSE(raster.empty());
  CHECK(compute.find("uint gSceneGaussianStride") != std::string::npos);
  CHECK(compute.find("uint gRgbaOffset") != std::string::npos);
  CHECK(compute.find("uint gShOffset") != std::string::npos);
  CHECK(compute.find("uint gIdOffset") != std::string::npos);
  CHECK(compute.find("return index * gSceneGaussianStride") != std::string::npos);
  CHECK(raster.find("uint gSceneGaussianStride") != std::string::npos);
  CHECK(raster.find("uint gRgbaOffset") != std::string::npos);
  CHECK(raster.find("uint gShOffset") != std::string::npos);
  CHECK(raster.find("uint gIdOffset") != std::string::npos);
  CHECK(raster.find("return index * gSceneGaussianStride") != std::string::npos);
}

TEST_CASE("Shaders stay final-only") {
  const std::filesystem::path root = std::filesystem::path(DIRECTXSPLAT_TEST_ASSET_DIR).parent_path().parent_path();
  const std::string compute = ReadTextFile(root / "shaders" / "gaussian_compute.hlsl");
  const std::string raster = ReadTextFile(root / "shaders" / "gaussian_raster.hlsl");

  REQUIRE_FALSE(compute.empty());
  REQUIRE_FALSE(raster.empty());
  CHECK(compute.find("gVisualization == 16u") == std::string::npos);
  CHECK(raster.find("PSMainDebug") == std::string::npos);
  CHECK(raster.find("VSMainDebug") == std::string::npos);
  CHECK(raster.find("case 15u") == std::string::npos);
}

TEST_CASE("Renderer residency cache keys include view-space depth convention") {
  const std::filesystem::path root = std::filesystem::path(DIRECTXSPLAT_TEST_ASSET_DIR).parent_path().parent_path();
  const std::string renderer = ReadTextFile(root / "directxsplat" / "src" / "renderer" / "Renderer.cpp");

  REQUIRE_FALSE(renderer.empty());
  CHECK(renderer.find("HashBytes(&input.settings.positiveViewSpaceZ") != std::string::npos);
}

TEST_CASE("raw PLY fallback budgets stay bounded") {
  const std::filesystem::path root = std::filesystem::path(DIRECTXSPLAT_TEST_ASSET_DIR).parent_path().parent_path();
  const std::string reader =
      ReadTextFile(root / "directxsplat" / "src" / "io" / "formats" / "ply" / "raw" / "ply_reader.cpp");

  REQUIRE_FALSE(reader.empty());
  CHECK(reader.find("kMaxPlyFileBytes = 256ull * 1024ull * 1024ull") != std::string::npos);
  CHECK(reader.find("kMaxPlyScalarBytes = 256ull * 1024ull * 1024ull") != std::string::npos);
}

}  // namespace dxsplat
