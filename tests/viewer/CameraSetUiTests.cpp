#include <doctest/doctest.h>

#include <array>
#include <string>

#include "ui/UiLayer.h"

TEST_CASE("Camera UI labels are exact") {
  const std::array<const char*, 3> labels = directxsplat::UiCameraLabels();

  CHECK(std::string(labels[0]) == "Show camera frames");
  CHECK(std::string(labels[1]) == "frame size");
  CHECK(std::string(labels[2]) == "index");
}

TEST_CASE("Animation UI labels are exact") {
  const std::array<const char*, 2> labels = directxsplat::UiAnimationLabels();

  CHECK(std::string(labels[0]) == "Animation");
  CHECK(std::string(labels[1]) == "fps");
}

TEST_CASE("index clamps when camera set shrinks") {
  directxsplat::CameraUiState state{};
  state.index = 5;

  directxsplat::ClampCameraUiState(state, 3);
  CHECK(state.index == 2);

  directxsplat::ClampCameraUiState(state, 1);
  CHECK(state.index == 0);

  state.index = 4;
  directxsplat::ClampCameraUiState(state, 0);
  CHECK(state.index == 0);
}
