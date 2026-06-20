#pragma once

#include <array>
#include <cstdint>

namespace dxsplat {

struct InputState {
  std::array<bool, 256> keysDown{};
  std::array<bool, 5> mouseButtonsDown{};
  std::array<bool, 5> mouseButtonsPressed{};
  std::array<bool, 5> mouseButtonsReleased{};
  float mouseDeltaX = 0.0f;
  float mouseDeltaY = 0.0f;
  float wheelDelta = 0.0f;
  int32_t mouseX = 0;
  int32_t mouseY = 0;
  bool mouseInside = false;
  bool mouseDoubleClickLeft = false;

  void BeginFrame() {
    mouseDeltaX = 0.0f;
    mouseDeltaY = 0.0f;
    wheelDelta = 0.0f;
    mouseButtonsPressed.fill(false);
    mouseButtonsReleased.fill(false);
    mouseDoubleClickLeft = false;
  }

  bool KeyDown(uint32_t vk) const { return vk < keysDown.size() ? keysDown[vk] : false; }
};

}  // namespace dxsplat