#include <doctest/doctest.h>

#include <vector>

#include "directxsplat/settings.h"

TEST_CASE("PushGraphSample wraps at capacity") {
  directxsplat::GraphSeries series{};
  for (int i = 0; i < 300; ++i) {
    directxsplat::PushGraphSample(series, static_cast<float>(i));
  }

  const std::vector<float> ordered = directxsplat::OrderedGraphSamples(series);
  REQUIRE(series.count == 256u);
  REQUIRE(ordered.size() == 256u);
  CHECK(ordered.front() == doctest::Approx(44.0f));
  CHECK(ordered.back() == doctest::Approx(299.0f));
}

TEST_CASE("Visible graph uses percentage") {
  directxsplat::FrameStats stats{};
  stats.gaussiansVisible = 25;
  stats.gaussiansTotal = 100;

  directxsplat::GraphSeries series{};
  directxsplat::PushGraphSample(series, directxsplat::VisiblePercentageSample(stats));

  const std::vector<float> ordered = directxsplat::OrderedGraphSamples(series);
  REQUIRE(ordered.size() == 1u);
  CHECK(ordered[0] == doctest::Approx(25.0f));
}

TEST_CASE("Visible graph handles zero total") {
  directxsplat::FrameStats stats{};
  stats.gaussiansVisible = 25;
  stats.gaussiansTotal = 0;

  directxsplat::GraphSeries series{};
  directxsplat::PushGraphSample(series, directxsplat::VisiblePercentageSample(stats));

  const std::vector<float> ordered = directxsplat::OrderedGraphSamples(series);
  REQUIRE(ordered.size() == 1u);
  CHECK(ordered[0] == doctest::Approx(0.0f));
}
