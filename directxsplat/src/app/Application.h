#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/CameraController.h"
#include "platform/SwapchainContext.h"
#include "app/CameraPathAnimator.h"
#include "directxsplat/context.h"
#include "directxsplat/directxsplat.h"
#include "directxsplat/io.h"
#include "directxsplat/renderer.h"
#include "directxsplat/settings.h"
#include "platform/Win32Window.h"
#include "render/CameraFrameRenderer.h"
#include "scene/SceneManager.h"
#include "ui/UiLayer.h"

namespace directxsplat {

class Application {
 public:
  Application();
  ~Application();

  Status Initialize(const ViewerConfig& config);
  Status Load(const std::filesystem::path& scenePath);
  Status SetScene(Scene scene);
  Status SetCameraSet(CameraSet cameras);
  const CameraSet& ActiveCameraSet() const;
  Status Run();
  void RequestClose();
  void Shutdown();

 private:
  void UpdateInput(float dt);
  void UpdateAnimation(float dt);
  void UpdateBackgroundSceneLoading();
  void ApplyInitialFraming(const Scene& scene);
  void ApplyCameraSetToActiveScene();
  void CaptureActiveSceneCameraSet();
  void UpdateSelectedInputCamera();
  void SelectCameraIndex(int32_t index);
  void UpdateGraphData(const Scene* activeScene);
  void HandleDoubleClickFocus();
  void RequestTraversalScene(size_t index, bool activateWhenReady);
  size_t FindLoadedSceneIndexByPath(const std::string& path) const;
  Status UploadAndAddScene(Scene scene);
  void DestroyUploadedScenes();
  Status ApplyPendingResize();

  ViewerConfig config_;

  Win32Window window_;
  internal::SwapchainContext d3d_;
  D3D12Context coreContext_;
  Renderer renderer_;

  SceneManager sceneManager_;
  BackgroundSceneLoader traversalLoader_;
  CameraSet cameraSet_;
  bool cameraSetAssigned_ = false;
  CameraUiState cameraUi_{};
  AnimationUiState animationUi_{};
  CameraPathAnimator cameraPathAnimator_;
  CameraFrameRenderer cameraFrameRenderer_;

  CameraController camera_;
  RenderSettings renderSettings_{};
  int32_t selectedInputCamera_ = -1;
  uint32_t renderWidthOverride_ = 0;
  uint32_t renderHeightOverride_ = 0;
  bool vsyncEnabled_ = false;
  bool paused_ = false;
  bool cameraCutPending_ = false;
  bool showMetrics_ = true;
  bool guiVisible_ = true;
  bool guiToggleWasDown_ = false;
  FrameStats frameStats_{};
  ViewerGraphData graphData_{};
  float smoothedFrameMs_ = 0.0f;
  float smoothedFps_ = 0.0f;

  UiLayer ui_;

  bool imguiInitialized_ = false;
  std::string statusMessage_;
  bool traversalEnabled_ = false;
  bool traversalActivateRequested_ = false;
  size_t traversalRequestedIndex_ = 0;
  std::string traversalFolderPath_;
  bool resizePending_ = false;
  uint32_t pendingResizeWidth_ = 0;
  uint32_t pendingResizeHeight_ = 0;
  bool fullscreenTogglePending_ = false;
};

}  // namespace directxsplat
