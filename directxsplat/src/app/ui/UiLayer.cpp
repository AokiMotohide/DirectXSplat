#include "ui/UiLayer.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace dxsplat {

std::string FormatPinnedFps(float fps) {
  char buffer[96]{};
  std::snprintf(buffer, sizeof(buffer), "%-13s: %.3f", "fps", fps);
  return buffer;
}

std::string FormatPinnedSize(uint32_t width, uint32_t height) {
  char buffer[96]{};
  std::snprintf(buffer, sizeof(buffer), "%-13s: %u x %u", "size", width, height);
  return buffer;
}

std::string FormatPinnedSplats(uint64_t total) {
  char buffer[96]{};
  std::snprintf(buffer, sizeof(buffer), "%-13s: %llu", "splats", static_cast<unsigned long long>(total));
  return buffer;
}

std::string FormatPinnedVisible(uint64_t visible, uint64_t total) {
  const double percent = total > 0 ? static_cast<double>(visible) * 100.0 / static_cast<double>(total) : 0.0;
  char buffer[128]{};
  std::snprintf(buffer,
                sizeof(buffer),
                "%-13s: %llu splats (%.2f%%)",
                "visible",
                static_cast<unsigned long long>(visible),
                percent);
  return buffer;
}

std::array<const char*, 5> UiSectionLabels() {
  return {"Graphic", "Scene", "Camera", "Animation", "Statistics"};
}

std::array<const char*, 5> UiGraphicLabels() {
  return {"VSync", "Fast culling", "Gamma correction", "AA", "aa"};
}

std::array<const char*, 11> UiSceneLabels() {
  return {"Render type", "color", "alpha", "depth", "Background", "R", "G", "B", "scale", "projection", "dilation"};
}

namespace {

constexpr float kRadToDeg = 57.295779513f;
constexpr float kDegToRad = 0.0174532925199f;
constexpr float kControlsWidth = 292.0f;
constexpr float kStatsMaxWidth = 300.0f;

uint64_t VisibleSplats(const UiFrameData& frame) {
  return frame.stats != nullptr ? frame.stats->gaussiansVisible : 0;
}

int NavigatorModeToIndex(NavigatorMode mode) {
  switch (mode) {
    case NavigatorMode::Orbit:
    case NavigatorMode::Trackball:
      return 1;
    case NavigatorMode::Fps:
    default:
      return 0;
  }
}

NavigatorMode NavigatorModeFromIndex(int index) {
  switch (index) {
    case 1:
      return NavigatorMode::Orbit;
    case 0:
    default:
      return NavigatorMode::Fps;
  }
}

int FindClosestInputCamera(const Scene& scene, const Vec3& position) {
  int best = -1;
  float bestDistance = 3.402823466e+38f;
  for (int i = 0; i < static_cast<int>(scene.inputCameras.size()); ++i) {
    const Vec3 delta = scene.inputCameras[static_cast<size_t>(i)].position - position;
    const float distance = Dot(delta, delta);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = i;
    }
  }
  return best;
}

uint64_t CountSceneSplats(const Scene* scene) {
  if (scene == nullptr) {
    return 0;
  }
  uint64_t total = 0;
  for (const GaussianSet& set : scene->splatSets) {
    total += static_cast<uint64_t>(set.gaussians.size());
  }
  return total;
}

uint64_t TotalSplats(const UiFrameData& frame) {
  const uint64_t sceneSplats = CountSceneSplats(frame.scene);
  return frame.stats != nullptr && frame.stats->gaussiansTotal > 0 ? frame.stats->gaussiansTotal : sceneSplats;
}

void RenderTraversalControls(UiFrameData& frame, UiActions& actions);

void MetricU64(const char* label, uint64_t value, const char* suffix = "") {
  ImGui::Text("%-13s: %llu%s", label, static_cast<unsigned long long>(value), suffix);
}

void MetricFloat(const char* label, float value, const char* suffix = "") {
  ImGui::Text("%-13s: %.3f%s", label, value, suffix);
}

void MetricTime(const char* label, float value, float total) {
  const float pct = total > 0.0f ? (value / total) * 100.0f : 0.0f;
  ImGui::Text("%-13s: %.3fms %.1f%%", label, value, pct);
}

void CompactSeparator() {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

void RenderActionButtons(UiActions& actions) {
  if (ImGui::Button("open") && actions.openScene) {
    actions.openScene();
  }
  ImGui::SameLine();
  if (ImGui::Button("shot") && actions.saveScreenshot) {
    actions.saveScreenshot();
  }
  ImGui::SameLine();
  if (ImGui::Button("reset") && actions.resetView) {
    actions.resetView();
  }
  ImGui::SameLine();
  if (ImGui::Button("quit") && actions.exitApplication) {
    actions.exitApplication();
  }
}

void RenderStats(const UiFrameData& frame) {
  const FrameStats* stats = frame.stats;
  const uint64_t totalSplats = TotalSplats(frame);
  const uint64_t residentSplats =
      stats != nullptr && stats->residentGaussians > 0 ? stats->residentGaussians : totalSplats;
  if (!frame.gpuName.empty()) {
    ImGui::TextUnformatted(frame.gpuName.c_str());
  }
  MetricU64("total splats", totalSplats);
  MetricU64("loaded splats", residentSplats);

  if (stats != nullptr) {
    if (stats->gaussiansTotal > 0) {
      const float visiblePct = static_cast<float>(stats->gaussiansVisible) * 100.0f / static_cast<float>(stats->gaussiansTotal);
      ImGui::Text("%-13s: %llu splats (%.2f%%)", "visible", static_cast<unsigned long long>(stats->gaussiansVisible), visiblePct);
    } else {
      MetricU64("visible", stats->gaussiansVisible, " splats");
    }
    if (frame.renderWidth > 0 && frame.renderHeight > 0) {
      ImGui::Text("%-13s: %u x %u", "size", frame.renderWidth, frame.renderHeight);
    }
    MetricFloat("fps", frame.fps);
    MetricFloat("frame e2e", frame.frameMs, "ms");
    if (stats->gpuMs > 0.0f) {
      MetricFloat("gpu total", stats->gpuMs, "ms");
      MetricTime("prepare", stats->gpuPrepareMs, stats->gpuMs);
      MetricTime("sort", stats->gpuSortMs, stats->gpuMs);
      MetricTime("raster", stats->gpuRasterMs, stats->gpuMs);
      if (stats->gpuDepthMs > 0.0f) {
        MetricTime("depth", stats->gpuDepthMs, stats->gpuMs);
      }
    }
    if (stats->cpuMs > 0.0f) {
      MetricFloat("cpu", stats->cpuMs, "ms");
    }
    if (stats->residentChunks > 0) {
      MetricU64("chunks", stats->residentChunks);
    }
    if (stats->streamedUploads > 0 || stats->streamedEvictions > 0) {
      ImGui::Text("%-13s: +%llu / -%llu", "streaming", static_cast<unsigned long long>(stats->streamedUploads),
                  static_cast<unsigned long long>(stats->streamedEvictions));
    }
  }

  if (frame.comparisonAvailable) {
    MetricFloat("mae", static_cast<float>(frame.comparison.mae));
    MetricFloat("mse", static_cast<float>(frame.comparison.mse));
    MetricFloat("psnr", static_cast<float>(frame.comparison.psnr));
    MetricFloat("flip", static_cast<float>(frame.comparison.flipLike));
  }
}

void RenderLineGraph(const char* title, const char* columnTitle, const char* rowTitle, float value, float scaleMax) {
  ImGui::TextUnformatted(title);
  ImGui::TextUnformatted(rowTitle);
  const float values[2] = {value, value};
  const std::string id = std::string("##") + title;
  ImGui::PlotLines(id.c_str(), values, 2, 0, nullptr, 0.0f, std::max(scaleMax, 1.0f), ImVec2(250.0f, 42.0f));
  ImGui::TextUnformatted(columnTitle);
}

void RenderHistogramGraph(const char* title, const char* columnTitle, const char* rowTitle) {
  ImGui::TextUnformatted(title);
  ImGui::TextUnformatted(rowTitle);
  std::array<float, 64> values{};
  const std::string id = std::string("##") + title;
  ImGui::PlotHistogram(id.c_str(), values.data(), static_cast<int>(values.size()), 0, nullptr, 0.0f, 1.0f, ImVec2(250.0f, 42.0f));
  ImGui::TextUnformatted(columnTitle);
}

void RenderStatisticsGraphs(const UiFrameData& frame) {
  const uint64_t total = TotalSplats(frame);
  const uint64_t visible = VisibleSplats(frame);
  const float visiblePct = total > 0 ? static_cast<float>(visible) * 100.0f / static_cast<float>(total) : 0.0f;

  CompactSeparator();
  RenderLineGraph("FPS", "FPS", "FPS", frame.fps, std::max(frame.fps, 120.0f));
  RenderLineGraph("Visible", "Visible", "Visible", visiblePct, 100.0f);
  RenderHistogramGraph("Splat Alpha Histogram", "Alpha", "Count");
  RenderHistogramGraph("Projection Active Threads", "Threads", "Count");
}

void RenderGraphicSection(UiFrameData& frame) {
  const auto labels = UiGraphicLabels();
  if (frame.vsyncEnabled != nullptr) {
    ImGui::Checkbox(labels[0], frame.vsyncEnabled);
  }
  ImGui::Checkbox(labels[1], &frame.settings->fastCulling);
  ImGui::Checkbox(labels[2], &frame.settings->gammaCorrection);
  ImGui::Checkbox(labels[3], &frame.settings->antialiasing);
  ImGui::SetNextItemWidth(128.0f);
  ImGui::SliderFloat("##aastrength", &frame.settings->antialiasingStrength, 0.0f, 2.5f, "%.2f");
  ImGui::SameLine();
  ImGui::TextUnformatted(labels[4]);
}

void RenderSceneSection(UiFrameData& frame, UiActions& actions) {
  const auto labels = UiSceneLabels();
  RenderType renderType = SanitizeRenderType(frame.settings->renderType);
  ImGui::SetNextItemWidth(128.0f);
  if (ImGui::BeginCombo("##rendertype", RenderTypeLabel(renderType))) {
    const RenderType options[] = {RenderType::Color, RenderType::Alpha, RenderType::Depth};
    for (RenderType option : options) {
      const bool selected = renderType == option;
      if (ImGui::Selectable(RenderTypeLabel(option), selected)) {
        renderType = option;
        frame.settings->renderType = option;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(labels[0]);

  float background[3] = {
      frame.settings->backgroundColor.x,
      frame.settings->backgroundColor.y,
      frame.settings->backgroundColor.z,
  };
  ImGui::SetNextItemWidth(176.0f);
  if (ImGui::ColorEdit3(labels[4], background, ImGuiColorEditFlags_Float)) {
    frame.settings->backgroundColor = {background[0], background[1], background[2]};
  }

  ImGui::SetNextItemWidth(128.0f);
  ImGui::SliderFloat("##scalemod", &frame.settings->gaussianScalingModifier, 0.001f, 2.0f, "%.3f");
  ImGui::SameLine();
  ImGui::TextUnformatted(labels[8]);
  ImGui::SetNextItemWidth(128.0f);
  ImGui::SliderFloat("##maxaxispixels", &frame.settings->maxAxisPixels, 1.0f, 512.0f, "%.0f");
  ImGui::SameLine();
  ImGui::TextUnformatted(labels[9]);
  ImGui::SetNextItemWidth(128.0f);
  ImGui::SliderFloat("##frustumdilation", &frame.settings->frustumDilation, 0.0f, 0.25f, "%.3f");
  ImGui::SameLine();
  ImGui::TextUnformatted(labels[10]);
  RenderTraversalControls(frame, actions);
}

void RenderTraversalControls(UiFrameData& frame, UiActions& actions) {
  if (!frame.traversalEnabled || !actions.prevScene || !actions.nextScene) {
    return;
  }
  if (ImGui::Button("prev")) {
    actions.prevScene();
  }
  ImGui::SameLine();
  if (ImGui::Button("next")) {
    actions.nextScene();
  }
  ImGui::SameLine();
  ImGui::Text("%llu / %llu", static_cast<unsigned long long>(frame.traversalCurrentIndex + 1),
              static_cast<unsigned long long>(frame.traversalSceneCount));
}

void RenderCameraSnapControls(UiFrameData& frame) {
  if (frame.scene == nullptr || frame.camera == nullptr || frame.selectedInputCamera == nullptr) {
    return;
  }
  if (frame.scene->inputCameras.empty()) {
    return;
  }

  *frame.selectedInputCamera =
      std::clamp(*frame.selectedInputCamera, 0, static_cast<int32_t>(frame.scene->inputCameras.size() - 1));

  const char* preview = frame.scene->inputCameras[static_cast<size_t>(*frame.selectedInputCamera)].name.c_str();
  ImGui::SetNextItemWidth(180.0f);
  if (ImGui::BeginCombo("##inputcamera", preview)) {
    for (int i = 0; i < static_cast<int>(frame.scene->inputCameras.size()); ++i) {
      const bool selected = i == *frame.selectedInputCamera;
      if (ImGui::Selectable(frame.scene->inputCameras[static_cast<size_t>(i)].name.c_str(), selected)) {
        *frame.selectedInputCamera = i;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::TextUnformatted("camera");

  if (ImGui::Button("snap")) {
    frame.camera->SnapToInputCamera(frame.scene->inputCameras[static_cast<size_t>(*frame.selectedInputCamera)]);
  }
  ImGui::SameLine();
  if (ImGui::Button("closest")) {
    const int closest = FindClosestInputCamera(*frame.scene, frame.camera->State().position);
    if (closest >= 0) {
      *frame.selectedInputCamera = closest;
      frame.camera->SnapToInputCamera(frame.scene->inputCameras[static_cast<size_t>(closest)]);
    }
  }
}

void RenderCameraControls(UiFrameData& frame) {
  CameraState state = frame.camera->State();
  bool changed = false;

  const int navigatorIndex = NavigatorModeToIndex(state.navigatorMode);
  if (ImGui::RadioButton("FPS", navigatorIndex == 0)) {
    state.navigatorMode = NavigatorModeFromIndex(0);
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Orbit", navigatorIndex == 1)) {
    state.navigatorMode = NavigatorModeFromIndex(1);
    changed = true;
  }

  float fovDeg = state.fovYRadians * kRadToDeg;
  ImGui::SetNextItemWidth(174.0f);
  if (ImGui::SliderFloat("##fov", &fovDeg, 1.0f, 120.0f, "%.3f")) {
    state.fovYRadians = std::clamp(fovDeg, 1.0f, 180.0f) * kDegToRad;
    changed = true;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted("Fov Y");

  float clipping[2] = {state.nearPlane, state.farPlane};
  ImGui::SetNextItemWidth(174.0f);
  if (ImGui::InputFloat2("##clipping", clipping, "%.3f")) {
    state.nearPlane = std::max(clipping[0], 0.0001f);
    state.farPlane = std::max(clipping[1], state.nearPlane + 0.001f);
    changed = true;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted("clip");

  float translation[3] = {state.position.x, state.position.y, state.position.z};
  ImGui::TextUnformatted("Translation");
  ImGui::SetNextItemWidth(174.0f);
  if (ImGui::InputFloat3("##translation", translation, "%.3f")) {
    state.position = {translation[0], translation[1], translation[2]};
    changed = true;
  }

  float rotation[3] = {state.yaw * kRadToDeg, state.pitch * kRadToDeg, state.roll * kRadToDeg};
  ImGui::TextUnformatted("Rotation");
  ImGui::SetNextItemWidth(174.0f);
  if (ImGui::InputFloat3("##rotation", rotation, "%.3f")) {
    state.yaw = rotation[0] * kDegToRad;
    state.pitch = rotation[1] * kDegToRad;
    state.roll = rotation[2] * kDegToRad;
    changed = true;
  }

  ImGui::SetNextItemWidth(174.0f);
  if (ImGui::SliderFloat("##movespeed", &state.movementSpeed, 0.001f, 100.0f, "%.3f")) {
    changed = true;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted("speed");

  ImGui::SetNextItemWidth(174.0f);
  if (ImGui::SliderFloat("##rotspeed", &state.rotationSpeed, 0.05f, 5.0f, "%.2f")) {
    changed = true;
  }
  ImGui::SameLine();
  ImGui::TextUnformatted("turn");

  if (ImGui::Checkbox("Acceleration", &state.useAcceleration)) {
    changed = true;
  }

  if (changed) {
    frame.camera->SetState(state);
  }

  RenderCameraSnapControls(frame);
}

void RenderCameraSection(UiFrameData& frame) {
  RenderCameraControls(frame);
}

void RenderAnimationSection(UiFrameData&) {}

void RenderStatisticsSection(UiFrameData& frame) {
  RenderStats(frame);
  RenderStatisticsGraphs(frame);
}

void RenderPinnedStatsWindow(const UiFrameData& frame) {
  const ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(kStatsMaxWidth, 1000.0f));
  const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoCollapse;
  if (ImGui::Begin("DirectXSplatStats", nullptr, flags)) {
    const uint64_t total = TotalSplats(frame);
    ImGui::TextUnformatted(FormatPinnedFps(frame.fps).c_str());
    ImGui::TextUnformatted(FormatPinnedSize(frame.renderWidth, frame.renderHeight).c_str());
    ImGui::TextUnformatted(FormatPinnedSplats(total).c_str());
    ImGui::TextUnformatted(FormatPinnedVisible(VisibleSplats(frame), total).c_str());
  }
  ImGui::End();
}

void RenderLeftControlsWindow(UiFrameData& frame, UiActions& actions) {
  const auto labels = UiSectionLabels();
  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSizeConstraints(ImVec2(kControlsWidth, 0.0f), ImVec2(kControlsWidth, 1000.0f));
  const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoCollapse;
  if (ImGui::Begin("DirectXSplatControls", nullptr, flags)) {
    RenderActionButtons(actions);
    if (ImGui::CollapsingHeader(labels[0], ImGuiTreeNodeFlags_DefaultOpen)) {
      RenderGraphicSection(frame);
    }
    if (ImGui::CollapsingHeader(labels[1], ImGuiTreeNodeFlags_DefaultOpen)) {
      RenderSceneSection(frame, actions);
    }
    if (ImGui::CollapsingHeader(labels[2], ImGuiTreeNodeFlags_DefaultOpen)) {
      RenderCameraSection(frame);
    }
    if (ImGui::CollapsingHeader(labels[3])) {
      RenderAnimationSection(frame);
    }
    if (ImGui::CollapsingHeader(labels[4])) {
      RenderStatisticsSection(frame);
    }
  }
  ImGui::End();
}

void PushOverlayStyle() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 5.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 3.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.027f, 0.025f, 0.90f));
  ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.010f, 0.011f, 0.012f, 0.96f));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.010f, 0.011f, 0.012f, 0.96f));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.070f, 0.145f, 0.235f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.095f, 0.210f, 0.340f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.120f, 0.265f, 0.430f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.075f, 0.155f, 0.250f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.115f, 0.240f, 0.380f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.145f, 0.295f, 0.455f, 0.95f));
  ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.360f, 0.680f, 1.000f, 1.00f));
}

void PopOverlayStyle() {
  ImGui::PopStyleColor(10);
  ImGui::PopStyleVar(5);
}

}  

void UiLayer::Render(UiFrameData& frame, UiActions& actions) {
  if (frame.settings == nullptr || frame.camera == nullptr) {
    return;
  }

  if (frame.showMetrics != nullptr) {
    *frame.showMetrics = true;
  }

  PushOverlayStyle();
  RenderLeftControlsWindow(frame, actions);
  RenderPinnedStatsWindow(frame);
  PopOverlayStyle();
}

}  
