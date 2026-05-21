#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

#include "appcommon/image.h"
#include "metrics/ImageMetrics.h"
#include "tools/CliOptions.h"

namespace dxsplat {
namespace {

appcommon::ImageRgba8 MakeImage(uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels) {
  appcommon::ImageRgba8 image{};
  image.width = width;
  image.height = height;
  image.pixels = pixels;
  return image;
}

}

TEST_CASE("CLI parser covers option and positional matrix") {
  auto parsed = ParseCliOptions({"--help"});
  REQUIRE(parsed.ok());
  CHECK(parsed.value.showHelp);
  CHECK_FALSE(parsed.value.scenePath.has_value());

  parsed = ParseCliOptions({"--images-path", "images", "--scene-folder", "scenes", "--render-size", "320x240",
                            "botanical", "garden.ply"});
  REQUIRE(parsed.ok());
  REQUIRE(parsed.value.imagePathOverride.has_value());
  REQUIRE(parsed.value.folderTraversalPath.has_value());
  REQUIRE(parsed.value.renderWidthOverride.has_value());
  REQUIRE(parsed.value.renderHeightOverride.has_value());
  REQUIRE(parsed.value.scenePath.has_value());
  CHECK(*parsed.value.imagePathOverride == "images");
  CHECK(*parsed.value.folderTraversalPath == "scenes");
  CHECK(*parsed.value.renderWidthOverride == 320u);
  CHECK(*parsed.value.renderHeightOverride == 240u);
  CHECK(*parsed.value.scenePath == "botanical garden.ply");

  parsed = ParseCliOptions({"--render-size", "320"});
  CHECK_FALSE(parsed.ok());
  parsed = ParseCliOptions({"--render-size", "0x240"});
  CHECK_FALSE(parsed.ok());
  parsed = ParseCliOptions({"--force-aspect", "1.777"});
  CHECK_FALSE(parsed.ok());
  parsed = ParseCliOptions({"--images-path"});
  CHECK_FALSE(parsed.ok());
}

TEST_CASE("image metrics match deterministic reference values") {
  const appcommon::ImageRgba8 a = MakeImage(2u, 1u, {
      0u, 0u, 0u, 255u,
      255u, 255u, 255u, 255u,
  });
  const appcommon::ImageRgba8 b = MakeImage(2u, 1u, {
      0u, 0u, 0u, 255u,
      255u, 0u, 255u, 255u,
  });

  const ImageComparison same = CompareImages(a, a);
  CHECK(same.mae == doctest::Approx(0.0));
  CHECK(same.mse == doctest::Approx(0.0));
  CHECK(same.psnr == doctest::Approx(120.0));
  CHECK(same.flipLike == doctest::Approx(0.0));

  const ImageComparison diff = CompareImages(a, b);
  CHECK(diff.mae == doctest::Approx(1.0 / 6.0));
  CHECK(diff.mse == doctest::Approx(1.0 / 6.0));
  CHECK(diff.psnr == doctest::Approx(10.0 * std::log10(6.0)));
  CHECK(diff.flipLike == doctest::Approx(1.0 / 6.0));

  const appcommon::ImageRgba8 diffImage = BuildDiffImage(a, b);
  REQUIRE(diffImage.width == 2u);
  REQUIRE(diffImage.height == 1u);
  REQUIRE(diffImage.pixels.size() == 8u);
  CHECK(diffImage.pixels[0] == 0u);
  CHECK(diffImage.pixels[1] == 0u);
  CHECK(diffImage.pixels[2] == 0u);
  CHECK(diffImage.pixels[3] == 255u);
  CHECK(diffImage.pixels[4] == 0u);
  CHECK(diffImage.pixels[5] == 255u);
  CHECK(diffImage.pixels[6] == 0u);
  CHECK(diffImage.pixels[7] == 255u);

  const appcommon::ImageRgba8 wrongSize = MakeImage(1u, 1u, {0u, 0u, 0u, 255u});
  const ImageComparison invalid = CompareImages(a, wrongSize);
  CHECK(std::isinf(invalid.mae));
  CHECK(std::isinf(invalid.mse));
  CHECK(std::isinf(invalid.flipLike));
  CHECK(invalid.psnr == doctest::Approx(0.0));
  CHECK(BuildDiffImage(a, wrongSize).Empty());
}

}  // namespace dxsplat
