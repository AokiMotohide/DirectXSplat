#include <doctest/doctest.h>

#include <array>
#include <string>

#include "ui/UiLayer.h"

TEST_CASE("FormatPinnedVisible formats percentage") {
  CHECK(dxsplat::FormatPinnedVisible(25, 100) == "visible      : 25 splats (25.00%)");
}

TEST_CASE("FormatPinnedVisible handles zero total") {
  CHECK(dxsplat::FormatPinnedVisible(0, 0) == "visible      : 0 splats (0.00%)");
}

TEST_CASE("Pinned stats formatting matches compact metrics") {
  CHECK(dxsplat::FormatPinnedFps(60.0f) == "fps          : 60.000");
  CHECK(dxsplat::FormatPinnedSize(1600, 900) == "size         : 1600 x 900");
  CHECK(dxsplat::FormatPinnedSplats(42) == "splats       : 42");
}

TEST_CASE("UiLayer exposes stable section labels") {
  const std::array<const char*, 5> labels = dxsplat::UiSectionLabels();

  CHECK(std::string(labels[0]) == "Graphic");
  CHECK(std::string(labels[1]) == "Scene");
  CHECK(std::string(labels[2]) == "Camera");
  CHECK(std::string(labels[3]) == "Animation");
  CHECK(std::string(labels[4]) == "Statistics");
}

TEST_CASE("UiLayer exposes stable Graphic labels") {
  const std::array<const char*, 5> labels = dxsplat::UiGraphicLabels();

  CHECK(std::string(labels[0]) == "VSync");
  CHECK(std::string(labels[1]) == "Fast culling");
  CHECK(std::string(labels[2]) == "Gamma correction");
  CHECK(std::string(labels[3]) == "AA");
  CHECK(std::string(labels[4]) == "aa");
}

TEST_CASE("UiLayer exposes stable Scene labels") {
  const std::array<const char*, 11> labels = dxsplat::UiSceneLabels();

  CHECK(std::string(labels[0]) == "Render type");
  CHECK(std::string(labels[1]) == "color");
  CHECK(std::string(labels[2]) == "alpha");
  CHECK(std::string(labels[3]) == "depth");
  CHECK(std::string(labels[4]) == "Background");
  CHECK(std::string(labels[5]) == "R");
  CHECK(std::string(labels[6]) == "G");
  CHECK(std::string(labels[7]) == "B");
  CHECK(std::string(labels[8]) == "scale");
  CHECK(std::string(labels[9]) == "projection");
  CHECK(std::string(labels[10]) == "dilation");
}
