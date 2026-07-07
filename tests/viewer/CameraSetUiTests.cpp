#include <doctest/doctest.h>

#include "ui/UiLayer.h"

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
