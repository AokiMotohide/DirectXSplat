#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "directxsplat/directxsplat.h"
#include "directxsplat/math.h"
#include "directxsplat/scene.h"
#include "directxsplat/sort.h"
#include "directxsplat/types.h"

namespace directxsplat {

constexpr ShadingDegree SanitizeShadingDegree(ShadingDegree degree) {
  switch (degree) {
    case ShadingDegree::Dc:
    case ShadingDegree::Degree1:
    case ShadingDegree::Degree2:
    case ShadingDegree::Degree3:
      return degree;
    default:
      return ShadingDegree::Degree3;
  }
}

constexpr RenderType SanitizeRenderType(RenderType type) {
  switch (type) {
    case RenderType::Color:
    case RenderType::Alpha:
    case RenderType::Depth:
      return type;
    default:
      return RenderType::Color;
  }
}

constexpr const char* RenderTypeLabel(RenderType type) {
  switch (SanitizeRenderType(type)) {
    case RenderType::Alpha:
      return "alpha";
    case RenderType::Depth:
      return "depth";
    case RenderType::Color:
    default:
      return "color";
  }
}

struct RenderSettings {
  bool antialiasing = true;
  float antialiasingStrength = 1.0f;
  bool fastCulling = true;
  float frustumDilation = 0.05f;
  float gaussianScalingModifier = 1.0f;
  bool outputDepth = false;
  RenderType renderType = RenderType::Color;
  Vec3 backgroundColor{0.0f, 0.0f, 0.0f};
  bool gammaCorrection = false;
  ShadingDegree shadingDegree = ShadingDegree::Degree3;
  bool positiveViewSpaceZ = true;
  float maxAxisPixels = 256.0f;
  uint64_t splatBudget = 0;
  float lodHysteresis = 0.15f;
};

enum class StatisticsGraph {
  Fps,
  Visible,
  SplatAlphaHistogram,
  ProjectionActiveThreads,
};

struct GraphSeries {
  std::array<float, 256> values{};
  size_t count = 0;
  size_t head = 0;
};

constexpr size_t kSplatAlphaHistogramBins = 50;
constexpr size_t kProjectionActiveThreadHistogramBins = 64;

struct HistogramData {
  std::array<float, 64> bins{};
  float minValue = 0.0f;
  float maxValue = 1.0f;
};

struct ViewerGraphData {
  GraphSeries fps;
  GraphSeries visible;
  HistogramData splatAlpha;
  HistogramData projectionActiveThreads;
};

struct FrameStats {
  uint64_t gaussiansVisible = 0;
  uint64_t gaussiansTotal = 0;
  uint64_t residentGaussians = 0;
  uint64_t residentChunks = 0;
  uint64_t splatBudget = 0;
  uint64_t streamedUploads = 0;
  uint64_t streamedEvictions = 0;
  uint64_t lod0Chunks = 0;
  uint64_t lod1Chunks = 0;
  uint64_t lod2Chunks = 0;
  uint64_t sortPasses = 0;
  SortBackend sortBackend = SortBackend::OneSweep;
  float gpuPrepareMs = 0.0f;
  float gpuSortMs = 0.0f;
  float gpuRasterMs = 0.0f;
  float gpuDepthMs = 0.0f;
  float gpuMs = 0.0f;
  float cpuMs = 0.0f;
  HistogramData splatAlpha{};
  HistogramData projectionActiveThreads{};
};

struct ProjectionLightInput {
  Mat4 viewProj{};
  Vec3 position{};
  Vec3 colorTint{1.0f, 1.0f, 1.0f};
  float lumens = 0.0f;
  float blackLevel = 0.0f;
  float solidAngle = 1.0f;
  float contrastRatio = 1000.0f;
  uint32_t inputTransferFunction = 0;
  float inputGamma = 2.2f;
  bool inputTextureHardwareDecoded = false;
  bool radiometricProfileEnabled = false;
  float whiteLevel = 1.0f;
  float spatialUniformity = 1.0f;
  float shadowBias = 0.001f;
  uint32_t shadowSlice = 0;
  bool shadowReady = false;
  bool enabled = false;
};

struct RenderInput {
  Mat4 view{};
  Mat4 proj{};
  Mat4 model{};
  Vec3 cameraPosition{};
  Vec3 worldCameraPosition{};
  ProjectionLightInput projectionLight{};
  bool approximateRelighting = false;
  bool physicalRelighting = false;
  float environmentIntensity = 1.0f;
  float bakedRelightingMix = 0.65f;
  RenderSettings settings{};
  uint32_t viewportWidth = 1;
  uint32_t viewportHeight = 1;
  float nearPlane = 0.1f;
  float farPlane = 5000.0f;
  Vec2 jitter{};
  bool cameraCut = false;
  uint64_t frameIndex = 0;
};

void PushGraphSample(GraphSeries& series, float value);
std::vector<float> OrderedGraphSamples(const GraphSeries& series);
float VisiblePercentageSample(const FrameStats& stats);
HistogramData BuildSplatAlphaHistogram(const FrameStats& stats, size_t binCount);
HistogramData BuildProjectionActiveThreadsHistogram(const FrameStats& stats, size_t binCount);
const char* StatisticsGraphTitle(StatisticsGraph graph);
const char* StatisticsGraphColumnTitle(StatisticsGraph graph);
const char* StatisticsGraphRowTitle(StatisticsGraph graph);

}  // namespace directxsplat
