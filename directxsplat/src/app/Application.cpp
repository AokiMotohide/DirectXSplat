#include "app/Application.h"

#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include "api/CameraSetInternal.h"
#include "dxsplat/bounding.h"
#include "tools/ScenePathValidation.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace dxsplat {

namespace fs = std::filesystem;

namespace {

std::vector<InputCamera> InputCamerasFromCameraSet(const CameraSet& cameraSet) {
  std::vector<InputCamera> out;
  out.reserve(cameraSet.cameras.size());
  for (size_t i = 0; i < cameraSet.cameras.size(); ++i) {
    out.push_back(InputCameraFromCameraParams(cameraSet.cameras[i], i));
  }
  return out;
}

}  // namespace

Application::Application() = default;

Application::~Application() { Shutdown(); }

Status Application::Initialize(const ViewerConfig& config) {
  config_ = config;

  const uint32_t startupWidth = std::max<uint32_t>(config_.width, 1u);
  const uint32_t startupHeight = std::max<uint32_t>(config_.height, 1u);

  if (!window_.Create(L"DirectXSplat", startupWidth, startupHeight)) {
    return Status::Error("failed to create window (win32=" + std::to_string(window_.LastCreateError()) + ")");
  }

  Status status = d3d_.Initialize(window_.Hwnd(), window_.Width(), window_.Height(), true);
  if (!status.ok) {
    return status;
  }

  window_.SetResizeCallback([this](uint32_t w, uint32_t h) {
    if (w > 0 && h > 0) {
      pendingResizeWidth_ = w;
      pendingResizeHeight_ = h;
      resizePending_ = true;
    }
  });
  window_.SetDropCallback([this](const std::filesystem::path& path) {
    const auto validated = internal::ValidateDroppedScenePath(path);
    if (!validated.ok()) {
      statusMessage_ = validated.status.message;
      return;
    }
    const Status status = Load(validated.value);
    statusMessage_ = status.ok ? "" : status.message;
  });

  camera_.SetViewport(window_.Width(), window_.Height());

  status = coreContext_.Initialize(d3d_.Device(), d3d_.CommandQueue(), d3d_.Fence());
  if (!status.ok) {
    return status;
  }

  status = renderer_.Initialize(coreContext_);
  if (!status.ok) {
    return status;
  }
  status = cameraFrameRenderer_.Initialize(d3d_.Device(), DXGI_FORMAT_R8G8B8A8_UNORM);
  if (!status.ok) {
    return status;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 0.0f;
  style.PopupRounding = 0.0f;
  style.ScrollbarRounding = 0.0f;
  style.GrabRounding = 0.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.WindowPadding = ImVec2(6.0f, 5.0f);
  style.FramePadding = ImVec2(5.0f, 3.0f);
  style.ItemSpacing = ImVec2(5.0f, 4.0f);
  ImVec4* colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.027f, 0.025f, 0.90f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.025f, 0.027f, 0.025f, 0.90f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.010f, 0.011f, 0.012f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.010f, 0.011f, 0.012f, 0.96f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.010f, 0.011f, 0.012f, 0.96f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.105f, 0.155f, 0.220f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.145f, 0.220f, 0.310f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.185f, 0.285f, 0.400f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.105f, 0.155f, 0.220f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.145f, 0.220f, 0.310f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.185f, 0.285f, 0.400f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.105f, 0.155f, 0.220f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.145f, 0.220f, 0.310f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.185f, 0.285f, 0.400f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.360f, 0.680f, 1.000f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.280f, 0.580f, 0.920f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.360f, 0.680f, 1.000f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.91f, 0.70f, 0.02f, 1.0f);

  ImGui_ImplWin32_Init(window_.Hwnd());
  ImGui_ImplDX12_Init(d3d_.Device(), internal::SwapchainContext::kFrameCount, DXGI_FORMAT_R8G8B8A8_UNORM,
                      d3d_.ImGuiSrvHeap(),
                      d3d_.ImGuiSrvHeap()->GetCPUDescriptorHandleForHeapStart(),
                      d3d_.ImGuiSrvHeap()->GetGPUDescriptorHandleForHeapStart());
  imguiInitialized_ = true;

  renderWidthOverride_ = config_.width;
  renderHeightOverride_ = config_.height;
  vsyncEnabled_ = config_.vsync;

  if (!config_.initialScenePath.empty()) {
    status = Load(config_.initialScenePath);
    if (!status.ok) {
      statusMessage_ = status.message;
    }
  }

  if (!config_.sceneFolderPath.empty()) {
    const std::string folderPath = config_.sceneFolderPath.string();
    const Status traversalStatus =
        traversalLoader_.Initialize(folderPath, SceneLoadOptions{config_.sourceImageDirectory.string()});
    if (!traversalStatus.ok) {
      statusMessage_ = traversalStatus.message;
    } else {
      traversalEnabled_ = traversalLoader_.SceneCount() > 0;
      traversalFolderPath_ = folderPath;
      if (traversalEnabled_) {
        RequestTraversalScene(0, true);
      }
    }
  }

  return Status::Ok();
}

Status Application::Run() {
  auto previous = std::chrono::steady_clock::now();

  while (true) {
    window_.BeginFrameInput();
    if (!window_.PumpMessages()) {
      break;
    }

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - previous).count();
    previous = now;
    const float frameMs = dt * 1000.0f;
    const float fps = dt > 0.000001f ? 1.0f / dt : 0.0f;
    if (smoothedFrameMs_ <= 0.0f) {
      smoothedFrameMs_ = frameMs;
      smoothedFps_ = fps;
    } else {
      smoothedFrameMs_ = smoothedFrameMs_ * 0.90f + frameMs * 0.10f;
      smoothedFps_ = smoothedFps_ * 0.90f + fps * 0.10f;
    }

    UpdateBackgroundSceneLoading();

    if (window_.IsMinimized()) {
      Sleep(16);
      continue;
    }

    Status resizeStatus = ApplyPendingResize();
    if (!resizeStatus.ok) {
      statusMessage_ = resizeStatus.message;
      Sleep(16);
      continue;
    }

    frameStats_ = {};

    Status beginStatus = d3d_.BeginFrame(vsyncEnabled_);
    if (!beginStatus.ok) {
      statusMessage_ = beginStatus.message;
      continue;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    const InputState& frameInput = window_.Input();
    const bool guiToggleDown =
        (frameInput.KeyDown(VK_CONTROL) || frameInput.KeyDown(VK_LCONTROL) || frameInput.KeyDown(VK_RCONTROL)) &&
        (frameInput.KeyDown(VK_MENU) || frameInput.KeyDown(VK_LMENU) || frameInput.KeyDown(VK_RMENU)) &&
        frameInput.KeyDown('G');
    if (guiToggleDown && !guiToggleWasDown_) {
      guiVisible_ = !guiVisible_;
    }
    guiToggleWasDown_ = guiToggleDown;
    if (frameInput.KeyDown(VK_ESCAPE)) {
      window_.RequestClose();
    }

    if (!paused_) {
      UpdateInput(dt);
    }

    const Scene* activeScene = sceneManager_.ActiveScene();
    const UploadedSceneHandle activeUploadedScene = sceneManager_.ActiveUploadedScene();

    RenderInput input{};
    input.view = camera_.ViewMatrix();
    input.cameraPosition = camera_.State().position;
    input.settings = renderSettings_;
    input.viewportWidth = d3d_.Width();
    input.viewportHeight = d3d_.Height();
    input.nearPlane = camera_.State().nearPlane;
    input.farPlane = camera_.State().farPlane;
    const float aspect = static_cast<float>(input.viewportWidth) / std::max(1.0f, static_cast<float>(input.viewportHeight));
    input.proj = camera_.ProjectionMatrixForAspect(aspect);

    RenderTargetBinding target{};
    target.colorTarget = d3d_.CurrentBackBuffer();
    target.colorRtv = d3d_.CurrentRtv();
    target.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    target.viewport = d3d_.Viewport();
    target.scissor = d3d_.ScissorRect();
    target.clearColor = true;
    target.clearColorValue[0] = renderSettings_.backgroundColor.x;
    target.clearColorValue[1] = renderSettings_.backgroundColor.y;
    target.clearColorValue[2] = renderSettings_.backgroundColor.z;
    target.clearColorValue[3] = 1.0f;
    if (activeUploadedScene.IsValid()) {
      RenderFrameContext frameContext{};
      frameContext.fence = d3d_.Fence();
      frameContext.completedFenceValue = d3d_.CompletedFenceValue();
      frameContext.submissionFenceValue = d3d_.PendingSubmissionFenceValue();
      frameContext.frameIndex = d3d_.FrameIndex();
      input.frameIndex = frameContext.frameIndex;

      RenderPreparationResult preparationResult{};
      Status prepareStatus = renderer_.PrepareSceneForRender(activeUploadedScene, input, frameContext, &preparationResult);
      frameStats_ = preparationResult.stats;
      if (!prepareStatus.ok) {
        statusMessage_ = prepareStatus.message;
        d3d_.CommandList()->OMSetRenderTargets(1, &target.colorRtv, FALSE, nullptr);
        if (target.clearColor) {
          d3d_.CommandList()->ClearRenderTargetView(target.colorRtv, target.clearColorValue, 0, nullptr);
        }
      } else {
        RenderResult renderResult{};
        Status renderStatus = renderer_.Render(d3d_.CommandList(), target, activeUploadedScene, input, frameContext, renderResult);
        frameStats_ = renderResult.stats;
        if (!renderStatus.ok) {
          statusMessage_ = renderStatus.message;
        } else {
          statusMessage_.clear();
        }
        if (renderResult.submission.uploadSyncPoint.IsValid()) {
          HRESULT waitHr = d3d_.CommandQueue()->Wait(renderResult.submission.uploadSyncPoint.fence, renderResult.submission.uploadSyncPoint.value);
          if (FAILED(waitHr)) {
            statusMessage_ = "direct queue upload sync failed";
            renderer_.NotifyDeviceLost();
            d3d_.NotifyQueueLost();
            MessageBoxA(window_.Hwnd(), statusMessage_.c_str(), "DirectXSplat", MB_OK | MB_ICONERROR);
            break;
          }
        }
      }
    } else {
      d3d_.CommandList()->OMSetRenderTargets(1, &target.colorRtv, FALSE, nullptr);
      if (target.clearColor) {
        d3d_.CommandList()->ClearRenderTargetView(target.colorRtv, target.clearColorValue, 0, nullptr);
      }
    }

    UpdateGraphData(activeScene);

    Status cameraFrameStatus = cameraFrameRenderer_.Render(d3d_.CommandList(),
                                                           target.colorRtv,
                                                           d3d_.Viewport(),
                                                           d3d_.ScissorRect(),
                                                           input.view,
                                                           input.proj,
                                                           cameraSet_,
                                                           cameraUi_);
    if (!cameraFrameStatus.ok) {
      statusMessage_ = cameraFrameStatus.message;
    }

    UiFrameData uiFrame{};
    uiFrame.settings = &renderSettings_;
    uiFrame.cameraUi = &cameraUi_;
    uiFrame.selectedInputCamera = &selectedInputCamera_;
    uiFrame.renderWidthOverride = &renderWidthOverride_;
    uiFrame.renderHeightOverride = &renderHeightOverride_;
    uiFrame.vsyncEnabled = &vsyncEnabled_;
    uiFrame.paused = &paused_;
    uiFrame.showMetrics = &showMetrics_;
    uiFrame.guiVisible = &guiVisible_;
    uiFrame.fullscreen = window_.IsFullscreen();
    uiFrame.gpuName = d3d_.AdapterName();
    uiFrame.camera = &camera_;
    uiFrame.scene = activeScene;
    uiFrame.stats = &frameStats_;
    uiFrame.graphData = &graphData_;
    uiFrame.fps = smoothedFps_;
    uiFrame.frameMs = smoothedFrameMs_;
    uiFrame.renderWidth = input.viewportWidth;
    uiFrame.renderHeight = input.viewportHeight;
    uiFrame.cameraCount = cameraSet_.cameras.size();
    uiFrame.traversalEnabled = traversalEnabled_;
    uiFrame.traversalSceneCount = traversalLoader_.SceneCount();
    uiFrame.traversalCurrentIndex = traversalRequestedIndex_;
    uiFrame.statusMessage = statusMessage_;

    bool openSceneAfterFrame = false;
    UiActions actions{};
    actions.openScene = [&openSceneAfterFrame]() { openSceneAfterFrame = true; };
    actions.saveScreenshot = [this]() { SaveScreenshotDialog(); };
    actions.nextScene = [this]() {
      if (traversalEnabled_ && traversalLoader_.SceneCount() > 0) {
        const size_t next = (traversalRequestedIndex_ + 1) % traversalLoader_.SceneCount();
        RequestTraversalScene(next, true);
        return;
      }
      if (sceneManager_.Scenes().empty()) return;
      size_t idx = (sceneManager_.ActiveSceneIndex() + 1) % sceneManager_.Scenes().size();
      sceneManager_.SetActiveSceneIndex(idx);
      if (!cameraSetAssigned_) {
        CaptureActiveSceneCameraSet();
      }
      UpdateSelectedInputCamera();
      ApplyInitialFraming(*sceneManager_.ActiveScene());
    };
    actions.prevScene = [this]() {
      if (traversalEnabled_ && traversalLoader_.SceneCount() > 0) {
        const size_t prev = traversalRequestedIndex_ == 0 ? traversalLoader_.SceneCount() - 1 : traversalRequestedIndex_ - 1;
        RequestTraversalScene(prev, true);
        return;
      }
      if (sceneManager_.Scenes().empty()) return;
      size_t idx = sceneManager_.ActiveSceneIndex() == 0 ? sceneManager_.Scenes().size() - 1
                                                          : sceneManager_.ActiveSceneIndex() - 1;
      sceneManager_.SetActiveSceneIndex(idx);
      if (!cameraSetAssigned_) {
        CaptureActiveSceneCameraSet();
      }
      UpdateSelectedInputCamera();
      ApplyInitialFraming(*sceneManager_.ActiveScene());
    };
    actions.resetView = [this]() {
      if (sceneManager_.ActiveScene() != nullptr) {
        ApplyInitialFraming(*sceneManager_.ActiveScene());
      }
    };
    actions.exitApplication = [this]() { window_.RequestClose(); };
    actions.selectCamera = [this](int32_t index) { SelectCameraIndex(index); };

    if (guiVisible_) {
      ui_.Render(uiFrame, actions);
    }

    ImGui::Render();
    ID3D12DescriptorHeap* heaps[] = {d3d_.ImGuiSrvHeap()};
    d3d_.CommandList()->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d_.CommandList());

    Status endStatus = d3d_.EndFrame(vsyncEnabled_);
    if (!endStatus.ok) {
      statusMessage_ = endStatus.message;
      renderer_.NotifyDeviceLost();
      MessageBoxA(window_.Hwnd(), statusMessage_.c_str(), "DirectXSplat", MB_OK | MB_ICONERROR);
      break;
    }

    if (fullscreenTogglePending_) {
      fullscreenTogglePending_ = false;
      window_.SetFullscreen(!window_.IsFullscreen());
    }

    if (screenshotWriter_.HasPendingCapture()) {
      std::string completedPath;
      Status screenshotStatus = screenshotWriter_.ResolvePendingCapture(&completedPath);
      if (!screenshotStatus.ok) {
        statusMessage_ = screenshotStatus.message;
      } else if (!completedPath.empty()) {
        statusMessage_ = "Screenshot saved";
      }
    }

    if (openSceneAfterFrame) {
      OpenSceneDialogAndLoad();
    }
  }

  return Status::Ok();
}

void Application::RequestClose() { window_.RequestClose(); }

void Application::Shutdown() {
  traversalLoader_.Shutdown();
  traversalEnabled_ = false;
  traversalActivateRequested_ = false;
  traversalRequestedIndex_ = 0;
  traversalFolderPath_.clear();
  Status idle = d3d_.WaitForGpu();
  const bool gpuIdle = idle.ok;
  if (gpuIdle) {
    DestroyUploadedScenes();
  } else {
    statusMessage_ = idle.message;
    renderer_.NotifyDeviceLost();
  }
  cameraFrameRenderer_.Shutdown();

  if (imguiInitialized_) {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    imguiInitialized_ = false;
  }

  if (gpuIdle) {
    renderer_.Shutdown();
    coreContext_.Reset();
    d3d_.Shutdown();
  } else {
    renderer_.Shutdown();
    coreContext_.Reset();
    d3d_.Shutdown();
  }
  window_.Destroy();
}

Status Application::Load(const std::filesystem::path& scenePath) {
  const std::string path = scenePath.string();
  auto sceneResult = LoadSceneFromFile(path, SceneLoadOptions{config_.sourceImageDirectory.string()});
  if (!sceneResult.ok()) {
    return Status::Error(sceneResult.status.message);
  }

  return SetScene(std::move(sceneResult.value));
}

Status Application::SetScene(Scene scene) {
  DestroyUploadedScenes();
  sceneManager_.Clear();
  graphScene_ = nullptr;
  Status uploadStatus = UploadAndAddScene(std::move(scene));
  if (!uploadStatus.ok) {
    return uploadStatus;
  }
  if (!cameraSetAssigned_) {
    CaptureActiveSceneCameraSet();
  }
  ApplyCameraSetToActiveScene();
  UpdateSelectedInputCamera();
  if (sceneManager_.ActiveScene() != nullptr) {
    ApplyInitialFraming(*sceneManager_.ActiveScene());
  }
  statusMessage_.clear();
  return Status::Ok();
}

Status Application::SetCameraSet(CameraSet cameras) {
  cameraSet_ = std::move(cameras);
  cameraSetAssigned_ = true;
  cameraFrameRenderer_.Invalidate();
  ClampCameraUiState(cameraUi_, cameraSet_.cameras.size());
  ApplyCameraSetToActiveScene();
  UpdateSelectedInputCamera();
  SelectCameraIndex(cameraUi_.index);
  return Status::Ok();
}

const CameraSet& Application::ActiveCameraSet() const { return cameraSet_; }

Status Application::OpenSceneDialogAndLoad() {
  const std::wstring file = OpenFileDialog(
      L"Scene Files (*.ply;*.sog;*.spz;*.splat;meta.json;lod-meta.json)\0*.ply;*.sog;*.spz;*.splat;meta.json;lod-meta.json\0All Files\0*.*\0\0", L"Open Scene", false);
  if (file.empty()) {
    return Status::Ok();
  }

  const int bytes = WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
  WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, utf8.data(), bytes, nullptr, nullptr);

  Status status = Load(fs::path(utf8));
  if (!status.ok) {
    statusMessage_ = status.message;
  }
  return status;
}

Status Application::SaveScreenshotDialog() {
  const std::wstring file = OpenFileDialog(L"PPM Files (*.ppm)\0*.ppm\0\0", L"Save Screenshot", true);
  if (file.empty()) {
    return Status::Ok();
  }

  const int bytes = WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
  WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, utf8.data(), bytes, nullptr, nullptr);
  if (fs::path(utf8).extension().empty()) {
    utf8 += ".ppm";
  }

  Status status = screenshotWriter_.QueueBackBufferPpm(d3d_, utf8);
  if (status.ok) {
    statusMessage_ = "Screenshot queued";
  } else {
    statusMessage_ = status.message;
  }
  return status;
}

Status Application::SetExportDirectoryDialog() {
  BROWSEINFOW bi{};
  bi.hwndOwner = window_.Hwnd();
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  bi.lpszTitle = L"Set export directory";
  PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
  if (pidl == nullptr) {
    return Status::Ok();
  }
  wchar_t path[MAX_PATH] = {};
  const BOOL ok = SHGetPathFromIDListW(pidl, path);
  CoTaskMemFree(pidl);
  if (!ok) {
    statusMessage_ = "Failed to set export directory";
    return Status::Error(statusMessage_);
  }
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
  WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8.data(), bytes, nullptr, nullptr);
  exportDirectory_ = utf8;
  statusMessage_ = "Export directory set";
  return Status::Ok();
}

Status Application::CapturePointViewToExportDirectory() {
  fs::path dir(exportDirectory_.empty() ? "." : exportDirectory_);
  if (!fs::exists(dir)) {
    fs::create_directories(dir);
  }
  std::ostringstream name;
  name << "point_view_" << std::setw(4) << std::setfill('0') << captureIndex_++ << ".ppm";
  const std::string path = (dir / name.str()).string();
  Status status = screenshotWriter_.QueueBackBufferPpm(d3d_, path);
  if (status.ok) {
    statusMessage_ = "Capture queued";
  } else {
    statusMessage_ = status.message;
  }
  return status;
}

void Application::UpdateInput(float dt) {
  ImGuiIO& io = ImGui::GetIO();
  const InputState& input = window_.Input();

  if (!io.WantCaptureMouse && input.mouseDoubleClickLeft) {
    HandleDoubleClickFocus();
  }

  CameraState state = camera_.State();
  if (state.navigatorMode == NavigatorMode::Trackball) {
    state.navigatorMode = NavigatorMode::Orbit;
    camera_.SetState(state);
  }
  if (state.navigatorMode == NavigatorMode::None) {
    return;
  }

  if (state.navigatorMode == NavigatorMode::Fps) {
    const bool mouseLook = (input.mouseButtonsDown[0] || input.mouseButtonsDown[1]) && !io.WantCaptureMouse;
    float lookDx = mouseLook ? input.mouseDeltaX : 0.0f;
    float lookDy = mouseLook ? input.mouseDeltaY : 0.0f;
    float rollDelta = 0.0f;
    constexpr float keyLookRate = 500.0f;
    if (!io.WantCaptureKeyboard) {
      if (input.KeyDown('J')) lookDx -= keyLookRate * dt;
      if (input.KeyDown('L')) lookDx += keyLookRate * dt;
      if (input.KeyDown('I')) lookDy -= keyLookRate * dt;
      if (input.KeyDown('K')) lookDy += keyLookRate * dt;
      if (input.KeyDown('U')) rollDelta -= keyLookRate * dt;
      if (input.KeyDown('O')) rollDelta += keyLookRate * dt;
    }
    const bool rotationEnabled = mouseLook || lookDx != 0.0f || lookDy != 0.0f || rollDelta != 0.0f;
    camera_.UpdateFps(dt, input.KeyDown('W'), input.KeyDown('S'), input.KeyDown('A'), input.KeyDown('D'),
                      input.KeyDown('E'), input.KeyDown('Q'), lookDx, lookDy, rollDelta, rotationEnabled);
  } else {
    float orbitDx = 0.0f;
    float orbitDy = 0.0f;
    float panDx = 0.0f;
    float panDy = 0.0f;
    if (!io.WantCaptureMouse) {
      if (input.mouseButtonsDown[0]) {
        orbitDx = input.mouseDeltaX;
        orbitDy = input.mouseDeltaY;
      }
      if (input.mouseButtonsDown[2]) {
        panDx = input.mouseDeltaX;
        panDy = input.mouseDeltaY;
      }
    }
    camera_.UpdateOrbit(dt, orbitDx, orbitDy, panDx, panDy, io.WantCaptureMouse ? 0.0f : input.wheelDelta);
  }
}

void Application::UpdateBackgroundSceneLoading() {
  size_t index = 0;
  Scene loadedScene;
  std::string error;
  while (traversalLoader_.PollLoaded(index, loadedScene, error)) {
    if (!error.empty()) {
      statusMessage_ = error;
      continue;
    }
    if (!loadedScene.sourcePath.empty()) {
      Scene scene = std::move(loadedScene);
      const size_t existing = FindLoadedSceneIndexByPath(scene.sourcePath);
      if (existing == std::numeric_limits<size_t>::max()) {
        Status uploadStatus = UploadAndAddScene(std::move(scene));
        if (!uploadStatus.ok) {
          statusMessage_ = uploadStatus.message;
          continue;
        }
      } else {
        sceneManager_.SetActiveSceneIndex(existing);
      }

      if (traversalActivateRequested_ && index == traversalRequestedIndex_) {
        const std::vector<std::string> scenePaths = traversalLoader_.ScenePaths();
        if (index < scenePaths.size()) {
          const std::string targetPath = scenePaths[index];
          const size_t active = FindLoadedSceneIndexByPath(targetPath);
          if (active != std::numeric_limits<size_t>::max()) {
            sceneManager_.SetActiveSceneIndex(active);
            if (sceneManager_.ActiveScene() != nullptr) {
              if (!cameraSetAssigned_) {
                CaptureActiveSceneCameraSet();
              }
              UpdateSelectedInputCamera();
              ApplyInitialFraming(*sceneManager_.ActiveScene());
            }
          }
        }
        traversalActivateRequested_ = false;
      }
    }
  }
}

void Application::ApplyInitialFraming(const Scene& scene) {
  std::vector<Vec3> points;
  for (const auto& set : scene.splatSets) {
    for (const auto& g : set.gaussians) {
      points.push_back(g.position);
    }
  }
  const Aabb robust = ComputePercentileAabb(points, 0.25f, 99.75f);
  if (robust.valid) {
    camera_.FocusBounds(robust);
  } else if (scene.sceneBounds.valid) {
    camera_.FocusBounds(scene.sceneBounds);
  }
}

void Application::ApplyCameraSetToActiveScene() {
  Scene* activeScene = sceneManager_.ActiveScene();
  if (activeScene == nullptr || !cameraSetAssigned_) {
    return;
  }
  activeScene->inputCameras = InputCamerasFromCameraSet(cameraSet_);
}

void Application::CaptureActiveSceneCameraSet() {
  const Scene* activeScene = sceneManager_.ActiveScene();
  if (activeScene == nullptr) {
    cameraSet_ = {};
    cameraFrameRenderer_.Invalidate();
    ClampCameraUiState(cameraUi_, 0);
    return;
  }
  StatusOr<CameraSet> cameras = ConvertInputCamerasToCameraSet(*activeScene);
  if (cameras.ok()) {
    cameraSet_ = std::move(cameras.value);
  } else {
    cameraSet_ = {};
  }
  cameraFrameRenderer_.Invalidate();
  ClampCameraUiState(cameraUi_, cameraSet_.cameras.size());
}

void Application::UpdateSelectedInputCamera() {
  const Scene* activeScene = sceneManager_.ActiveScene();
  if (activeScene != nullptr && !activeScene->inputCameras.empty()) {
    selectedInputCamera_ = std::clamp(
        selectedInputCamera_ < 0 ? 0 : selectedInputCamera_, 0,
        static_cast<int32_t>(activeScene->inputCameras.size() - 1));
    cameraUi_.index = selectedInputCamera_;
  } else {
    selectedInputCamera_ = -1;
  }
  ClampCameraUiState(cameraUi_, cameraSet_.cameras.size());
}

void Application::SelectCameraIndex(int32_t index) {
  if (cameraSet_.cameras.empty()) {
    ClampCameraUiState(cameraUi_, 0);
    selectedInputCamera_ = -1;
    return;
  }

  cameraUi_.index = index;
  ClampCameraUiState(cameraUi_, cameraSet_.cameras.size());
  selectedInputCamera_ = cameraUi_.index;

  if (selectedInputCamera_ >= 0 && static_cast<size_t>(selectedInputCamera_) < cameraSet_.cameras.size()) {
    camera_.SnapToCameraParams(cameraSet_.cameras[static_cast<size_t>(selectedInputCamera_)]);
  }
}

void Application::UpdateGraphData(const Scene* activeScene) {
  PushGraphSample(graphData_.fps, smoothedFps_);
  PushGraphSample(graphData_.visible, VisiblePercentageSample(frameStats_));
  if (activeScene != graphScene_) {
    graphData_.splatAlpha = activeScene != nullptr ? BuildSplatAlphaHistogram(*activeScene, kSplatAlphaHistogramBins)
                                                   : HistogramData{};
    graphScene_ = activeScene;
  }
  graphData_.projectionActiveThreads =
      BuildProjectionActiveThreadsHistogram(frameStats_, kProjectionActiveThreadHistogramBins);
}

void Application::HandleDoubleClickFocus() {
  const Scene* scene = sceneManager_.ActiveScene();
  if (scene == nullptr) {
    return;
  }

  const InputState& input = window_.Input();
  const float ndcX = (static_cast<float>(input.mouseX) / std::max(1.0f, static_cast<float>(window_.Width()))) * 2.0f - 1.0f;
  const float ndcY = 1.0f - (static_cast<float>(input.mouseY) / std::max(1.0f, static_cast<float>(window_.Height()))) * 2.0f;

  const Vec3 rayOrigin = camera_.State().position;
  const Vec3 rayDir = camera_.ScreenToWorldRayDir(ndcX, ndcY);

  float bestT = std::numeric_limits<float>::max();
  Vec3 bestPoint{};
  float bestRadius = 0.1f;

  for (const auto& set : scene->splatSets) {
    if (!set.visible) {
      continue;
    }
    for (const auto& g : set.gaussians) {
      const Vec3 to = g.position - rayOrigin;
      const float t = Dot(to, rayDir);
      if (t <= 0.0f || t >= bestT) {
        continue;
      }
      const Vec3 closest = rayOrigin + rayDir * t;
      const float radius = std::max({std::abs(g.scale.x), std::abs(g.scale.y), std::abs(g.scale.z), 0.01f});
      const float dist = Length(g.position - closest);
      if (dist < radius * 2.0f) {
        bestT = t;
        bestPoint = g.position;
        bestRadius = radius;
      }
    }
  }

  if (bestT < std::numeric_limits<float>::max()) {
    camera_.FocusPoint(bestPoint, bestRadius * 2.0f);
    camera_.SetOrbitPivot(bestPoint);
  }
}

void Application::RequestTraversalScene(size_t index, bool activateWhenReady) {
  if (!traversalEnabled_ || traversalLoader_.SceneCount() == 0) {
    return;
  }
  traversalRequestedIndex_ = std::min(index, traversalLoader_.SceneCount() - 1);
  traversalActivateRequested_ = activateWhenReady;
  traversalLoader_.RequestLoad(traversalRequestedIndex_);

  const std::vector<std::string> scenePaths = traversalLoader_.ScenePaths();
  if (traversalRequestedIndex_ >= scenePaths.size()) {
    return;
  }
  const std::string targetPath = scenePaths[traversalRequestedIndex_];
  const size_t existing = FindLoadedSceneIndexByPath(targetPath);
  if (existing != std::numeric_limits<size_t>::max()) {
    sceneManager_.SetActiveSceneIndex(existing);
    if (sceneManager_.ActiveScene() != nullptr) {
      if (!cameraSetAssigned_) {
        CaptureActiveSceneCameraSet();
      }
      UpdateSelectedInputCamera();
      ApplyInitialFraming(*sceneManager_.ActiveScene());
    }
    traversalActivateRequested_ = false;
  } else {
    statusMessage_ = "Loading scene " + std::to_string(traversalRequestedIndex_ + 1) + "/" +
                     std::to_string(traversalLoader_.SceneCount());
  }
}

Status Application::UploadAndAddScene(Scene scene) {
  UploadedSceneHandle handle{};
  std::vector<UploadedChunkHandle> chunks;
  Status uploadStatus = renderer_.CreateUploadedScene(scene, handle, &chunks);
  if (!uploadStatus.ok) {
    return uploadStatus;
  }
  return sceneManager_.AddScene(std::move(scene), handle, std::move(chunks));
}

void Application::DestroyUploadedScenes() {
  for (const auto& entry : sceneManager_.Scenes()) {
    if (entry.uploadedScene.IsValid()) {
      renderer_.DestroyUploadedScene(entry.uploadedScene);
    }
  }
}

Status Application::ApplyPendingResize() {
  if (!resizePending_) {
    return Status::Ok();
  }
  resizePending_ = false;
  if (pendingResizeWidth_ == 0 || pendingResizeHeight_ == 0) {
    return Status::Ok();
  }
  if (pendingResizeWidth_ == d3d_.Width() && pendingResizeHeight_ == d3d_.Height()) {
    camera_.SetViewport(pendingResizeWidth_, pendingResizeHeight_);
    return Status::Ok();
  }
  Status s = d3d_.Resize(pendingResizeWidth_, pendingResizeHeight_);
  if (!s.ok) {
    return s;
  }
  camera_.SetViewport(pendingResizeWidth_, pendingResizeHeight_);
  return Status::Ok();
}

size_t Application::FindLoadedSceneIndexByPath(const std::string& path) const {
  const auto& scenes = sceneManager_.Scenes();
  for (size_t i = 0; i < scenes.size(); ++i) {
    if (scenes[i].scene.sourcePath == path) {
      return i;
    }
  }
  return std::numeric_limits<size_t>::max();
}

std::wstring Application::OpenFileDialog(const wchar_t* filter, const wchar_t* title, bool saveMode) {
  wchar_t fileName[MAX_PATH] = {};

  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = window_.Hwnd();
  ofn.lpstrFilter = filter;
  ofn.lpstrFile = fileName;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrTitle = title;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (saveMode) {
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
      return fileName;
    }
  } else {
    if (GetOpenFileNameW(&ofn)) {
      return fileName;
    }
  }

  return {};
}

}  // namespace dxsplat
