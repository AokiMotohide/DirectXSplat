#include "platform/Win32Window.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <backends/imgui_impl_win32.h>

#ifdef IsMinimized
#undef IsMinimized
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace directxsplat {

namespace fs = std::filesystem;
constexpr UINT kDragQueryFileCount = 0xFFFFFFFFu;

Win32Window::Win32Window() = default;

Win32Window::~Win32Window() { Destroy(); }

bool Win32Window::Create(const std::wstring& title, uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
  lastCreateError_ = 0;

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wc.lpfnWndProc = StaticWndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"DxsplatWindowClass";
  const ATOM cls = RegisterClassExW(&wc);
  if (cls == 0) {
    const DWORD err = GetLastError();
    if (err != ERROR_CLASS_ALREADY_EXISTS) {
      lastCreateError_ = err;
      return false;
    }
  }

  RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  hwnd_ = CreateWindowExW(0, wc.lpszClassName, title.c_str(),
                          WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                          rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
                          wc.hInstance, this);
  if (hwnd_ == nullptr) {
    lastCreateError_ = GetLastError();
    return false;
  }

  BOOL useDark = TRUE;
  DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
  DragAcceptFiles(hwnd_, TRUE);
  return true;
}

void Win32Window::Destroy() {
  if (hwnd_ != nullptr) {
    DragAcceptFiles(hwnd_, FALSE);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

bool Win32Window::PumpMessages() {
  MSG msg{};
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
    if (msg.message == WM_QUIT) {
      running_ = false;
    }
  }
  return running_;
}

void Win32Window::RequestClose() {
  running_ = false;
  PostQuitMessage(0);
}

void Win32Window::SetFullscreen(bool fullscreen) {
  if (hwnd_ == nullptr || fullscreen_ == fullscreen) {
    return;
  }

  if (fullscreen) {
    windowedStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
    GetWindowRect(hwnd_, &windowedRect_);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
    SetWindowLongPtrW(hwnd_, GWL_STYLE, windowedStyle_ & ~WS_OVERLAPPEDWINDOW);
    SetWindowPos(hwnd_, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                 monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                 monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    fullscreen_ = true;
  } else {
    SetWindowLongPtrW(hwnd_, GWL_STYLE, windowedStyle_ == 0 ? WS_OVERLAPPEDWINDOW : windowedStyle_);
    SetWindowPos(hwnd_, nullptr, windowedRect_.left, windowedRect_.top,
                 windowedRect_.right - windowedRect_.left, windowedRect_.bottom - windowedRect_.top,
                 SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED);
    fullscreen_ = false;
  }
}

bool Win32Window::IsFullscreen() const { return fullscreen_; }

HWND Win32Window::Hwnd() const { return hwnd_; }

DWORD Win32Window::LastCreateError() const { return lastCreateError_; }

uint32_t Win32Window::Width() const { return width_; }

uint32_t Win32Window::Height() const { return height_; }

bool Win32Window::IsMinimized() const { return minimized_; }

InputState& Win32Window::Input() { return input_; }

const InputState& Win32Window::Input() const { return input_; }

void Win32Window::BeginFrameInput() { input_.BeginFrame(); }

void Win32Window::SetResizeCallback(ResizeCallback callback) { onResize_ = std::move(callback); }

void Win32Window::SetDropCallback(DropCallback callback) { onDrop_ = std::move(callback); }

LRESULT CALLBACK Win32Window::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  Win32Window* window = nullptr;
  if (msg == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    window = reinterpret_cast<Win32Window*>(create->lpCreateParams);
    if (window != nullptr) {
      window->hwnd_ = hwnd;
    }
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
  } else {
    window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (window != nullptr) {
    return window->WndProc(hwnd, msg, wParam, lParam);
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  const bool imguiHandled = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam) != 0;

  switch (msg) {
    case WM_CLOSE:
      running_ = false;
      PostQuitMessage(0);
      return 0;
    case WM_SIZE: {
      width_ = LOWORD(lParam);
      height_ = HIWORD(lParam);
      minimized_ = (wParam == SIZE_MINIMIZED);
      if (!minimized_ && onResize_) {
        onResize_(width_, height_);
      }
      return 0;
    }
    case WM_DROPFILES: {
      HDROP drop = reinterpret_cast<HDROP>(wParam);
      if (onDrop_ && DragQueryFileW(drop, kDragQueryFileCount, nullptr, 0) > 0) {
        const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
        std::wstring path(length + 1u, L'\0');
        DragQueryFileW(drop, 0, path.data(), length + 1u);
        path.resize(length);
        onDrop_(fs::path(path));
      }
      DragFinish(drop);
      return 0;
    }
    case WM_MOUSEMOVE: {
      const int32_t x = GET_X_LPARAM(lParam);
      const int32_t y = GET_Y_LPARAM(lParam);
      input_.mouseDeltaX += static_cast<float>(x - input_.mouseX);
      input_.mouseDeltaY += static_cast<float>(y - input_.mouseY);
      input_.mouseX = x;
      input_.mouseY = y;
      input_.mouseInside = true;
      return 0;
    }
    case WM_MOUSELEAVE:
      input_.mouseInside = false;
      return 0;
    case WM_MOUSEWHEEL:
      input_.wheelDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
      return 0;
    case WM_LBUTTONDOWN:
      input_.mouseButtonsDown[0] = true;
      input_.mouseButtonsPressed[0] = true;
      SetCapture(hwnd);
      return 0;
    case WM_LBUTTONUP:
      input_.mouseButtonsDown[0] = false;
      input_.mouseButtonsReleased[0] = true;
      ReleaseCapture();
      return 0;
    case WM_RBUTTONDOWN:
      input_.mouseButtonsDown[1] = true;
      input_.mouseButtonsPressed[1] = true;
      SetCapture(hwnd);
      return 0;
    case WM_RBUTTONUP:
      input_.mouseButtonsDown[1] = false;
      input_.mouseButtonsReleased[1] = true;
      ReleaseCapture();
      return 0;
    case WM_MBUTTONDOWN:
      input_.mouseButtonsDown[2] = true;
      input_.mouseButtonsPressed[2] = true;
      SetCapture(hwnd);
      return 0;
    case WM_MBUTTONUP:
      input_.mouseButtonsDown[2] = false;
      input_.mouseButtonsReleased[2] = true;
      ReleaseCapture();
      return 0;
    case WM_LBUTTONDBLCLK:
      input_.mouseDoubleClickLeft = true;
      return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      if (wParam < input_.keysDown.size()) {
        input_.keysDown[wParam] = true;
      }
      return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      if (wParam < input_.keysDown.size()) {
        input_.keysDown[wParam] = false;
      }
      return 0;
    default:
      break;
  }
  if (imguiHandled) {
    return 1;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace directxsplat
