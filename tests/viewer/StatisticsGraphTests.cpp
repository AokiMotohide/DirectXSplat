#include <doctest/doctest.h>

#include <string>
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

TEST_CASE("Splat alpha histogram labels are exact") {
  CHECK(std::string(directxsplat::StatisticsGraphTitle(directxsplat::StatisticsGraph::SplatAlphaHistogram)) ==
        "Splat Alpha Histogram");
  CHECK(std::string(directxsplat::StatisticsGraphColumnTitle(directxsplat::StatisticsGraph::SplatAlphaHistogram)) == "Alpha");
  CHECK(std::string(directxsplat::StatisticsGraphRowTitle(directxsplat::StatisticsGraph::SplatAlphaHistogram)) == "Count");
}

TEST_CASE("Projection Active Threads graph labels are exact") {
  CHECK(std::string(directxsplat::StatisticsGraphTitle(directxsplat::StatisticsGraph::ProjectionActiveThreads)) ==
        "Projection Active Threads");
  CHECK(std::string(directxsplat::StatisticsGraphColumnTitle(directxsplat::StatisticsGraph::ProjectionActiveThreads)) == "Threads");
  CHECK(std::string(directxsplat::StatisticsGraphRowTitle(directxsplat::StatisticsGraph::ProjectionActiveThreads)) == "Count");
}

TEST_CASE("Statistics graph titles are exact") {
  CHECK(std::string(directxsplat::StatisticsGraphTitle(directxsplat::StatisticsGraph::Fps)) == "FPS");
  CHECK(std::string(directxsplat::StatisticsGraphTitle(directxsplat::StatisticsGraph::Visible)) == "Visible Points (%)");
  CHECK(std::string(directxsplat::StatisticsGraphTitle(directxsplat::StatisticsGraph::SplatAlphaHistogram)) ==
        "Splat Alpha Histogram");
  CHECK(std::string(directxsplat::StatisticsGraphTitle(directxsplat::StatisticsGraph::ProjectionActiveThreads)) ==
        "Projection Active Threads");
}

TEST_CASE("Line graph labels match reference labels") {
  CHECK(std::string(directxsplat::StatisticsGraphColumnTitle(directxsplat::StatisticsGraph::Fps)) == "Time");
  CHECK(std::string(directxsplat::StatisticsGraphRowTitle(directxsplat::StatisticsGraph::Fps)) == "FPS");
  CHECK(std::string(directxsplat::StatisticsGraphColumnTitle(directxsplat::StatisticsGraph::Visible)) == "Time");
  CHECK(std::string(directxsplat::StatisticsGraphRowTitle(directxsplat::StatisticsGraph::Visible)) == "Visible (%)");
}
