#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "app/CameraController.h"
#include "platform/SwapchainContext.h"
#include "dxsplat/context.h"
#include "dxsplat/directxsplat.h"
#include "dxsplat/io.h"
#include "dxsplat/renderer.h"
#include "dxsplat/settings.h"
#include "platform/Win32Window.h"
#include "scene/SceneManager.h"
#include "tools/ScreenshotWriter.h"
#include "ui/UiLayer.h"

namespace dxsplat {

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
  Status OpenSceneDialogAndLoad();
  Status SaveScreenshotDialog();
  Status SetExportDirectoryDialog();
  Status CapturePointViewToExportDirectory();

  void UpdateInput(float dt);
  void UpdateBackgroundSceneLoading();
  void ApplyInitialFraming(const Scene& scene);
  void ApplyCameraSetToActiveScene();
  void UpdateSelectedInputCamera();
  void HandleDoubleClickFocus();
  void RequestTraversalScene(size_t index, bool activateWhenReady);
  size_t FindLoadedSceneIndexByPath(const std::string& path) const;
  Status UploadAndAddScene(Scene scene);
  void DestroyUploadedScenes();
  Status ApplyPendingResize();

  std::wstring OpenFileDialog(const wchar_t* filter, const wchar_t* title, bool saveMode);

  ViewerConfig config_;

  Win32Window window_;
  internal::SwapchainContext d3d_;
  D3D12Context coreContext_;
  Renderer renderer_;

  SceneManager sceneManager_;
  BackgroundSceneLoader traversalLoader_;
  CameraSet cameraSet_;
  bool cameraSetAssigned_ = false;

  CameraController camera_;
  RenderSettings renderSettings_{};
  int32_t selectedInputCamera_ = -1;
  uint32_t renderWidthOverride_ = 0;
  uint32_t renderHeightOverride_ = 0;
  bool vsyncEnabled_ = false;
  bool paused_ = false;
  bool showMetrics_ = true;
  bool guiVisible_ = true;
  bool guiToggleWasDown_ = false;
  FrameStats frameStats_{};
  float smoothedFrameMs_ = 0.0f;
  float smoothedFps_ = 0.0f;

  UiLayer ui_;

  ScreenshotWriter screenshotWriter_;

  bool imguiInitialized_ = false;
  std::string statusMessage_;
  bool traversalEnabled_ = false;
  bool traversalActivateRequested_ = false;
  size_t traversalRequestedIndex_ = 0;
  std::string traversalFolderPath_;
  std::string exportDirectory_ = ".";
  uint32_t captureIndex_ = 0;
  bool resizePending_ = false;
  uint32_t pendingResizeWidth_ = 0;
  uint32_t pendingResizeHeight_ = 0;
  bool fullscreenTogglePending_ = false;
};

}  // namespace dxsplat
