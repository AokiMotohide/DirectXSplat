#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "dxsplat/settings.h"

namespace dxsplat {
namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

}

TEST_CASE("RenderType string conversions are lowercase") {
  CHECK(std::string(RenderTypeLabel(RenderType::Color)) == "color");
  CHECK(std::string(RenderTypeLabel(RenderType::Alpha)) == "alpha");
  CHECK(std::string(RenderTypeLabel(RenderType::Depth)) == "depth");
}

TEST_CASE("RenderSettings defaults match UI defaults") {
  const RenderSettings settings{};

  CHECK(settings.renderType == RenderType::Color);
  CHECK(settings.backgroundColor.x == doctest::Approx(0.0f));
  CHECK(settings.backgroundColor.y == doctest::Approx(0.0f));
  CHECK(settings.backgroundColor.z == doctest::Approx(0.0f));
  CHECK_FALSE(settings.gammaCorrection);
  CHECK(settings.antialiasing);
  CHECK(settings.antialiasingStrength == doctest::Approx(1.0f));
  CHECK(settings.fastCulling);
}

TEST_CASE("Shaders expose render settings constants") {
  const std::filesystem::path root = std::filesystem::path(DIRECTXSPLAT_TEST_ASSET_DIR).parent_path().parent_path();
  const std::string compute =
      ReadTextFile(root / "directxsplat" / "src" / "shaders" / "gaussian_compute.hlsl");
  const std::string raster =
      ReadTextFile(root / "directxsplat" / "src" / "shaders" / "gaussian_raster.hlsl");

  REQUIRE_FALSE(compute.empty());
  REQUIRE_FALSE(raster.empty());
  CHECK(compute.find("gRenderType") != std::string::npos);
  CHECK(compute.find("gGammaCorrection") != std::string::npos);
  CHECK(compute.find("gBackgroundColor") != std::string::npos);
  CHECK(compute.find("gShadingDegree") != std::string::npos);
  CHECK(raster.find("gRenderType") != std::string::npos);
  CHECK(raster.find("gGammaCorrection") != std::string::npos);
  CHECK(raster.find("gBackgroundColor") != std::string::npos);
  CHECK(raster.find("gShadingDegree") != std::string::npos);
}

}  // namespace dxsplat
