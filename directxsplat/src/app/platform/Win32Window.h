#pragma once

#include <Windows.h>

#include <filesystem>
#include <functional>
#include <string>

#include "platform/InputState.h"

namespace dxsplat {

class Win32Window {
 public:
  using ResizeCallback = std::function<void(uint32_t, uint32_t)>;
  using DropCallback = std::function<void(const std::filesystem::path&)>;

  Win32Window();
  ~Win32Window();

  bool Create(const std::wstring& title, uint32_t width, uint32_t height);
  void Destroy();

  bool PumpMessages();
  void RequestClose();
  void SetFullscreen(bool fullscreen);
  bool IsFullscreen() const;
  HWND Hwnd() const;
  DWORD LastCreateError() const;

  uint32_t Width() const;
  uint32_t Height() const;
  bool IsMinimized() const;

  InputState& Input();
  const InputState& Input() const;

  void BeginFrameInput();
  void SetResizeCallback(ResizeCallback callback);
  void SetDropCallback(DropCallback callback);

 private:
  static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  HWND hwnd_ = nullptr;
  uint32_t width_ = 1280;
  uint32_t height_ = 720;
  bool minimized_ = false;
  bool running_ = true;
  bool fullscreen_ = false;
  DWORD windowedStyle_ = 0;
  RECT windowedRect_{};
  DWORD lastCreateError_ = 0;
  InputState input_{};
  ResizeCallback onResize_;
  DropCallback onDrop_;
};

}  // namespace dxsplat
