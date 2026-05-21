#include <Windows.h>

#include <iostream>
#include <string>

#include "appcommon/swapchain_context.h"
#include "dxsplat/context.h"
#include "dxsplat/renderer.h"
#include "dxsplat_examples/ExampleArgs.h"
#include "dxsplat_examples/ExampleRender.h"

namespace {

class MinimalWindow {
 public:
  bool Create(uint32_t width, uint32_t height) {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = &MinimalWindow::WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"DirectXSplatMinimalViewerWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd_ = CreateWindowExW(0,
                            wc.lpszClassName,
                            L"DirectXSplat Minimal Viewer",
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            rect.right - rect.left,
                            rect.bottom - rect.top,
                            nullptr,
                            nullptr,
                            instance,
                            this);
    if (hwnd_ == nullptr) {
      return false;
    }
    ShowWindow(hwnd_, SW_SHOW);
    return true;
  }

  bool PumpMessages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return false;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    return running_;
  }

  HWND Hwnd() const { return hwnd_; }

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    MinimalWindow* window = nullptr;
    if (message == WM_NCCREATE) {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
      window = static_cast<MinimalWindow*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
      window = reinterpret_cast<MinimalWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window != nullptr) {
      switch (message) {
        case WM_CLOSE:
          window->running_ = false;
          DestroyWindow(hwnd);
          return 0;
        case WM_DESTROY:
          window->running_ = false;
          PostQuitMessage(0);
          return 0;
        default:
          break;
      }
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }

  HWND hwnd_ = nullptr;
  bool running_ = true;
};

}  // namespace

int main(int argc, char** argv) {
  using namespace dxsplat;
  using namespace dxsplat::examples;

  StatusOr<ExampleOptions> parsed = ParseExampleOptions(ExampleKind::MinimalViewer, ArgsFromMain(argc, argv));
  if (!parsed.ok()) {
    std::cerr << parsed.status.message << "\n";
    return 1;
  }
  if (parsed.value.showHelp) {
    std::cout << ExampleUsage(ExampleKind::MinimalViewer) << "\n";
    return 0;
  }

  StatusOr<Scene> loaded = LoadExampleScene(parsed.value.scenePath);
  if (!loaded.ok()) {
    std::cerr << loaded.status.message << "\n";
    return 1;
  }

  MinimalWindow window;
  if (!window.Create(parsed.value.width, parsed.value.height)) {
    std::cerr << "failed creating window\n";
    return 1;
  }

  appcommon::SwapchainContext swapchain;
  Status status = swapchain.Initialize(window.Hwnd(), parsed.value.width, parsed.value.height, false);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  D3D12Context context;
  status = context.Initialize(swapchain.Device(), swapchain.CommandQueue(), swapchain.Fence());
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  Renderer renderer;
  status = renderer.Initialize(context);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  UploadedSceneHandle sceneHandle{};
  status = renderer.CreateUploadedScene(loaded.value, sceneHandle);
  if (!status.ok) {
    std::cerr << status.message << "\n";
    return 1;
  }

  uint32_t frame = 0;
  while (window.PumpMessages() && (parsed.value.frameCount == 0 || frame < parsed.value.frameCount)) {
    status = swapchain.BeginFrame(parsed.value.vsync);
    if (!status.ok) {
      std::cerr << status.message << "\n";
      renderer.NotifyDeviceLost();
      break;
    }

    RenderInput input = MakeExampleRenderInput(loaded.value, swapchain.Width(), swapchain.Height(), swapchain.FrameIndex());

    RenderTargetBinding target{};
    target.colorTarget = swapchain.CurrentBackBuffer();
    target.colorRtv = swapchain.CurrentRtv();
    target.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    target.colorStateBefore = D3D12_RESOURCE_STATE_PRESENT;
    target.colorStateAfter = D3D12_RESOURCE_STATE_PRESENT;
    target.viewport = swapchain.Viewport();
    target.scissor = swapchain.ScissorRect();
    target.clearColor = true;
    target.clearColorValue[3] = 1.0f;

    RenderFrameContext frameContext{};
    frameContext.fence = swapchain.Fence();
    frameContext.completedFenceValue = swapchain.CompletedFenceValue();
    frameContext.submissionFenceValue = swapchain.PendingSubmissionFenceValue();
    frameContext.frameIndex = swapchain.FrameIndex();

    RenderPreparationResult preparation{};
    status = renderer.PrepareSceneForRender(sceneHandle, input, frameContext, &preparation);
    RenderResult result{};
    if (status.ok) {
      status = renderer.Render(swapchain.CommandList(), target, sceneHandle, input, frameContext, result);
      if (result.submission.uploadSyncPoint.IsValid()) {
        HRESULT waitHr = swapchain.CommandQueue()->Wait(result.submission.uploadSyncPoint.fence,
                                                        result.submission.uploadSyncPoint.value);
        if (FAILED(waitHr)) {
          status = Status::Error("direct queue upload sync failed");
        }
      }
    }

    Status endStatus = swapchain.EndFrame(parsed.value.vsync);
    if (!status.ok || !endStatus.ok) {
      std::cerr << (!status.ok ? status.message : endStatus.message) << "\n";
      if (!endStatus.ok) {
        renderer.NotifyDeviceLost();
      }
      break;
    }
    ++frame;
  }

  renderer.Shutdown();
  context.Shutdown();
  swapchain.Shutdown();
  return status.ok ? 0 : 1;
}
