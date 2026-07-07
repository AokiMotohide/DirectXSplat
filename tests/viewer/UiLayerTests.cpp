#include <doctest/doctest.h>

#include "ui/UiLayer.h"

TEST_CASE("FormatPinnedVisible formats percentage") {
  CHECK(directxsplat::FormatPinnedVisible(25, 100) == "visible      : 25 splats (25.00%)");
}

TEST_CASE("FormatPinnedVisible handles zero total") {
  CHECK(directxsplat::FormatPinnedVisible(0, 0) == "visible      : 0 splats (0.00%)");
}

TEST_CASE("Pinned stats formatting matches compact metrics") {
  CHECK(directxsplat::FormatPinnedFps(60.0f) == "fps          : 60.000");
  CHECK(directxsplat::FormatPinnedSize(1600, 900) == "size         : 1600 x 900");
  CHECK(directxsplat::FormatPinnedSplats(42) == "splats       : 42");
}
