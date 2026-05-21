#include "ui/UiLayer.h"

#include <algorithm>
#include <imgui.h>

namespace dxsplat {

namespace {

constexpr float kRadToDeg = 57.295779513f;
constexpr float kDegToRad = 0.0174532925199f;

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
  const uint64_t sceneSplats = CountSceneSplats(frame.scene);
  const uint64_t totalSplats = stats != nullptr && stats->gaussiansTotal > 0 ? stats->gaussiansTotal : sceneSplats;
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

void RenderToggleRow(UiFrameData& frame) {
  if (frame.vsyncEnabled != nullptr) {
    ImGui::Checkbox("Vsync", frame.vsyncEnabled);
    ImGui::SameLine();
  }
  ImGui::Checkbox("AA", &frame.settings->antialiasing);
}

void RenderRenderControls(UiFrameData& frame) {
  ImGui::SetNextItemWidth(128.0f);
  ImGui::SliderFloat("##scalemod", &frame.settings->gaussianScalingModifier, 0.001f, 2.0f, "%.3f");
  ImGui::SameLine();
  ImGui::TextUnformatted("scale");
  ImGui::SetNextItemWidth(128.0f);
  ImGui::SliderFloat("##maxaxispixels", &frame.settings->maxAxisPixels, 1.0f, 512.0f, "%.0f");
  ImGui::SameLine();
  ImGui::TextUnformatted("projection");
  ImGui::SetNextItemWidth(128.0f);
  ImGui::SliderFloat("##aastrength", &frame.settings->antialiasingStrength, 0.0f, 2.5f, "%.2f");
  ImGui::SameLine();
  ImGui::TextUnformatted("aa");
  RenderToggleRow(frame);
  ImGui::Checkbox("Fast culling", &frame.settings->fastCulling);
  if (frame.settings->fastCulling) {
    ImGui::SetNextItemWidth(128.0f);
    ImGui::SliderFloat("##frustumdilation", &frame.settings->frustumDilation, 0.0f, 0.25f, "%.3f");
    ImGui::SameLine();
    ImGui::TextUnformatted("dilation");
  }
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
    showMetricsPanel_ = true;
    *frame.showMetrics = true;
  }

  PushOverlayStyle();
  ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSizeConstraints(ImVec2(292.0f, 0.0f), ImVec2(292.0f, 1000.0f));
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
  if (ImGui::Begin("DirectXSplat", nullptr, flags)) {
    RenderActionButtons(actions);
    RenderStats(frame);
    CompactSeparator();
    RenderRenderControls(frame);
    RenderTraversalControls(frame, actions);
    CompactSeparator();
    RenderCameraControls(frame);
  }
  ImGui::End();
  PopOverlayStyle();
}

}  
