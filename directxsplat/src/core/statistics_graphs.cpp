#include "dxsplat/settings.h"

#include <algorithm>
#include <cmath>

namespace dxsplat {
namespace {

constexpr size_t kGraphCapacity = 256;
constexpr size_t kHistogramCapacity = 64;
constexpr float kProjectionThreadMax = 256.0f;

size_t ClampedBinCount(size_t binCount) {
  return std::clamp<size_t>(binCount, 1, kHistogramCapacity);
}

float AlphaFromOpacity(float opacity) {
  const float clamped = std::clamp(opacity, -20.0f, 20.0f);
  return 1.0f / (1.0f + std::exp(-clamped));
}

void AddHistogramValue(HistogramData& histogram, float value, float count, size_t binCount) {
  const float range = std::max(histogram.maxValue - histogram.minValue, 1e-6f);
  const float unit = std::clamp((value - histogram.minValue) / range, 0.0f, 1.0f);
  const size_t bin = std::min(static_cast<size_t>(unit * static_cast<float>(binCount)), binCount - 1u);
  histogram.bins[bin] += count;
}

HistogramData RebinHistogram(const HistogramData& source, size_t binCount, float minValue, float maxValue) {
  HistogramData out{};
  out.minValue = minValue;
  out.maxValue = maxValue;
  binCount = ClampedBinCount(binCount);

  const float sourceRange = std::max(source.maxValue - source.minValue, 1e-6f);
  for (size_t i = 0; i < source.bins.size(); ++i) {
    if (source.bins[i] <= 0.0f) {
      continue;
    }
    const float unit = (static_cast<float>(i) + 0.5f) / static_cast<float>(source.bins.size());
    const float value = source.minValue + unit * sourceRange;
    AddHistogramValue(out, value, source.bins[i], binCount);
  }
  return out;
}

}  // namespace

void PushGraphSample(GraphSeries& series, float value) {
  series.values[series.head] = value;
  series.head = (series.head + 1u) % kGraphCapacity;
  series.count = std::min(series.count + 1u, kGraphCapacity);
}

std::vector<float> OrderedGraphSamples(const GraphSeries& series) {
  std::vector<float> out;
  out.reserve(series.count);
  const size_t start = series.count == kGraphCapacity ? series.head : 0u;
  for (size_t i = 0; i < series.count; ++i) {
    out.push_back(series.values[(start + i) % kGraphCapacity]);
  }
  return out;
}

float VisiblePercentageSample(const FrameStats& stats) {
  if (stats.gaussiansTotal == 0) {
    return 0.0f;
  }
  return static_cast<float>(static_cast<double>(stats.gaussiansVisible) * 100.0 /
                            static_cast<double>(stats.gaussiansTotal));
}

HistogramData BuildSplatAlphaHistogram(const Scene& scene, size_t binCount) {
  HistogramData histogram{};
  histogram.minValue = 0.0f;
  histogram.maxValue = 1.0f;
  binCount = ClampedBinCount(binCount);

  // Convert loader opacity logits to renderer alpha
  for (const GaussianSet& set : scene.splatSets) {
    for (const Gaussian& gaussian : set.gaussians) {
      AddHistogramValue(histogram, AlphaFromOpacity(gaussian.opacity), 1.0f, binCount);
    }
  }
  return histogram;
}

HistogramData BuildProjectionActiveThreadsHistogram(const FrameStats& stats, size_t binCount) {
  const float sourceMax = stats.projectionActiveThreads.maxValue > stats.projectionActiveThreads.minValue
                              ? stats.projectionActiveThreads.maxValue
                              : kProjectionThreadMax;
  return RebinHistogram(stats.projectionActiveThreads, binCount, 0.0f, sourceMax);
}

const char* StatisticsGraphTitle(StatisticsGraph graph) {
  switch (graph) {
    case StatisticsGraph::Fps:
      return "FPS";
    case StatisticsGraph::Visible:
      return "Visible Points (%)";
    case StatisticsGraph::SplatAlphaHistogram:
      return "Splat Alpha Histogram";
    case StatisticsGraph::ProjectionActiveThreads:
      return "Projection Active Threads";
    default:
      return "";
  }
}

const char* StatisticsGraphColumnTitle(StatisticsGraph graph) {
  switch (graph) {
    case StatisticsGraph::Fps:
      return "Time";
    case StatisticsGraph::Visible:
      return "Time";
    case StatisticsGraph::SplatAlphaHistogram:
      return "Alpha";
    case StatisticsGraph::ProjectionActiveThreads:
      return "Threads";
    default:
      return "";
  }
}

const char* StatisticsGraphRowTitle(StatisticsGraph graph) {
  switch (graph) {
    case StatisticsGraph::Fps:
      return "FPS";
    case StatisticsGraph::Visible:
      return "Visible (%)";
    case StatisticsGraph::SplatAlphaHistogram:
    case StatisticsGraph::ProjectionActiveThreads:
      return "Count";
    default:
      return "";
  }
}

}  // namespace dxsplat
