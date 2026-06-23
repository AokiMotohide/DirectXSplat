#include <doctest/doctest.h>

#include "dxsplat/settings.h"

TEST_CASE("Splat alpha histogram converts loaded opacity") {
  dxsplat::Scene scene{};
  dxsplat::GaussianSet set{};
  dxsplat::Gaussian gaussian{};

  gaussian.opacity = -20.0f;
  set.gaussians.push_back(gaussian);
  gaussian.opacity = 0.0f;
  set.gaussians.push_back(gaussian);
  gaussian.opacity = 20.0f;
  set.gaussians.push_back(gaussian);
  scene.splatSets.push_back(set);

  const dxsplat::HistogramData histogram = dxsplat::BuildSplatAlphaHistogram(scene, 64);

  CHECK(histogram.minValue == doctest::Approx(0.0f));
  CHECK(histogram.maxValue == doctest::Approx(1.0f));
  CHECK(histogram.bins[0] == doctest::Approx(1.0f));
  CHECK(histogram.bins[32] == doctest::Approx(1.0f));
  CHECK(histogram.bins[63] == doctest::Approx(1.0f));
}

TEST_CASE("Projection Active Threads histogram preserves renderer bins") {
  dxsplat::FrameStats stats{};
  stats.projectionActiveThreads.minValue = 0.0f;
  stats.projectionActiveThreads.maxValue = 256.0f;
  stats.projectionActiveThreads.bins[0] = 2.0f;
  stats.projectionActiveThreads.bins[63] = 3.0f;

  const dxsplat::HistogramData histogram = dxsplat::BuildProjectionActiveThreadsHistogram(stats, 64);

  CHECK(histogram.minValue == doctest::Approx(0.0f));
  CHECK(histogram.maxValue == doctest::Approx(256.0f));
  CHECK(histogram.bins[0] == doctest::Approx(2.0f));
  CHECK(histogram.bins[63] == doctest::Approx(3.0f));
}
