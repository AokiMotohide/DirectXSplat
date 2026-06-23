#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "dxsplat/settings.h"

TEST_CASE("PushGraphSample wraps at capacity") {
  dxsplat::GraphSeries series{};
  for (int i = 0; i < 300; ++i) {
    dxsplat::PushGraphSample(series, static_cast<float>(i));
  }

  const std::vector<float> ordered = dxsplat::OrderedGraphSamples(series);
  REQUIRE(series.count == 256u);
  REQUIRE(ordered.size() == 256u);
  CHECK(ordered.front() == doctest::Approx(44.0f));
  CHECK(ordered.back() == doctest::Approx(299.0f));
}

TEST_CASE("Visible graph uses percentage") {
  dxsplat::FrameStats stats{};
  stats.gaussiansVisible = 25;
  stats.gaussiansTotal = 100;

  dxsplat::GraphSeries series{};
  dxsplat::PushGraphSample(series, dxsplat::VisiblePercentageSample(stats));

  const std::vector<float> ordered = dxsplat::OrderedGraphSamples(series);
  REQUIRE(ordered.size() == 1u);
  CHECK(ordered[0] == doctest::Approx(25.0f));
}

TEST_CASE("Visible graph handles zero total") {
  dxsplat::FrameStats stats{};
  stats.gaussiansVisible = 25;
  stats.gaussiansTotal = 0;

  dxsplat::GraphSeries series{};
  dxsplat::PushGraphSample(series, dxsplat::VisiblePercentageSample(stats));

  const std::vector<float> ordered = dxsplat::OrderedGraphSamples(series);
  REQUIRE(ordered.size() == 1u);
  CHECK(ordered[0] == doctest::Approx(0.0f));
}

TEST_CASE("Splat alpha histogram labels are exact") {
  CHECK(std::string(dxsplat::StatisticsGraphTitle(dxsplat::StatisticsGraph::SplatAlphaHistogram)) ==
        "Splat Alpha Histogram");
  CHECK(std::string(dxsplat::StatisticsGraphColumnTitle(dxsplat::StatisticsGraph::SplatAlphaHistogram)) == "Alpha");
  CHECK(std::string(dxsplat::StatisticsGraphRowTitle(dxsplat::StatisticsGraph::SplatAlphaHistogram)) == "Count");
}

TEST_CASE("Projection Active Threads graph labels are exact") {
  CHECK(std::string(dxsplat::StatisticsGraphTitle(dxsplat::StatisticsGraph::ProjectionActiveThreads)) ==
        "Projection Active Threads");
  CHECK(std::string(dxsplat::StatisticsGraphColumnTitle(dxsplat::StatisticsGraph::ProjectionActiveThreads)) == "Threads");
  CHECK(std::string(dxsplat::StatisticsGraphRowTitle(dxsplat::StatisticsGraph::ProjectionActiveThreads)) == "Count");
}

TEST_CASE("Statistics graph titles are exact") {
  CHECK(std::string(dxsplat::StatisticsGraphTitle(dxsplat::StatisticsGraph::Fps)) == "FPS");
  CHECK(std::string(dxsplat::StatisticsGraphTitle(dxsplat::StatisticsGraph::Visible)) == "Visible");
  CHECK(std::string(dxsplat::StatisticsGraphTitle(dxsplat::StatisticsGraph::SplatAlphaHistogram)) ==
        "Splat Alpha Histogram");
  CHECK(std::string(dxsplat::StatisticsGraphTitle(dxsplat::StatisticsGraph::ProjectionActiveThreads)) ==
        "Projection Active Threads");
}
