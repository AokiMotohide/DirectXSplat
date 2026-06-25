#include <doctest/doctest.h>

#include "dxsplat/settings.h"

TEST_CASE("Splat alpha histogram preserves projected frame bins") {
  directxsplat::FrameStats stats{};
  stats.splatAlpha.minValue = 0.0f;
  stats.splatAlpha.maxValue = 1.0f;
  stats.splatAlpha.bins[0] = 1.0f;
  stats.splatAlpha.bins[25] = 2.0f;
  stats.splatAlpha.bins[49] = 3.0f;

  const directxsplat::HistogramData histogram = directxsplat::BuildSplatAlphaHistogram(stats, 50);

  CHECK(histogram.minValue == doctest::Approx(0.0f));
  CHECK(histogram.maxValue == doctest::Approx(1.0f));
  CHECK(histogram.bins[0] == doctest::Approx(1.0f));
  CHECK(histogram.bins[25] == doctest::Approx(2.0f));
  CHECK(histogram.bins[49] == doctest::Approx(3.0f));
}

TEST_CASE("Projection Active Threads histogram preserves renderer bins") {
  directxsplat::FrameStats stats{};
  stats.projectionActiveThreads.minValue = 0.0f;
  stats.projectionActiveThreads.maxValue = 64.0f;
  stats.projectionActiveThreads.bins[0] = 2.0f;
  stats.projectionActiveThreads.bins[63] = 3.0f;

  const directxsplat::HistogramData histogram = directxsplat::BuildProjectionActiveThreadsHistogram(stats, 64);

  CHECK(histogram.minValue == doctest::Approx(0.0f));
  CHECK(histogram.maxValue == doctest::Approx(64.0f));
  CHECK(histogram.bins[0] == doctest::Approx(2.0f));
  CHECK(histogram.bins[63] == doctest::Approx(3.0f));
}
