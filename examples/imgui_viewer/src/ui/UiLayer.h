#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "app/CameraController.h"
#include "dxsplat/scene.h"
#include "dxsplat/settings.h"
#include "metrics/ImageMetrics.h"

namespace dxsplat {

struct UiActions {
  std::function<void()> openScene;
  std::function<void()> saveScreenshot;
  std::function<void()> nextScene;
  std::function<void()> prevScene;
  std::function<void()> resetView;
  std::function<void()> exitApplication;
};

struct UiFrameData {
  RenderSettings* settings = nullptr;
  int32_t* selectedInputCamera = nullptr;
  uint32_t* renderWidthOverride = nullptr;
  uint32_t* renderHeightOverride = nullptr;
  bool* vsyncEnabled = nullptr;
  bool* paused = nullptr;
  bool* showMetrics = nullptr;
  bool* guiVisible = nullptr;
  bool fullscreen = false;
  std::string gpuName;
  CameraController* camera = nullptr;
  const Scene* scene = nullptr;
  FrameStats* stats = nullptr;
  float fps = 0.0f;
  float frameMs = 0.0f;
  uint32_t renderWidth = 0;
  uint32_t renderHeight = 0;
  bool traversalEnabled = false;
  size_t traversalSceneCount = 0;
  size_t traversalCurrentIndex = 0;
  std::string statusMessage;
  ImageComparison comparison{};
  bool comparisonAvailable = false;
};

class UiLayer {
 public:
  void Render(UiFrameData& frame, UiActions& actions);

 private:
  bool showMetricsPanel_ = true;
};

}  // namespace dxsplat
